// render_texture.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/type.h"
#include "../mod/mod.h"

namespace rnd
{
	size_t numTextures = 0;

	// TEXTURE
	TextureGL*	CreateTexture(const char* filePath);

	// TEXTURE FILE
	const char*	LoadTextureFileFail(FILE* file, unsigned char* tempImage,
				Texture::frame* frames, const char* err);
	uint32_t	NextMipDim(uint32_t dim);
}

/*
################################################################################################


	TEXTURE


################################################################################################
*/

const char* const rnd::Texture::METATABLE = "metaTexture";

/*--------------------------------------
	rnd::Texture::Texture
--------------------------------------*/
rnd::Texture::Texture(const char* fileName, const uint32_t (&d)[2], uint32_t numFrames,
	frame* frames, uint16_t flags)
	: fileName(com::NewStringCopy(fileName)), numFrames(numFrames), frames(frames), flags(flags)
{
	dims[0] = d[0];
	dims[1] = d[1];
	shortName = com::StrRPBrk(this->fileName, "/\\");

	if(shortName)
		shortName++;
	else
		shortName = this->fileName;

	EnsureLink();
}

/*--------------------------------------
	rnd::Texture::~Texture
--------------------------------------*/
rnd::Texture::~Texture()
{
	if(fileName)
		delete[] fileName;

	FreeTextureFrames(frames);
}

/*--------------------------------------
	rnd::Texture::SetMip

HACK: Assumes this is TextureGL, store a type int or do a virtual if that can't be guaranteed.
--------------------------------------*/
void rnd::Texture::SetMip(bool b)
{
	if((flags & MIP) == b)
		return;

	flags = com::FixedBits(flags, MIP, b);
	((TextureGL*)this)->ResetMipFilter();
}

/*--------------------------------------
	rnd::TextureGL::TextureGL
--------------------------------------*/
rnd::TextureGL::TextureGL(const char* fileName, const uint32_t (&dims)[2], uint32_t numMipmaps,
	uint32_t numFrames, frame* frames, GLubyte* image) : Texture(fileName, dims, numFrames,
	frames, MIP)
{
	numTextures++;

	// Initialize image list
	lastImg = 0;
	numImgs = 0;

	// Check for alpha texels
	uint32_t size = dims[0] * dims[1];

	for(uint32_t i = 0; i < size; i++)
	{
		if(!image[i])
		{
			flags |= ALPHA;
			break;
		}
	}

	// Send data to gl
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MipFilter());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMipmaps);
	uint32_t levelDims[2] = {dims[0], dims[1]};
	uint32_t levelOffset = 0;

	for(GLint level = 0; level <= numMipmaps; level++)
	{
		if(levelDims[0] < 4)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexImage2D(GL_TEXTURE_2D, level, GL_LUMINANCE8, levelDims[0], levelDims[1], 0, GL_RED,
			GL_UNSIGNED_BYTE, image + levelOffset);

		levelOffset += levelDims[0] * levelDims[1];
		levelDims[0] = NextMipDim(levelDims[0]);
		levelDims[1] = NextMipDim(levelDims[1]);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glBindTexture(GL_TEXTURE_2D, 0);
}

/*--------------------------------------
	rnd::TextureGL::~TextureGL
--------------------------------------*/
rnd::TextureGL::~TextureGL()
{
	numTextures--;
	glDeleteTextures(1, &texName);

	if(lastImg)
		WRP_FATALF("Texture %s deleted while bound to an image", fileName);
}

/*--------------------------------------
	rnd::TextureGL::MipFilter
--------------------------------------*/
GLint rnd::TextureGL::MipFilter() const
{
	return (mipMapping.Bool() && (flags & MIP)) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
}

/*--------------------------------------
	rnd::TextureGL::ResetMipFilter
--------------------------------------*/
void rnd::TextureGL::ResetMipFilter()
{
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MipFilter());
	glBindTexture(GL_TEXTURE_2D, 0);
}

/*--------------------------------------
	rnd::TextureGL::MinFilter
--------------------------------------*/
GLint rnd::TextureGL::MinFilter(bool linear) const
{
	if(mipMapping.Bool() && (flags & MIP))
	{
		if(mipFade.Float() != 0.0f)
			return linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
		else
			return linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
	}
	else
		return linear ? GL_LINEAR : GL_NEAREST;
}

/*--------------------------------------
	rnd::FindTexture
--------------------------------------*/
rnd::Texture* rnd::FindTexture(const char* fileName)
{
	for(com::linker<Texture>* it = Texture::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->FileName(), fileName))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	rnd::EnsureTexture

Either finds or loads the texture with the given file name.
--------------------------------------*/
rnd::Texture* rnd::EnsureTexture(const char* fileName)
{
	Texture* tex;
	return (tex = FindTexture(fileName)) ? tex : CreateTexture(fileName);
}

/*--------------------------------------
	rnd::NumTextures
--------------------------------------*/
size_t rnd::NumTextures()
{
	return numTextures;
}

/*--------------------------------------
	rnd::CreateTexture

Creates texture and loads fileName from the textures directory. Returns ptr to new texture on
success. Returns 0 on failure.
--------------------------------------*/
rnd::TextureGL* rnd::CreateTexture(const char* fileName)
{
	GLubyte* image;
	const char *err = 0, *path = mod::Path("textures/", fileName, err);

	// Load texture file data
	uint32_t dims[2], numMipmaps, numFrames;
	Texture::frame* frames;

	if(err || (err = LoadTextureFile(path, image, dims, numMipmaps, numFrames, frames, true)))
	{
		con::LogF("Failed to load texture '%s' (%s)", fileName, err);
		return 0;
	}

	TextureGL* tex = new TextureGL(fileName, dims, numMipmaps, numFrames, frames, image);
	FreeTextureImage(image);
	return tex;
}

/*
################################################################################################


	TEXTURE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::FindTexture

IN	sFileName
OUT	texT
--------------------------------------*/
int rnd::FindTexture(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	if(Texture* t = FindTexture(fileName))
	{
		t->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	rnd::EnsureTexture

IN	sFileName
OUT	tex
--------------------------------------*/
int rnd::EnsureTexture(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	Texture* tex = EnsureTexture(fileName);

	if(!tex)
		luaL_error(l, "texture load failure");

	tex->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	rnd::TexFileName (FileName)

IN	texT
OUT	sFileName
--------------------------------------*/
int rnd::TexFileName(lua_State* l)
{
	Texture* t = Texture::CheckLuaTo(1);
	lua_pushstring(l, t->FileName());
	return 1;
}

/*--------------------------------------
LUA	rnd::TexShortName (ShortName)

IN	texT
OUT	sShortName
--------------------------------------*/
int rnd::TexShortName(lua_State* l)
{
	lua_pushstring(l, Texture::CheckLuaTo(1)->ShortName());
	return 1;
}

/*--------------------------------------
LUA	rnd::TexNumFrames (NumFrames)

IN	texT
OUT	iNumFrames
--------------------------------------*/
int rnd::TexNumFrames(lua_State* l)
{
	Texture* t = Texture::CheckLuaTo(1);
	lua_pushinteger(l, t->NumFrames());
	return 1;
}

/*--------------------------------------
LUA	rnd::TexFrameDims (FrameDims)

IN	texT, iFrame
OUT	[iFrameWidth, iFrameHeight]
--------------------------------------*/
int rnd::TexFrameDims(lua_State* l)
{
	Texture* t = Texture::CheckLuaTo(1);
	int frame = lua_tointeger(l, 2);

	if(frame >= 0 && frame < t->NumFrames())
	{
		const Texture::frame& f = t->Frames()[frame];
		lua_pushinteger(l, f.dims[0]);
		lua_pushinteger(l, f.dims[1]);
		return 2;
	}

	return 0;
}

/*--------------------------------------
LUA	rnd::TexMip (Mip)

IN	texT
OUT	bMip
--------------------------------------*/
int rnd::TexMip(lua_State* l)
{
	lua_pushboolean(l, Texture::CheckLuaTo(1)->Mip());
	return 1;
}

/*--------------------------------------
LUA	rnd::TexSetMip (SetMip)

IN	texT, bMip
--------------------------------------*/
int rnd::TexSetMip(lua_State* l)
{
	Texture::CheckLuaTo(1)->SetMip(lua_toboolean(l, 2));
	return 0;
}

/*--------------------------------------
LUA	rnd::TexDelete (__gc)

IN	texT
--------------------------------------*/
int rnd::TexDelete(lua_State* l)
{
	TextureGL* t = (TextureGL*)Texture::UserdataLuaTo(1);

	if(!t)
		return 0;

	if(t->Locked())
		luaL_error(l, "Tried to delete locked texture");

	delete t;
	return 0;
}

/*
################################################################################################


	TEXTURE FILE


################################################################################################
*/

/*--------------------------------------
	rnd::LoadTextureFile

Loads file filePath following the texture format. If the file loaded successfully, image and
subsequent arguments are modified, and 0 is returned. Otherwise, an error string is returned.

char SIG[4] = {0x69, 0x91, 'T', 'x'}
le uint32_t version = 0
le uint32_t sheetWidth
le uint32_t sheetHeight
le uint32_t numMipmaps
le uint32_t numFrames
unsigned char sheet[width * height]

mipWidth = width
mipHeight = height

mipmaps[numMipmaps]
	mipWidth = max(1, mipWidth / 2)
	mipHeight = max(1, mipHeight / 2)
	unsigned char mipmap[mipWidth * mipHeight]

frames[numFrames]
	le uint32_t dimX
	IF dimX != 0
		le uint32_t dimY
		le uint32_t topLeftX
		le uint32_t topLeftY
		le int32_t originX (relative to topLeft)
		le int32_t originY

FIXME: Ensure that bad files will not create undefined behavior or crash the game. Garbage
	textures is an acceptable outcome.
--------------------------------------*/
#define LOAD_TEXTURE_FILE_FAIL(err) return LoadTextureFileFail(file, tempImage, frames, err)

const char* rnd::LoadTextureFile(const char* filePath, GLubyte*& image, uint32_t (&dims)[2],
	uint32_t& numMipmaps, uint32_t& numFrames, Texture::frame*& frames, bool flip)
{
	unsigned char* tempImage = 0;
	frames = 0;
	size_t numRead;
	FILE* file = fopen(filePath, "rb");

	if(!file)
		LOAD_TEXTURE_FILE_FAIL("Could not open file");

	// Header
	const size_t HEADER_SIZE = 24;
	unsigned char header[HEADER_SIZE];

	if(fread(header, sizeof(unsigned char), HEADER_SIZE, file) != HEADER_SIZE)
		LOAD_TEXTURE_FILE_FAIL("Could not read header");

	if(strncmp((char*)header, "\x69\x91Tx", 4))
		LOAD_TEXTURE_FILE_FAIL("Incorrect signature");

	uint32_t version;
	com::MergeLE(header + 4, version);

	if(version != 0)
		LOAD_TEXTURE_FILE_FAIL("Unknown mesh file version");

	com::MergeLE(header + 8, dims[0]);
	com::MergeLE(header + 12, dims[1]);
	
	if(!dims[0] || !dims[1])
		LOAD_TEXTURE_FILE_FAIL("Sheet dimensions are 0");
	else if(dims[0] & dims[0] - 1 || dims[1] & dims[1] - 1)
		LOAD_TEXTURE_FILE_FAIL("Sheet dimensions are not powers of two");

	com::MergeLE(header + 16, numMipmaps);
	com::MergeLE(header + 20, numFrames);

	if(!numFrames)
		LOAD_TEXTURE_FILE_FAIL("numFrames is 0");

	// Add up image size
	size_t imageSize = (size_t)dims[0] * (size_t)dims[1];
	uint32_t mipDims[2] = {dims[0], dims[1]};

	for(uint32_t i = 0; i < numMipmaps; i++)
	{
		if(mipDims[0] == 1 && mipDims[1] == 1)
			LOAD_TEXTURE_FILE_FAIL("Too many mipmaps");

		mipDims[0] = NextMipDim(mipDims[0]);
		mipDims[1] = NextMipDim(mipDims[1]);
		imageSize += (size_t)mipDims[0] * (size_t)mipDims[1];
	}

	// Load image
	tempImage = new unsigned char[imageSize];
	numRead = fread(tempImage, sizeof(unsigned char), imageSize, file);

	if(numRead != imageSize)
		LOAD_TEXTURE_FILE_FAIL("Could not read image");

	if(flip)
	{
		uint32_t mipOffset = 0;
		uint32_t curDims[2] = {dims[0], dims[1]};

		for(uint32_t mip = 0; mip <= numMipmaps; mip++)
		{
			for(size_t y = 0; y < curDims[1] / 2; y++)
			{
				size_t f = curDims[1] - y - 1;
				size_t yd = mipOffset + y * curDims[0];
				size_t fd = mipOffset + f * curDims[0];

				for(size_t x = 0; x < curDims[0]; x++)
					com::Swap(tempImage[yd + x], tempImage[fd + x]);
			}

			mipOffset += curDims[0] * curDims[1];
			curDims[0] = NextMipDim(curDims[0]);
			curDims[1] = NextMipDim(curDims[1]);
		}
	}

	// Load frames
	frames = new Texture::frame[numFrames];
	uint32_t defFrame = -1; // Index of first defined frame (non-zero dimensions)

	for(uint32_t i = 0; i < numFrames; i++)
	{
		const size_t UNDEFINED_SIZE = 4;
		const size_t DEFINED_SIZE = 20;
		const size_t FRAME_SIZE = UNDEFINED_SIZE + DEFINED_SIZE;
		Texture::frame& f = frames[i];
		unsigned char frameBytes[FRAME_SIZE];
		numRead = fread(frameBytes, sizeof(unsigned char), UNDEFINED_SIZE, file);

		if(numRead != UNDEFINED_SIZE)
			LOAD_TEXTURE_FILE_FAIL("Could not read frame width");

		com::MergeLE(frameBytes, f.dims[0]);

		if(!f.dims[0])
		{
			// Undefined frame; copy first defined loaded frame if it exists
			if(defFrame != -1)
				f = frames[defFrame];

			continue;
		}

		numRead = fread(frameBytes + UNDEFINED_SIZE, sizeof(unsigned char), DEFINED_SIZE, file);

		if(numRead != DEFINED_SIZE)
			LOAD_TEXTURE_FILE_FAIL("Could not read rest of frame");

		com::MergeLE(frameBytes + 4, f.dims[1]);
		com::MergeLE(frameBytes + 8, f.corner[0]);
		com::MergeLE(frameBytes + 12, f.corner[1]);
		com::MergeLE(frameBytes + 16, f.origin[0]);
		com::MergeLE(frameBytes + 20, f.origin[1]);

		if(f.corner[0] >= dims[0] || f.corner[1] >= dims[1])
			LOAD_TEXTURE_FILE_FAIL("Frame top left position is outside sheet");

		if(!f.dims[0] || !f.dims[1])
			LOAD_TEXTURE_FILE_FAIL("Frame dimensions are 0");

		if(defFrame == -1)
			defFrame = i;
	}

	if(defFrame == -1)
		LOAD_TEXTURE_FILE_FAIL("All frames are undefined");

	// Copy first defined frame to undefined frames before it
	for(uint32_t i = 0; i < defFrame; i++)
		frames[i] = frames[defFrame];

	// Done
	fclose(file);

	if(COM_EQUAL_TYPES(GLubyte, unsigned char))
		image = tempImage;
	else
	{
		image = new GLubyte[imageSize];

		for(size_t i = 0; i < imageSize; i++)
			image[i] = (GLubyte)tempImage[i];

		delete[] tempImage;
	}

	return 0;
}

/*--------------------------------------
	rnd::LoadTextureFileFail
--------------------------------------*/
const char* rnd::LoadTextureFileFail(FILE* file, unsigned char* tempImage,
	Texture::frame* frames, const char* err)
{
	if(file)
		fclose(file);

	if(tempImage)
		delete[] tempImage;

	if(frames)
		delete[] frames;

	return err;
}

/*--------------------------------------
	rnd::FreeTextureImage
--------------------------------------*/
void rnd::FreeTextureImage(GLubyte* image)
{
	if(image)
		delete[] image;
}

/*--------------------------------------
	rnd::FreeTextureFrames
--------------------------------------*/
void rnd::FreeTextureFrames(Texture::frame* frames)
{
	if(frames)
		delete[] frames;
}

/*--------------------------------------
	rnd::NextMipDim
--------------------------------------*/
uint32_t rnd::NextMipDim(uint32_t dim)
{
	return COM_MAX(1, dim / 2);
}