// render_image_pass.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"
#include "../console/console.h"
#include "../wrap/wrap.h"

#define IMAGE_PASS_ATTRIB_POS		0
#define IMAGE_PASS_ATTRIB_TEXCOORD	1
#define IMAGE_PASS_ATTRIB_SWAP		2

namespace rnd
{
	void ImagePassCreateImageVertices();
	void ImagePassUploadVerticesAndElements();
	void ImagePassDrawImages();
	void ImagePassState();
	void ImagePassCleanup();
}

static struct
{
	GLuint			vShader, fShader, program;
	GLint			samImage, samPalette, samSubPalettes;
	GLuint			vertBuffer, elemBuffer;
	GLuint			vertBufferSize, elemBufferSize; // Buffer byte size
	GLuint			numVerts;
	GLuint			numOldVerts; // From previous frame
	GLuint			numElems;
	GLuint*			textureElemNums; // Per texture
	rnd::vertex_g*	verts;
	GLuint*			elems;
} imagePass = {0};

/*--------------------------------------
	rnd::ImagePassCreateImageVertices

Calculates vertices of each registered image. Screen pass vertex and element counts are set,
including the textureElemNums array.

TODO:
Add culling
Fix jitter (not sure if this should be done here or in shader)
Add optimization (prevent recreating unmoved vertices)

FIXME: Refactor
--------------------------------------*/
void rnd::ImagePassCreateImageVertices()
{
	const PaletteGL& pal = *CurrentPaletteGL();
	const float scaleWidth = 2.0f/(float)wrp::VideoWidth();
	const float scaleHeight = 2.0f/(float)wrp::VideoHeight();
	unsigned int texIndex = 0;
	imagePass.numVerts = 0;
	imagePass.numElems = 0;

	// Iterate through texture list
	for(com::linker<Texture>* texIt = Texture::List().l; texIt; texIt = texIt->prev, texIndex++)
	{
		TextureGL& tex = *(TextureGL*)texIt->o;
		imagePass.textureElemNums[texIndex] = 0;

		// Iterate through texture's registered images
		for(img_reg* imgIt = tex.lastImg; imgIt != 0; imgIt = imgIt->prev)
		{
			const gui::iImage& img = *imgIt->img;

			// Do not draw invisible
			if(!img.visible)
				continue;

			// Clamp frame num
			unsigned frame = COM_MIN(COM_MAX(img.frame, 0), tex.NumFrames() - 1);

			// Get image frame vars, cast float
			const Texture::frame& f = tex.Frames()[frame];
			const float &ix = img.pos.x, &iy = img.pos.y;			// Image pos

				// Image z, user to clip
			const float iz = preciseClipRange.Bool() ? img.pos.z : (img.pos.z * 2.0f - 1.0f);

			const float &s = img.scale;								// Image scale
			const float tx = tex.Dims()[0], ty = tex.Dims()[1];		// Texture dimensions
			const float ox = f.origin[0], oy = f.origin[1];			// Frame origin
			const float cx = f.corner[0], cy = ty - f.corner[1];	// Frame top-left corner
			const float dx = f.dims[0], dy = f.dims[1];				// Frame dimensions

			float left = (ix - ox * s) * scaleWidth - 1.0f;
			float right = (ix + (dx - ox) * s) * scaleWidth - 1.0f;
			float bottom = (iy - (dy - oy) * s) * scaleHeight - 1.0f;
			float top = (iy + oy * s) * scaleHeight - 1.0f;

			float tcLeft = cx / tx;
			float tcRight = (cx + dx) / tx;
			float tcBottom = (cy - dy) / ty;
			float tcTop = cy / ty;

			// Vertex positions and texcoords
			rnd::vertex_g* verts = imgIt->verts;
			
				// Bottom left
			verts[0].pos[0] = left;
			verts[0].pos[1] = bottom;
			verts[0].texCoord[0] = tcLeft;
			verts[0].texCoord[1] = tcBottom;

				// Bottom right
			verts[1].pos[0] = right;
			verts[1].pos[1] = bottom;
			verts[1].texCoord[0] = tcRight;
			verts[1].texCoord[1] = tcBottom;

				// Top right
			verts[2].pos[0] = right;
			verts[2].pos[1] = top;
			verts[2].texCoord[0] = tcRight;
			verts[2].texCoord[1] = tcTop;

				// Top left
			verts[3].pos[0] = left;
			verts[3].pos[1] = top;
			verts[3].texCoord[0] = tcLeft;
			verts[3].texCoord[1] = tcTop;

			verts[0].pos[2] = verts[1].pos[2] = verts[2].pos[2] = verts[3].pos[2] = iz;

			// Quad's subpalette
			imgIt->verts[0].subPalette = imgIt->verts[1].subPalette =
				imgIt->verts[2].subPalette = imgIt->verts[3].subPalette =
				(float)imgIt->img->subPalette * pal.subCoordScale;

			// Count
			imagePass.numVerts += 4;
			imagePass.textureElemNums[texIndex] += 6;
		}

		imagePass.numElems += imagePass.textureElemNums[texIndex];
	}
}

/*--------------------------------------
	rnd::ImagePassUploadVerticesAndElements

Allocates new vertex and element arrays if vertex count has increased beyond their sizes and
uploads data to GL_ARRAY_BUFFER and GL_ELEMENT_ARRAY_BUFFER.
--------------------------------------*/
void rnd::ImagePassUploadVerticesAndElements()
{
	if(!imagePass.numVerts)
		return;

	// Allocate vertex and element arrays
	// Reallocate if rendering more vertices than before
	if(imagePass.numVerts > imagePass.numOldVerts)
	{
		if(imagePass.verts)
			delete[] imagePass.verts;

		if(imagePass.elems)
			delete[] imagePass.elems;

		imagePass.verts = new vertex_g[imagePass.numVerts];
		imagePass.elems = new GLuint[imagePass.numElems];
	}

	// Fill arrays with image vertices and calculate elements
	GLuint vertIndex = 0;
	GLuint elemIndex = 0;

	for(com::linker<Texture>* texIt = Texture::List().l; texIt; texIt = texIt->prev)
	{
		TextureGL& tex = *(TextureGL*)texIt->o;

		for(img_reg* imgIt = tex.lastImg; imgIt != 0; imgIt = imgIt->prev)
		{
			if(!imgIt->img->visible)
				continue;

			imagePass.verts[vertIndex]		= imgIt->verts[0];
			imagePass.verts[vertIndex + 1]	= imgIt->verts[1];
			imagePass.verts[vertIndex + 2]	= imgIt->verts[2];
			imagePass.verts[vertIndex + 3]	= imgIt->verts[3];

			imagePass.elems[elemIndex]		= vertIndex;
			imagePass.elems[elemIndex + 1]	= vertIndex + 1;
			imagePass.elems[elemIndex + 2]	= vertIndex + 2;
			imagePass.elems[elemIndex + 3]	= vertIndex;
			imagePass.elems[elemIndex + 4]	= vertIndex + 2;
			imagePass.elems[elemIndex + 5]	= vertIndex + 3;

			vertIndex += 4;
			elemIndex += 6;
		}
	}

	// Upload vertices and elements
	// Increase buffer size if arrays were allocated
	if(imagePass.vertBufferSize < sizeof(vertex_g) * imagePass.numVerts)
	{
		imagePass.vertBufferSize = sizeof(vertex_g) * imagePass.numVerts;
		imagePass.elemBufferSize = sizeof(GLuint) * imagePass.numElems;

		glBufferData(GL_ARRAY_BUFFER, imagePass.vertBufferSize, imagePass.verts,
			GL_DYNAMIC_DRAW);

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, imagePass.elemBufferSize, imagePass.elems,
			GL_DYNAMIC_DRAW);
	}
	else
	{
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex_g) * imagePass.numVerts,
			imagePass.verts);

		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(GLuint) * imagePass.numElems,
			imagePass.elems);
	}
}

/*--------------------------------------
	rnd::ImagePassDrawImages

Iterates through the texture list and draws the stored count of elements.
--------------------------------------*/
void rnd::ImagePassDrawImages()
{
	if(!imagePass.numVerts)
		return;

	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	GLuint texIndex = 0, elemOffset = 0;

	for(com::linker<Texture>* texIt = Texture::List().l; texIt; texIt = texIt->prev)
	{
		glBindTexture(GL_TEXTURE_2D, ((TextureGL*)texIt->o)->texName);
		glDrawElements(GL_TRIANGLES, imagePass.textureElemNums[texIndex], GL_UNSIGNED_INT,
			(void*)elemOffset);

		elemOffset += imagePass.textureElemNums[texIndex] * sizeof(GLuint);
		texIndex++;
	}
}

/*--------------------------------------
	rnd::ImagePassState

Sets state for image pass. All state set here is guaranteed to be constant until the end of the
image pass.

FIXME: do reverse depth [1, 0] like the rest of drawing
--------------------------------------*/
void rnd::ImagePassState()
{
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);

	glDisable(GL_CULL_FACE); // FIXME: make billboards counter-clockwise

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glUseProgram(imagePass.program);

	glBindBuffer(GL_ARRAY_BUFFER, imagePass.vertBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imagePass.elemBuffer);

	imagePass.textureElemNums = new GLuint[NumTextures()];

	glVertexAttribPointer(IMAGE_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_g),
		(void*)0);
	glVertexAttribPointer(IMAGE_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex_g), (void*)(sizeof(GLfloat) * 3));
	glVertexAttribPointer(IMAGE_PASS_ATTRIB_SWAP, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_g),
		(void*)(sizeof(GLfloat) * 5));

	glEnableVertexAttribArray(IMAGE_PASS_ATTRIB_POS);
	glEnableVertexAttribArray(IMAGE_PASS_ATTRIB_TEXCOORD);
	glEnableVertexAttribArray(IMAGE_PASS_ATTRIB_SWAP);
}

/*--------------------------------------
	rnd::ImagePassCleanup
--------------------------------------*/
void rnd::ImagePassCleanup()
{
	delete[] imagePass.textureElemNums;

	glDisableVertexAttribArray(IMAGE_PASS_ATTRIB_POS);
	glDisableVertexAttribArray(IMAGE_PASS_ATTRIB_TEXCOORD);
	glDisableVertexAttribArray(IMAGE_PASS_ATTRIB_SWAP);
}

/*--------------------------------------
	rnd::ImagePass

Draws UI elements.

CLEARS:
DEPTH_BUFFER

OUTPUT:
* COLOR_BUFFER:	Final RGB output
--------------------------------------*/
void rnd::ImagePass()
{
	timers[TIMER_IMAGE_PASS].Start();

	ImagePassState();
	ImagePassCreateImageVertices();
	ImagePassUploadVerticesAndElements();
	ImagePassDrawImages();
	ImagePassCleanup();

	timers[TIMER_IMAGE_PASS].Stop();
}

/*--------------------------------------
	rnd::InitImagePass
--------------------------------------*/
bool rnd::InitImagePass()
{
	prog_attribute attributes[] =
	{
		{IMAGE_PASS_ATTRIB_POS, "pos"},
		{IMAGE_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{IMAGE_PASS_ATTRIB_SWAP, "swap"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&imagePass.samImage, "image"},
		{&imagePass.samPalette, "palette"},
		{&imagePass.samSubPalettes, "subPalettes"},
		{0, 0}
	};

	if(!InitProgram("image", imagePass.program, imagePass.vShader, &vertImageSource, 1,
	imagePass.fShader, &fragImageSource, 1, attributes, uniforms))
		return false;

	glUseProgram(imagePass.program); // RESET

	// Set constant uniforms
	glUniform1i(imagePass.samImage, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(imagePass.samPalette, RND_PALETTE_TEXTURE_NUM);
	glUniform1i(imagePass.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);

	// Buffers
	glGenBuffers(1, &imagePass.vertBuffer);
	glGenBuffers(1, &imagePass.elemBuffer);

	// Reset
	glUseProgram(0);

	return true;
}