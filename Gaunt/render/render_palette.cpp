// render_palette.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../mod/mod.h"

#define PALETTE_NUM_COLUMNS 256
#define PALETTE_SIZE PALETTE_NUM_COLUMNS * 3

namespace rnd
{
	struct ramp
	{
		uint16_t start, size;
		bool bright;
	};

	struct ramp_pos
	{
		uint16_t ramp, texel;
	};

	res::Ptr<PaletteGL> curPal = 0;

	uint32_t topInfluence = 0;
	GLfloat influenceFactor = 0.0f, unpackFactor = 0.0f;

	uint32_t topLightSub = 0;
	float packLightSubFactor = 0.0f;

	// PALETTE
	PaletteGL*	CreatePalette(const char* fileName);

	// PALETTE FILE
	const char*	LoadPaletteFile(const char* filePath, GLubyte*& paletteOut,
				GLubyte*& subPalettesOut, uint32_t& numSubPalettesOut, GLubyte*& rampsOut,
				uint16_t& numRampTexelsOut, GLubyte*& rampLookupOut,
				unsigned char& numFirstBrightsOut, uint32_t& maxRampSpanOut);
	void		FreePalette(GLubyte* palette, GLubyte* subPalettes, GLubyte* ramps,
				GLubyte* rampLookup);
}

/*
################################################################################################


	PALETTE


################################################################################################
*/

const char* const rnd::Palette::METATABLE = "metaPalette";

/*--------------------------------------
	rnd::Palette::Palette
--------------------------------------*/
rnd::Palette::Palette(const char* name, uint32_t numSubs, unsigned char numFirsts,
	uint32_t maxRampSpan)
	: fileName(com::NewStringCopy(name)), numSubPalettes(numSubs), numFirstBrights(numFirsts),
	maxRampSpan(maxRampSpan)
{
	EnsureLink();
}

/*--------------------------------------
	rnd::Palette::~Palette
--------------------------------------*/
rnd::Palette::~Palette()
{
	if(fileName)
		delete[] fileName;
}

/*--------------------------------------
	rnd::PaletteGL::PaletteGL
--------------------------------------*/
rnd::PaletteGL::PaletteGL(const char* name, uint32_t numSubs, unsigned char numFirsts,
	uint32_t maxRampSpan, const GLubyte* pal, const GLubyte* subs, const GLubyte* ramps,
	uint16_t numRampTexels, const GLubyte* rampLookup)
	: Palette(name, numSubs, numFirsts, maxRampSpan)
{
	rampDistScale = 255.0f / numRampTexels;
	invNumRampTexels = 1.0f / numRampTexels;
	uint32_t numSubPalettesUp = com::NextPow2(numSubPalettes); // Force power-of-two dimensions
	subCoordScale = numSubPalettesUp ? 1.0f / numSubPalettesUp : 0.0f;

	// Gen gl textures
	glGenTextures(1, &texPalette);
	glGenTextures(1, &texSubPalettes);
	glGenTextures(1, &texRamps);
	glGenTextures(1, &texRampLookup);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);

	// Palette texture
	glBindTexture(GL_TEXTURE_1D, texPalette);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, PALETTE_NUM_COLUMNS, 0, GL_RGB, GL_UNSIGNED_BYTE,
		pal);

	// Sub-palette texture
	glBindTexture(GL_TEXTURE_2D, texSubPalettes);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, PALETTE_NUM_COLUMNS, numSubPalettesUp, 0,
		GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PALETTE_NUM_COLUMNS, numSubPalettes, GL_LUMINANCE,
		GL_UNSIGNED_BYTE, subs);

	// Ramp texture
	glBindTexture(GL_TEXTURE_1D, texRamps);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, numRampTexels, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		ramps);

	// Ramp lookup texture
	glBindTexture(GL_TEXTURE_1D, texRampLookup);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, rampLookup);
}

/*--------------------------------------
	rnd::PaletteGL::~PaletteGL
--------------------------------------*/
rnd::PaletteGL::~PaletteGL()
{
	glDeleteTextures(1, &texPalette);
	glDeleteTextures(1, &texSubPalettes);
}

/*--------------------------------------
	rnd::FindPalette
--------------------------------------*/
rnd::Palette* rnd::FindPalette(const char* fileName)
{
	for(com::linker<Palette>* it = Palette::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->FileName(), fileName))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	rnd::EnsurePalette

Either finds or loads the palette with the given file name.
--------------------------------------*/
rnd::Palette* rnd::EnsurePalette(const char* fileName)
{
	Palette* pal;
	return (pal = FindPalette(fileName)) ? pal : CreatePalette(fileName);
}

/*--------------------------------------
	rnd::CurrentPalette
--------------------------------------*/
rnd::Palette* rnd::CurrentPalette()
{
	return curPal;
}

/*--------------------------------------
	rnd::SetPalette
--------------------------------------*/
void rnd::SetPalette(Palette& pb, bool forceReset)
{
	if(forceReset == false && curPal.Value() == &pb)
		return;

	PaletteGL& p = (PaletteGL&)pb;
	curPal.Set(&p);
	glActiveTexture(RND_PALETTE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_1D, p.texPalette);
	glActiveTexture(RND_SUB_PALETTES_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, p.texSubPalettes);
	glActiveTexture(RND_RAMP_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_1D, p.texRamps);
	glActiveTexture(RND_RAMP_LOOKUP_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_1D, p.texRampLookup);

	// Set up colored light packing
	// The number of light sub-palettes is clamped to allow enough bits for the influence value
	// numColorBands is effectively rounded up to a power of two
	uint32_t numBands, bufferMask;

	if(light16.Bool())
		bufferMask = 65535, numBands = (uint32_t)numColorBands.Float();
	else
		bufferMask = 255, numBands = (uint32_t)numColorBandsLow.Float();

	uint32_t influenceMask = numBands - 1;
	influenceMask |= influenceMask >> 1;
	influenceMask |= influenceMask >> 2;
	influenceMask |= influenceMask >> 4;
	influenceMask |= influenceMask >> 8;
	influenceMask |= influenceMask >> 16;
	uint32_t lightSubMask = (~influenceMask) & bufferMask;

	if(lightSubMask)
	{
		while((lightSubMask & 1) == 0)
			lightSubMask >>= 1;
	}

	topInfluence = influenceMask & bufferMask;
	influenceMask = ~lightSubMask & bufferMask; // Shift far left
	topLightSub = lightSubMask;

	if(influenceMask)
	{
		// Packed value of last discrete influence and first light-sub-palette
		// Multiplies influence floor([0, topInfluence]) to get packed influence
		influenceFactor = (float)influenceMask / float(bufferMask * topInfluence);
	}
	else // If there's only 1 discrete influence
		influenceFactor = 1.0f;

	// Packed value of first discrete influence and second light-sub-palette
	// Multiplies sub-palette [0, topLightSub] to get packed light sub
	packLightSubFactor = 1.0f / (float)bufferMask;

	if(lightSubMask)
		unpackFactor = influenceFactor * (float)bufferMask * p.subCoordScale;
	else // If there's only 1 lightSub
		unpackFactor = 0.0f;
}

/*--------------------------------------
	rnd::CreatePalette

Loads palette file and returns a new PaletteGL on load success.
--------------------------------------*/
rnd::PaletteGL* rnd::CreatePalette(const char* fileName)
{
	if(!fileName)
		return 0;

	GLubyte *paletteBytes, *subPalettesBytes, *rampBytes, *rampLookupBytes;
	uint32_t numSubPalettes, maxRampSpan;
	uint16_t numRampTexels, numFilterRampTexels;
	unsigned char numFirstBrights;
	const char *err = 0, *path = mod::Path("palettes/", fileName, err);

	if(err || (err = LoadPaletteFile(path, paletteBytes, subPalettesBytes, numSubPalettes,
	rampBytes, numRampTexels, rampLookupBytes, numFirstBrights, maxRampSpan)))
	{
		con::LogF("Failed to load palette '%s' (%s)", fileName, err);
		return 0;
	}

	PaletteGL* pal = new PaletteGL(fileName, numSubPalettes, numFirstBrights, maxRampSpan,
		paletteBytes, subPalettesBytes, rampBytes, numRampTexels, rampLookupBytes);

	FreePalette(paletteBytes, subPalettesBytes, rampBytes, rampLookupBytes);
	return pal;
}

/*--------------------------------------
	rnd::NormalizeSubPalette

Returns [0, 1] texture coordinate for the given sub-palette. 0 is guaranteed to return 0.0 so
shaders can do a "== 0.0" to check for the default sub-palette.
--------------------------------------*/
float rnd::NormalizeSubPalette(int subPalette)
{
	return subPalette ? (((float)subPalette + RND_SUB_PAL_BIAS) * curPal->subCoordScale) : 0.0f;
}

/*--------------------------------------
	rnd::PackedLightSubPalette

Returns sub-palette value for light buffer packing.
--------------------------------------*/
float rnd::PackedLightSubPalette(int subPalette)
{
	if(subPalette > topLightSub)
		subPalette = 0;

	return ((float)subPalette + RND_SUB_PAL_BIAS) * packLightSubFactor;
}

/*--------------------------------------
	rnd::CurrentPaletteGL
--------------------------------------*/
rnd::PaletteGL* rnd::CurrentPaletteGL()
{
	return (PaletteGL*)CurrentPalette();
}

/*
################################################################################################


	PALETTE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::FindPalette

IN	sFileName
OUT	palP
--------------------------------------*/
int rnd::FindPalette(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	if(Palette* p = FindPalette(fileName))
	{
		p->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	rnd::EnsurePalette

IN	sFileName
OUT	pal
--------------------------------------*/
int rnd::EnsurePalette(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	Palette* pal = EnsurePalette(fileName);

	if(!pal)
		luaL_error(l, "palette load failure");

	pal->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	rnd::PalFileName (FileName)

IN	palP
OUT	sFileName
--------------------------------------*/
int rnd::PalFileName(lua_State* l)
{
	Palette* p = Palette::CheckLuaTo(1);
	lua_pushstring(l, p->FileName());
	return 1;
}

/*--------------------------------------
LUA	rnd::PalNumSubPalettes (NumSubPalettes)

IN	palP
OUT	iNumSubPalettes
--------------------------------------*/
int rnd::PalNumSubPalettes(lua_State* l)
{
	Palette* p = Palette::CheckLuaTo(1);
	lua_pushinteger(l, p->NumSubPalettes());
	return 1;
}

/*--------------------------------------
LUA	rnd::PalDelete (__gc)

IN	palP
--------------------------------------*/
int rnd::PalDelete(lua_State* l)
{
	PaletteGL* p = (PaletteGL*)Palette::UserdataLuaTo(1);

	if(!p)
		return 0;

	if(p->Locked())
		luaL_error(l, "Tried to delete locked palette");

	delete p;
	return 0;
}

/*--------------------------------------
LUA	rnd::CurrentPalette

OUT	palCur
--------------------------------------*/
int rnd::CurrentPalette(lua_State* l)
{
	if(curPal)
	{
		curPal->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	rnd::SetPalette

IN	palP
--------------------------------------*/
int rnd::SetPalette(lua_State* l)
{
	Palette* p = Palette::CheckLuaTo(1);
	SetPalette(*p);
	return 0;
}

/*
################################################################################################


	PALETTE FILE


################################################################################################
*/

/*--------------------------------------
	rnd::LoadPaletteFile

Loads file fileName following the palette format. If the file loaded successfully, the out
parameters are set and 0 is returned. Otherwise, an error string is returned. Pointer outputs
are set to new arrays.

Ramps are used by the composition pass to shift colors and mimic different intensities of light
bouncing off that original diffuse color. They are also used by geometry-pass shaders to blend
the brightness of adjacent texels and produce the effect of filtered textures.

rampsOut is size numRampTexelsOut * 4 (RGB, color index) and rampLookupOut is size 1024.

rampLookupOut has 4 components:
	The target ramp's starting coordinate, hi component
	The target ramp's starting coordinate, lo component
	The target ramp's distance from start to end, up to 255 texels
	The input color's distance from start, up to 255 texels

File format:
char SIG[4] = {0x69, 0x91, 'P', 'l'}
le uint32_t version = 0
le uint32_t numSubPalettes
le uint16_t numRamps

paletteRGB[256]
	unsigned char r, g, b

subPalettes[numSubPalettes]
	unsigned char index[256]

rampMetas[numRamps]
	le uint16_t numRampTexels
	unsigned char bright

rampTexels[numRamps]
	unsigned char index[numRampTexels]

rampLookup[256] (converts a color index to a ramp and color's location on it)
	le uint16_t ramp, texel
--------------------------------------*/

// FIXME: make this call a function
#define LOAD_PALETTE_FILE_FAIL(err) { \
	if(file) fclose(file); \
	if(palette) delete[] palette; \
	if(subPalettes) delete[] subPalettes; \
	if(ramps) delete[] ramps; \
	if(rampLookup) delete[] rampLookup; \
	return err; \
}

const char* rnd::LoadPaletteFile(const char* filePath, GLubyte*& paletteOut,
	GLubyte*& subPalettesOut, uint32_t& numSubPalettesOut, GLubyte*& rampsOut,
	uint16_t& numRampTexelsOut, GLubyte*& rampLookupOut, unsigned char& numFirstBrightsOut,
	uint32_t& maxRampSpanOut)
{
	uint32_t version, numSubPalettes, maxRampSpan = 0;
	uint16_t numRamps;
	GLubyte *palette = 0, *subPalettes = 0, *ramps = 0, *rampLookup = 0;
	unsigned char numFirstBrights = 0;
	size_t numRead;
	FILE* file = fopen(filePath, "rb");

	if(!file)
		LOAD_PALETTE_FILE_FAIL("Could not open file");

	// Header
	unsigned char header[14];
	numRead = fread(header, sizeof(unsigned char), 14, file);

	if(numRead != 14)
		LOAD_PALETTE_FILE_FAIL("Could not read header");

	if(strncmp((char*)header, "\x69\x91Pl", 4))
		LOAD_PALETTE_FILE_FAIL("Incorrect signature");
	
	com::MergeLE(header + 4, version);

	if(version != 0)
		LOAD_PALETTE_FILE_FAIL("Unknown palette file version");

	com::MergeLE(header + 8, numSubPalettes);
	com::MergeLE(header + 12, numRamps);

	if(!numRamps || numRamps > 256)
		LOAD_PALETTE_FILE_FAIL("numRamps is not (0, 256]");

	numSubPalettes++; // For default 0-255 sub-palette

	// Palette
	unsigned char tempPalette[PALETTE_SIZE];

	numRead = fread(tempPalette, sizeof(unsigned char), PALETTE_SIZE, file);

	if(numRead != PALETTE_SIZE)
		LOAD_PALETTE_FILE_FAIL("Could not read palette colors");

	palette = new GLubyte[PALETTE_SIZE];

	for(size_t i = 0; i < PALETTE_SIZE; i++)
	{
		palette[i] = (GLubyte)tempPalette[i];

#if 0
		// FIXME TEMP
		// Round RGB to nearest multiple of 4 to add slight hue + luminance imperfection
		if((palette[i] & (1 | 2)) < 2)
			palette[i] &= ~(1 | 2);
		else if(palette[i] != 255)
			palette[i] = (palette[i] & ~(1 | 2)) + 4;
#endif
	}

	// Sub-palettes
	size_t subPalettesSize = PALETTE_NUM_COLUMNS * (size_t)numSubPalettes;
	subPalettes = new GLubyte[subPalettesSize];

	for(size_t i = 0; i < PALETTE_NUM_COLUMNS; i++)
		subPalettes[i] = (GLubyte)i;

	for(uint32_t i = 1; i < numSubPalettes; i++)
	{
		unsigned char tempSubPalette[PALETTE_NUM_COLUMNS];
		numRead = fread(tempSubPalette, sizeof(unsigned char), PALETTE_NUM_COLUMNS, file);

		if(numRead != PALETTE_NUM_COLUMNS)
			LOAD_PALETTE_FILE_FAIL("Could not read sub-palette indices");

		for(size_t j = 0; j < PALETTE_NUM_COLUMNS; j++)
			subPalettes[(size_t)i * PALETTE_NUM_COLUMNS + j] = (GLubyte)tempSubPalette[j];
	}

	// Ramp metas
	uint16_t numRampTexels = 0;
	uint16_t numFilterRampTexels = 0;
	ramp rampMetas[256]; // Max number of ramps allowed

	for(uint16_t i = 0; i < numRamps; i++)
	{
		ramp& rm = rampMetas[i];
		unsigned char metaBytes[3];

		if(fread(metaBytes, sizeof(unsigned char), 3, file) != 3)
			LOAD_PALETTE_FILE_FAIL("Could not read ramp's meta data");

		com::MergeLE(&metaBytes[0], rm.size);

		if(!rm.size || rm.size > 256)
			LOAD_PALETTE_FILE_FAIL("Ramp size must be [1, 256]");

		unsigned char brightChar = metaBytes[2];

		if(brightChar > 1)
			LOAD_PALETTE_FILE_FAIL("Ramp bright must be 0 or 1");

		rm.start = numRampTexels;
		numRampTexels += rm.size;
		maxRampSpan = COM_MAX(maxRampSpan, rm.size - 1);

		// Single-color ramps (i.e. fullbrights) are bright too for the sake of numFirstBrights
		rm.bright = brightChar != 0 || rm.size == 1;
	}

	// Light ramps
	uint16_t numRampTexelsUp = com::NextPow2(numRampTexels);
	size_t rampsSizeRGBI = (size_t)numRampTexelsUp * 4; // Allocate 4x for storing RGBI later
	ramps = new unsigned char[rampsSizeRGBI];
	memset(ramps, 0, rampsSizeRGBI);

	for(uint16_t i = 0, texel = 0; i < numRamps; i++)
	{
		ramp& rm = rampMetas[i];
		numRead = fread(ramps + texel, sizeof(unsigned char), rm.size, file);

		if(numRead != rm.size)
			LOAD_PALETTE_FILE_FAIL("Could not read ramp indices");

		texel += rm.size;
	}

	// Light ramp lookup table
	ramp_pos tempLookup[256];

	if(com::ReadLE((uint16_t*)tempLookup, 512, file) != sizeof(tempLookup))
		LOAD_PALETTE_FILE_FAIL("Could not read ramp lookup table");

	rampLookup = new unsigned char[1024];
	const float RAMP_BIAS = 0.5f; // .5 to allow filtering without ramps leaking into each other

	for(size_t i = 0; i < 256; i++)
	{
		size_t j = i * 4;
		ramp_pos& rp = tempLookup[i];

		if(rp.ramp >= numRamps)
			LOAD_PALETTE_FILE_FAIL("Out-of-bounds ramp index in ramp lookup table");

		ramp& rm = rampMetas[rp.ramp];

		if(rp.texel >= rm.size)
			LOAD_PALETTE_FILE_FAIL("Out-of-bounds texel index in ramp lookup table");

		uint16_t start = rm.start;
		uint16_t endDist = rm.size - 1;
		uint16_t colorDist = rp.texel;

		if(rm.bright)
		{
			// Act like ramp starts at this color so it can't get darker
			// Note, this means lights cannot brighten it either, since it's now 0% reflective
			start += colorDist;
			endDist -= colorDist;
			colorDist = 0;
		}

		float rf = ((float)start + RAMP_BIAS) / (float)numRampTexelsUp * 255.0f;
		rampLookup[j] = GLubyte(rf);
		rampLookup[j + 1] = GLubyte((rf - floor(rf)) * 255.0f);
		rampLookup[j + 2] = endDist;
		rampLookup[j + 3] = colorDist;

#if 0
		// Test inverse conversion
		float testRampDistScale = 255.0f / (float)numRampTexelsUp;
		float testStart = rampLookup[j] / 255.0f + rampLookup[j + 1] / 255.0f / 255.0f;
		int testStartI = testStart * numRampTexelsUp;
		float testEnd = testStart + rampLookup[j + 2] / 255.0f * testRampDistScale;
		int testEndI = testEnd * numRampTexelsUp;
		float testScale = 1.0f;
		float testColorCoord = COM_MIN(testStart + rampLookup[j + 3] / 255.0f * testScale * testRampDistScale, testEnd);
		int testColorCoordI = testColorCoord * numRampTexelsUp;

		if(testStartI != rm.start || testEndI != rm.start + rm.size - 1 ||
		testColorCoordI >= numRampTexelsUp || ramps[testColorCoordI] != i)
			int a = 0;
#endif
	}

	// Convert light ramps to RGBI, in-place
	for(size_t i = 0; i < numRampTexels; i++)
	{
		size_t t = numRampTexels - 1 - i;
		size_t c = t * 4;
		size_t index = ramps[t];
		size_t red = index * 3;
		ramps[c] = palette[red];
		ramps[c + 1] = palette[red + 1];
		ramps[c + 2] = palette[red + 2];
		ramps[c + 3] = index;
	}

	// Count numFirstBrights
	for(size_t i = 0; i < 256; i++)
	{
		if(rampMetas[tempLookup[i].ramp].bright)
			numFirstBrights++;
		else
			break;
	}

	// Done
	fclose(file);
	paletteOut = palette;
	subPalettesOut = subPalettes;
	numSubPalettesOut = numSubPalettes;
	rampsOut = ramps;
	numRampTexelsOut = numRampTexelsUp;
	rampLookupOut = rampLookup;
	numFirstBrightsOut = numFirstBrights;
	maxRampSpanOut = maxRampSpan;
	return 0;
}

/*--------------------------------------
	rnd::FreePalette
--------------------------------------*/
void rnd::FreePalette(GLubyte* palette, GLubyte* subPalettes, GLubyte* ramps,
	GLubyte* rampLookup)
{
	if(palette)
		delete[] palette;

	if(subPalettes)
		delete[] subPalettes;

	if(ramps)
		delete[] ramps;

	if(rampLookup)
		delete[] rampLookup;
}