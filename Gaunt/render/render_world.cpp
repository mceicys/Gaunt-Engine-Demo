// render_world.cpp -- World draw data and pass
// Martynas Ceicys

#include <string.h>
#include <stdlib.h>
#include <math.h> // abs
#include <float.h> // FLT_MAX

#include "render.h"
#include "render_private.h"
#include "../console/console.h"
#include "../../GauntCommon/obj.h"
#include "../../GauntCommon/tree.h"
#include "../hit/hit.h"
#include "../input/input.h"
#include "../vector/vec_lua.h"
#include "../wrap/wrap.h"

#define WORLD_PASS_ATTRIB_POS			0
#define WORLD_PASS_ATTRIB_TEXCOORD		1
#define WORLD_PASS_ATTRIB_NORM			2
#define WORLD_PASS_ATTRIB_SUB_PALETTE	3

namespace rnd
{
	GLuint		worldVertexBuffer;
	GLuint		worldElementBuffer;
	zone_reg*	zoneRegs;
	uint32_t	numZoneRegs;

	void		BindWorldBuffers();
	void		WorldDraw();
	void		WorldState();
	void		WorldCleanup();
}

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniWorldToClip, uniTexDims, uniSubPaletteFactor, uniDither, uniMipBias, uniMipFade,
			uniRampDistScale, uniInvNumRampTexels;
	GLint	samTexture, samSubPalettes, samRamps, samRampLookup;
} worldProg = {0};

/*--------------------------------------
	rnd::RegisterWorld

Uploads vertices to the world vertex buffer and registers zones. Zones are given zone_reg
objects.

FIXME: separate loading code and drawing code
--------------------------------------*/
void rnd::RegisterWorld(const scn::TriBatch* batches, uint32_t numBatches, scn::Zone* zones,
	scn::ZoneExt* zoneExts, uint32_t numZones)
{
	if(zoneRegs)
	{
		for(uint32_t i = 0; i < numZoneRegs; i++)
		{
			if(zoneRegs[i].drawBatches)
				delete[] zoneRegs[i].drawBatches;
		}

		delete[] zoneRegs;
		zoneRegs = 0;
	}

	numZoneRegs = 0;
	GLsizeiptr numVertices = 0;
	GLsizeiptr numTriangles = 0;

	for(uint32_t i = 0; i < numBatches; i++)
	{
		numVertices += batches[i].numVertices;
		numTriangles += batches[i].numTriangles;
	}

	if(!numTriangles)
		return;

	numZoneRegs = numZones;

	// Buffer vertex data
	vertex_world* verts = new vertex_world[numVertices];
	size_t vertIndex = 0;

	for(uint32_t i = 0; i < numBatches; i++)
	{
		const scn::TriBatch& batch = batches[i];

		for(uint32_t j = 0; j < batch.numVertices; j++)
		{
			for(size_t k = 0; k < 3; k++)
			{
				verts[vertIndex].pos[k] = batch.vertices[j].pos[k];
				verts[vertIndex].normal[k] = batch.vertices[j].normal[k];
			}

			for(size_t k = 0; k < 2; k++)
				verts[vertIndex].texCoord[k] = batch.vertices[j].texCoords[k];

			verts[vertIndex].subPalette = batch.vertices[j].subPalette + RND_SUB_PAL_BIAS;
			verts[vertIndex].ambient = com::Max(batch.vertices[j].ambient, 0.0f);
			verts[vertIndex].darkness = batch.vertices[j].darkness;

			vertIndex++;
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, worldVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_world) * numVertices, verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	delete[] verts;

	// Buffer element data
	GLsizeiptr numElements = numTriangles * 3;
	GLuint* elements = new GLuint[numElements];
	size_t elemIndex = 0;

	for(uint32_t i = 0; i < numBatches; i++)
	{
		const scn::TriBatch& batch = batches[i];

		for(uint32_t j = 0; j < batch.numTriangles * 3; j++)
			elements[elemIndex++] = batch.elements[j] + batch.firstGlobalVertex;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldElementBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numElements * sizeof(GLuint), elements,
		GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	delete[] elements;

	// Register zones
	zoneRegs = new zone_reg[numZoneRegs];
	GLint zoneFirstIndex = 0;

	for(uint32_t i = 0; i < numZones; i++)
	{
		scn::Zone& z = zones[i + 1];
		const scn::ZoneExt& ze = zoneExts[i + 1];
		zone_reg& zoneReg = zoneRegs[i];
		z.rndReg = &zoneReg;
		zoneReg.drawBatches = 0;
		zoneReg.numDrawBatches = 0;
		zoneReg.totalElements = 0;

		if(ze.numBatches)
		{
			zoneReg.numDrawBatches = ze.numBatches;
			zoneReg.drawBatches = new zone_reg::draw_batch[zoneReg.numDrawBatches];

			for(uint32_t j = 0; j < ze.numBatches; j++)
			{
				const scn::TriBatch& src = batches[ze.firstBatch + j];
				zone_reg::draw_batch& dest = zoneReg.drawBatches[j];
				dest.firstIndex = src.firstGlobalTriangle * 3;
				dest.byteOffset = dest.firstIndex * sizeof(GLuint);
				dest.numElements = src.numTriangles * 3;
				dest.tex = src.tex;
				zoneReg.totalElements += dest.numElements;
			}
		}
	}

	// Reset last sun direction so sun visibility is calculated
	prevSunDir = 0.0f;
}

/*--------------------------------------
	rnd::WorldDrawZone
--------------------------------------*/
void rnd::WorldDrawZone(const scn::Zone* z)
{
	if(!z->rndReg || !z->rndReg->numDrawBatches)
		return;

	glDrawElements(GL_TRIANGLES, z->rndReg->totalElements, GL_UNSIGNED_INT,
		(GLvoid*)z->rndReg->drawBatches[0].byteOffset);
}

/*--------------------------------------
	rnd::BindWorldBuffers
--------------------------------------*/
void rnd::BindWorldBuffers()
{
	glBindBuffer(GL_ARRAY_BUFFER, worldVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldElementBuffer);

	glVertexAttribPointer(WORLD_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_world),
		(void*)0);

	glVertexAttribPointer(WORLD_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex_world), (void*)(sizeof(GLfloat) * 3));

	glVertexAttribPointer(WORLD_PASS_ATTRIB_NORM, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_world),
		(void*)(sizeof(GLfloat) * 5));

	glVertexAttribPointer(WORLD_PASS_ATTRIB_SUB_PALETTE, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_world),
		(void*)(sizeof(GLfloat) * 8));
}

/*--------------------------------------
	rnd::WorldDraw
--------------------------------------*/
void rnd::WorldDraw()
{
	// FIXME: this check should be done before WorldState, but make sure it doesn't break other rendering by not setting some state
	if(!numVisZones)
		return;

	// FIXME: all this state setting should be done in WorldState and WorldCleanup
	glUseProgram(worldProg.shaderProgram);
	glUniformMatrix4fv(worldProg.uniWorldToClip, 1, GL_FALSE, gWorldToClip);
	PaletteGL* curPal = CurrentPaletteGL();
	glUniform1f(worldProg.uniSubPaletteFactor, curPal->subCoordScale);

	if(worldProg.uniMipBias != -1)
		glUniform1f(worldProg.uniMipBias, mipBias.Float());

	if(worldProg.uniMipFade != -1)
		glUniform1f(worldProg.uniMipFade, mipFade.Float());

	if(worldProg.uniRampDistScale != -1)
		glUniform1f(worldProg.uniRampDistScale, curPal->rampDistScale);

	if(worldProg.uniInvNumRampTexels != -1)
		glUniform1f(worldProg.uniInvNumRampTexels, curPal->invNumRampTexels);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_GEQUAL, 0, -1);

	for(size_t i = 0; i < numVisZones; i++)
		WorldDrawZoneTextured(worldProg, visZones[i]);

	glDisable(GL_STENCIL_TEST);
}

/*--------------------------------------
	rnd::WorldState

All state set here is constant until the world pass is done.
--------------------------------------*/
void rnd::WorldState()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glEnable(GL_CULL_FACE);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // FIXME: remove
	BindWorldBuffers();
	glEnableVertexAttribArray(WORLD_PASS_ATTRIB_POS);
	glEnableVertexAttribArray(WORLD_PASS_ATTRIB_TEXCOORD);
	glEnableVertexAttribArray(WORLD_PASS_ATTRIB_NORM);
	glEnableVertexAttribArray(WORLD_PASS_ATTRIB_SUB_PALETTE);
}

/*--------------------------------------
	rnd::WorldCleanup
--------------------------------------*/
void rnd::WorldCleanup()
{
	glDisableVertexAttribArray(WORLD_PASS_ATTRIB_POS);
	glDisableVertexAttribArray(WORLD_PASS_ATTRIB_TEXCOORD);
	glDisableVertexAttribArray(WORLD_PASS_ATTRIB_NORM);
	glDisableVertexAttribArray(WORLD_PASS_ATTRIB_SUB_PALETTE);
}

/*--------------------------------------
	rnd::WorldPass

Draws the static world.

OUTPUT:
* COLOR_BUFFER:	scene color indices (r) and normals (g, b)
* DEPTH_BUFFER:	scene depth
--------------------------------------*/
void rnd::WorldPass()
{
	timers[TIMER_WORLD_PASS].Start();

	WorldState();
	WorldDraw();
	WorldCleanup();

	int color = leafColor.Integer(), solidColor = solidLeafColor.Integer();

	if(color != -1 || solidColor != -1)
	{
		for(const scn::WorldNode* n = scn::WorldRoot(); n; n = com::TraverseTree(n))
		{
			if(n->hull)
			{
				if(n->solid)
				{
					if(solidColor != -1)
						n->hull->DrawWire(0.0f, com::QUA_IDENTITY, solidColor);
				}
				else
				{
					if(color != -1)
						n->hull->DrawWire(0.0f, com::QUA_IDENTITY, color);
				}
			}
		}
	}

	timers[TIMER_WORLD_PASS].Stop();
}

/*--------------------------------------
	rnd::InitWorldPass
--------------------------------------*/
bool rnd::InitWorldPass()
{
	// Buffers
	glGenBuffers(1, &worldVertexBuffer);
	glGenBuffers(1, &worldElementBuffer);

	return ReinitWorldProgram();
}

/*--------------------------------------
	rnd::ReinitWorldProgram
--------------------------------------*/
bool rnd::ReinitWorldProgram()
{
	DeleteWorldProgram();

	prog_attribute attributes[] =
	{
		{WORLD_PASS_ATTRIB_POS, "pos"},
		{WORLD_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{WORLD_PASS_ATTRIB_NORM, "norm"},
		{WORLD_PASS_ATTRIB_SUB_PALETTE, "subPalette"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&worldProg.uniWorldToClip, "worldToClip"},
		{&worldProg.uniTexDims, "texDims", true},
		{&worldProg.uniSubPaletteFactor, "subPaletteFactor"},
		{&worldProg.uniDither, "dither", true},
		{&worldProg.uniMipBias, "mipBias", true},
		{&worldProg.uniMipFade, "mipFade", true},
		{&worldProg.uniRampDistScale, "rampDistScale", true},
		{&worldProg.uniInvNumRampTexels, "invNumRampTexels", true},
		{&worldProg.samTexture, "texture"},
		{&worldProg.samSubPalettes, "subPalettes"},
		{&worldProg.samRampLookup, "rampLookup", true},
		{&worldProg.samRamps, "ramps", true},
		{0, 0}
	};
	
	const char* fragSrcs[] = {
		"#version 130", // FIXME: do 120 if all defines are 0 (except MIP_MAPPING)
		shaderDefineMipMapping,
		shaderDefineMipFade,
		shaderDefineFilterBrightness,
		shaderDefineFilterColor,
		"\n",
		fragWorldSource
	};

	if(!InitProgram("world", worldProg.shaderProgram, worldProg.vertexShader, &vertWorldSource,
	1, worldProg.fragmentShader, fragSrcs, sizeof(fragSrcs) / sizeof(char*), attributes,
	uniforms))
		goto fail;

	glUseProgram(worldProg.shaderProgram); // RESET

	// Constant uniforms
	GLfloat dither[16];
	MakeDitherArray(dither);
	glUniform1fv(worldProg.uniDither, 16, dither);
	glUniform1i(worldProg.samTexture, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(worldProg.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);
	glUniform1i(worldProg.samRamps, RND_RAMP_TEXTURE_NUM);
	glUniform1i(worldProg.samRampLookup, RND_RAMP_LOOKUP_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;

fail:
	con::LogF("Failed to initialize world shader program");

	// FIXME: Generalize low-setting check outside
	/* FIXME: Compilation failing on first try doesn't cleanly recover
		* Some HUD elements aren't hidden properly; is a Lua script being interrupted somehow?
		* A GL error is reported
	*/
	struct low_option
	{
		con::Option* opt;
		float val;
	};

	low_option lows[] = {
		{&mipMapping, (float)false},
		{&mipFade, 0.0f},
		{&filterTextureBrightness, (float)FILTER_TEXTURE_BRIGHTNESS_OFF},
		{&filterTextureColor, (float)false}
	};

	bool tryAgain = false;

	for(size_t i = 0; i < sizeof(lows)/sizeof(low_option); i++)
	{
		if(lows[i].opt->Float() != lows[i].val)
		{
			tryAgain = true;
			lows[i].opt->SetValue(lows[i].val);
		}
	}
	
	if(tryAgain)
	{
		con::LogF("Trying lowest settings for world program");
		return ReinitWorldProgram();
	}
	
	DeleteWorldProgram();
	return false;
}

/*--------------------------------------
	rnd::DeleteWorldProgram
--------------------------------------*/
void rnd::DeleteWorldProgram()
{
	if(worldProg.shaderProgram) glDeleteProgram(worldProg.shaderProgram);
	if(worldProg.vertexShader) glDeleteShader(worldProg.vertexShader);
	if(worldProg.fragmentShader) glDeleteShader(worldProg.fragmentShader);

	memset(&worldProg, 0, sizeof(worldProg));
}

/*--------------------------------------
	rnd::EnsureWorldProgram
--------------------------------------*/
void rnd::EnsureWorldProgram()
{
	if(!worldProg.shaderProgram)
	{
		if(!ReinitWorldProgram())
			WRP_FATAL("Could not compile world shader program");
	}
}