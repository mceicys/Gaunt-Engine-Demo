// render_sky.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"
#include "../mod/mod.h"

#define SKY_PASS_ATTRIB_POS 0

namespace rnd
{
	res::Ptr<const rnd::Texture> skySphereTex;

	void	SkyBoxState();
	void	SkyBoxCleanup();
	void	SkyBoxPass();
	bool	InitSkyBoxPass();
	void	SkySphereState();
	void	SkySphereToClip(GLfloat* toClipOut);
	void	SkySphereCleanup();
	void	SkySpherePass();
	bool	SkyOverlayPass();
	void	SkyDepthResetState();
	void	SkyDepthResetCleanup();
	void	SkyDepthResetPass();
	bool	InitSkyDepthResetPass();
};

static struct
{
	GLuint	texture;
	GLuint	vertexBuffer;
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniToClip;
	GLint	samTexture;
} skyBoxPass = {0};

static struct
{
	GLuint							vertexShader, fragmentShader, shaderProgram;
	GLint							uniToClip, uniTexDims, uniDither, uniMipBias, uniMipFade,
									uniRampDistScale, uniInvNumRampTexels, uniSeamMipBias;
	GLint							samTexture, samRamps, samRampLookup;
} skySphereProg = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniIntensity, uniSubPalette;
} skyLightPass = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
} skyDepthResetPass = {0};

/*
################################################################################################


	SKY


################################################################################################
*/

/*--------------------------------------
	rnd::SetSkyFace
--------------------------------------*/
bool rnd::SetSkyFace(int face, const char* fileName)
{
	if(face < 0 || face > 5)
	{
		con::LogF("Invalid sky face index %d", face);
		return false;
	}

	const GLenum TARGETS[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z, // x+, front
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, // x-, back
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X, // y+, left
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, // y-, right
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, // z+, up
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y, // z-, down
	};

	GLubyte* image;
	uint32_t dims[2], numFrames, numMipmaps;
	Texture::frame* frames;
	const char *err = 0, *path = mod::Path("textures/", fileName, err);

	if(err || (err = LoadTextureFile(path, image, dims, numMipmaps, numFrames, frames, true)))
	{
		con::LogF("Failed to load sky texture '%s' (%s)", fileName, err);
		return false;
	}

	if(dims[0] != dims[1])
	{
		con::LogF("Sky texture '%s' is non-square", fileName);
		FreeTextureImage(image);
		FreeTextureFrames(frames);
		return false;
	}

	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyBoxPass.texture);
	glTexImage2D(TARGETS[face], 0, GL_LUMINANCE8, dims[0], dims[1], 0, GL_RED, GL_UNSIGNED_BYTE,
		image);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	FreeTextureImage(image);
	FreeTextureFrames(frames);
	return true;
}

/*--------------------------------------
	rnd::SkySphereTexture
--------------------------------------*/
const rnd::Texture* rnd::SkySphereTexture()
{
	return skySphereTex;
}

/*--------------------------------------
	rnd::SetSkySphereTexture

FIXME: needs to be saved and loaded; make part of scene system
--------------------------------------*/
void rnd::SetSkySphereTexture(const Texture* tex)
{
	skySphereTex.Set(tex);
}

/*--------------------------------------
	rnd::SkyPass

OUTPUT:
* COLOR_BUFFER: Scene color indices (r) and normals (g, b, a)

FIXME: Can save nearly 1ms without SkyOverlayPass and depth reset
	99% of the time we're just drawing a sky sphere instead of a skybox
	Make SkyBoxPass either draw a hard-coded box or sphere, selectable thru a function
--------------------------------------*/
void rnd::SkyPass()
{
	timers[TIMER_SKY_PASS].Start();

	if(skySphereTex.Value())
		SkySpherePass();
	else
		SkyBoxPass();

	if(SkyOverlayPass())
		SkyDepthResetPass();

	timers[TIMER_SKY_PASS].Stop();
}

/*--------------------------------------
	rnd::InitSkyPass
--------------------------------------*/
bool rnd::InitSkyPass()
{
	return InitSkyBoxPass() && ReinitSkySphereProgram() && InitSkyDepthResetPass();
}

/*
################################################################################################


	SKY BOX


################################################################################################
*/

/*--------------------------------------
	rnd::SkyBoxState
--------------------------------------*/
void rnd::SkyBoxState()
{
	glDepthFunc(GL_GEQUAL);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_GREATER, skyMask, -1);
	glUseProgram(skyBoxPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, skyBoxPass.vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyBoxPass.texture);
	glEnableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glVertexAttribPointer(SKY_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, 0);
	const scn::Overlay& skyOvr = scn::SkyOverlay();
	const scn::Camera* cam;
	const GLfloat* vtc;

	if(skyOvr.cam)
	{
		cam = skyOvr.cam;
		vtc = gSkyVTC;
	}
	else
	{
		cam = scn::ActiveCamera();
		vtc = gViewToClip;
	}

#if 0
	// Not rearranged; requires rotated textures
	GLfloat skyBoxToView[16];
	com::MatQua(cam->FinalOri().Conjugate(), skyBoxToView);
	GLfloat skyBoxToClip[16];
	com::Multiply4x4(skyBoxToView, vtc, skyBoxToClip);
#else
	// Make matrix so cube map textures don't need to be rotated (front is z+, left is x-, up is y-)

	// FIXME: understand this again (and in relation to shader) and make comments more detailed so it's intuitive

	/* Remap quaternion for different space
	Derived by swapping and reordering Euler angles:
		qx = {sin(pitch * 0.5), 0.0, 0.0, cos(pitch * 0.5)};
		qy = {0.0, sin(yaw * 0.5), 0.0, cos(yaw * 0.5)};
		qz = {0.0, 0.0, sin(-roll * 0.5), cos(-roll * 0.5)};
		ori = qz * qx * qy */
	com::Qua temp = cam->FinalOri();
	com::Qua ori = {temp[1], temp[2], -temp[0], temp[3]};
	GLfloat skyBoxToView[16];
	com::MatQua(ori, skyBoxToView);

	// Rearrange gViewToClip to undo mapping of world coords to GL coords and flip some things
	GLfloat viewToClip[16] = {
		-vtc[1],	vtc[2],		vtc[0],		vtc[3],
		-vtc[5],	-vtc[6],	vtc[4],		vtc[7],
		0.0f,		0.0f,		1.0f,		0.0f, // Shader ignores clip z and sets it to w for max depth
		-vtc[13],	vtc[14],	vtc[12],	vtc[15],
	};

	GLfloat skyBoxToClip[16];
	com::Multiply4x4(skyBoxToView, viewToClip, skyBoxToClip);
#endif

	glUniformMatrix4fv(skyBoxPass.uniToClip, 1, GL_FALSE, skyBoxToClip);
}

/*--------------------------------------
	rnd::SkyBoxCleanup
--------------------------------------*/
void rnd::SkyBoxCleanup()
{
	glDisableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glDisable(GL_STENCIL_TEST);
}

/*--------------------------------------
	rnd::SkyBoxPass
--------------------------------------*/
void rnd::SkyBoxPass()
{
	SkyBoxState();
	glDrawArrays(GL_QUADS, 0, 24); // FIXME: GL_QUADS is deprecated, driver might not like it
	SkyBoxCleanup();
}

/*--------------------------------------
	rnd::InitSkyBoxPass
--------------------------------------*/
bool rnd::InitSkyBoxPass()
{
	prog_attribute attributes[] =
	{
		{SKY_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&skyBoxPass.uniToClip, "toClip"},
		{&skyBoxPass.samTexture, "texture"},
		{0, 0}
	};

	if(!InitProgram("skybox", skyBoxPass.shaderProgram, skyBoxPass.vertexShader,
	&vertSkyBoxSource, 1, skyBoxPass.fragmentShader, &fragSkyBoxSource, 1, attributes,
	uniforms))
		return false;

	glUseProgram(skyBoxPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(skyBoxPass.samTexture, RND_VARIABLE_TEXTURE_NUM);

	// Buffer
	const GLfloat CUBE_SIZE = 10.0f;

	const GLfloat CUBE[] = { // Focal space
		CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,	// front, x+
		CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,
		CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,	// back, x-
		-CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,
		-CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,	// left, y+
		-CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,
		-CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,	// right, y-
		CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,
		CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,	// top, z+
		-CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,
		CUBE_SIZE,	CUBE_SIZE,	CUBE_SIZE,
		CUBE_SIZE,	-CUBE_SIZE,	CUBE_SIZE,
		CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE,	// bottom, z-
		CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	CUBE_SIZE,	-CUBE_SIZE,
		-CUBE_SIZE,	-CUBE_SIZE,	-CUBE_SIZE
	};

	glGenBuffers(1, &skyBoxPass.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, skyBoxPass.vertexBuffer); // RESET
	glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE), CUBE, GL_STATIC_DRAW);

	// Texture
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &skyBoxPass.texture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyBoxPass.texture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	const GLenum TARGETS[6] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	for(size_t i = 0; i < 6; i++)
	{
		GLubyte tempTex[16];

		for(size_t j = 0; j < 16; j++)
			tempTex[j] = i * 16 + j;

		glTexImage2D(TARGETS[i], 0, GL_LUMINANCE8, 4, 4, 0, GL_RED, GL_UNSIGNED_BYTE, tempTex);
	}

	// Reset
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return true;
}

/*
################################################################################################


	SKY SPHERE


################################################################################################
*/

/*--------------------------------------
	rnd::ReinitSkySphereProgram
--------------------------------------*/
bool rnd::ReinitSkySphereProgram()
{
	DeleteSkySphereProgram();

	prog_attribute attributes[] =
	{
		{SKY_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&skySphereProg.uniToClip, "toClip"},
		{&skySphereProg.uniTexDims, "texDims"},
		{&skySphereProg.uniDither, "dither"},
		{&skySphereProg.uniMipBias, "mipBias"},
		{&skySphereProg.uniMipFade, "mipFade"},
		{&skySphereProg.uniRampDistScale, "rampDistScale"},
		{&skySphereProg.uniInvNumRampTexels, "invNumRampTexels"},
		{&skySphereProg.uniSeamMipBias, "seamMipBias"},
		{&skySphereProg.samTexture, "texture"},
		{&skySphereProg.samRampLookup, "rampLookup"},
		{&skySphereProg.samRamps, "ramps"},
		{0, 0}
	};

	const char* fragSrcs[] = {
		"#version 130\n", // FIXME: 120 when possible
		shaderDefineMipMapping,
		shaderDefineMipFade,
		shaderDefineFilterBrightness,
		shaderDefineFilterColor,
		"\n",
		fragSkySphereSource
	};

	if(!InitProgram("sky sphere", skySphereProg.shaderProgram, skySphereProg.vertexShader,
	&vertSkyBoxSource, 1, skySphereProg.fragmentShader, fragSrcs,
	sizeof(fragSrcs) / sizeof(char*), attributes, uniforms))
		goto fail;

	glUseProgram(skySphereProg.shaderProgram); // RESET

	// Set constant uniforms
	GLfloat dither[16];
	MakeDitherArray(dither);

	glUniform1fv(skySphereProg.uniDither, 16, dither);
	glUniform1i(skySphereProg.samTexture, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(skySphereProg.samRamps, RND_RAMP_TEXTURE_NUM);
	glUniform1i(skySphereProg.samRampLookup, RND_RAMP_LOOKUP_TEXTURE_NUM);

	// Reset
	glUseProgram(0);
	return true;

fail:
	// FIXME: try again with low settings
	DeleteSkySphereProgram();
	return false;
}

/*--------------------------------------
	rnd::DeleteSkySphereProgram
--------------------------------------*/
void rnd::DeleteSkySphereProgram()
{
	if(skySphereProg.shaderProgram) glDeleteProgram(skySphereProg.shaderProgram);
	if(skySphereProg.vertexShader) glDeleteShader(skySphereProg.vertexShader);
	if(skySphereProg.fragmentShader) glDeleteShader(skySphereProg.fragmentShader);

	memset(&skySphereProg, 0, sizeof(skySphereProg));
}

/*--------------------------------------
	rnd::EnsureSkySphereProgram
--------------------------------------*/
void rnd::EnsureSkySphereProgram()
{
	if(!skySphereProg.shaderProgram)
	{
		if(!ReinitSkySphereProgram())
			WRP_FATAL("Could not compile sky-sphere shader program");
	}
}

/*--------------------------------------
	rnd::SkySphereState
--------------------------------------*/
void rnd::SkySphereState()
{
	glDepthFunc(GL_GEQUAL);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_GREATER, skyMask, -1);
	glUseProgram(skySphereProg.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, skyBoxPass.vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	const TextureGL* tex = (const TextureGL*)skySphereTex.Value();
	glBindTexture(GL_TEXTURE_2D, tex->texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Improves top and bottom
	glEnableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glVertexAttribPointer(SKY_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, 0);
	GLfloat skyBoxToClip[16];
	SkySphereToClip(skyBoxToClip);
	glUniformMatrix4fv(skySphereProg.uniToClip, 1, GL_FALSE, skyBoxToClip);

	if(skySphereProg.uniTexDims != -1)
		glUniform2f(skySphereProg.uniTexDims, tex->Dims()[0], tex->Dims()[1]);

	if(skySphereProg.uniMipBias != -1)
		glUniform1f(skySphereProg.uniMipBias, mipBias.Float());

	// FIXME: do a different mip fade value for sky sphere?
	if(skySphereProg.uniMipFade != -1)
		glUniform1f(skySphereProg.uniMipFade, mipFade.Float());

	PaletteGL* curPal = CurrentPaletteGL();

	if(skySphereProg.uniRampDistScale != -1)
		glUniform1f(skySphereProg.uniRampDistScale, curPal->rampDistScale);

	if(skySphereProg.uniInvNumRampTexels != -1)
		glUniform1f(skySphereProg.uniInvNumRampTexels, curPal->invNumRampTexels);

	glUniform1f(skySphereProg.uniSeamMipBias, skySphereSeamMipBias.Float());
}

/*--------------------------------------
	rnd::SkySphereToClip
--------------------------------------*/
void rnd::SkySphereToClip(GLfloat* toClipOut)
{
	const scn::Overlay& skyOvr = scn::SkyOverlay();
	const scn::Camera* cam;
	const GLfloat* vtc;

	if(skyOvr.cam)
	{
		cam = skyOvr.cam;
		vtc = gSkyVTC;
	}
	else
	{
		cam = scn::ActiveCamera();
		vtc = gViewToClip;
	}

	GLfloat skyBoxToView[16];
	com::MatQua(cam->FinalOri().Conjugate(), skyBoxToView);
	com::Multiply4x4(skyBoxToView, vtc, toClipOut);
}

/*--------------------------------------
	rnd::SkySphereCleanup
--------------------------------------*/
void rnd::SkySphereCleanup()
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glDisableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glDisable(GL_STENCIL_TEST);
}

/*--------------------------------------
	rnd::SkySpherePass
--------------------------------------*/
void rnd::SkySpherePass()
{
	SkySphereState();
	glDrawArrays(GL_QUADS, 0, 24); // FIXME: GL_QUADS is deprecated, driver might not like it
	SkySphereCleanup();
}

/*
################################################################################################


	SKY OVERLAY


################################################################################################
*/

/*--------------------------------------
	rnd::SkyOverlayPass
--------------------------------------*/
bool rnd::SkyOverlayPass()
{
	const scn::Overlay& ovr = scn::SkyOverlay();

	if(!ovr.NumEntities())
		return false;

	const Mesh* curMsh = 0;
	const Texture* curTex = 0;
	ModelState();
	glUseProgram(modelProg.shaderProgram);
	glStencilFunc(GL_EQUAL, skyMask, skyMask);

	ModelDrawArray<0, true>(modelProg, gSkyWTC, 0, ovr.Entities(), ovr.NumEntities(), curMsh,
		curTex);

	ModelCleanup();
	return true;
}

/*
################################################################################################


	SKY DEPTH RESET


################################################################################################
*/

/*--------------------------------------
	rnd::SkyDepthResetState
--------------------------------------*/
void rnd::SkyDepthResetState()
{
	glEnable(GL_STENCIL_TEST);
	glDepthFunc(GL_ALWAYS);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glUseProgram(skyDepthResetPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, farScreenVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glVertexAttribPointer(SKY_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, 0);
}

/*--------------------------------------
	rnd::SkyDepthResetCleanup
--------------------------------------*/
void rnd::SkyDepthResetCleanup()
{
	glDisableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_GREATER);
	glDisable(GL_STENCIL_TEST);
}

/*--------------------------------------
	rnd::SkyDepthResetPass

Undo changes to depth so glass pass doesn't clip sky. Note, this prevents rendering of glass-sky
entities since the sky's depth is lost at that point.

FIXME: can draw sky glass before regular glass pass with this depth reset in between to make sky glass work
--------------------------------------*/
void rnd::SkyDepthResetPass()
{
	SkyDepthResetState();
	glDrawArrays(GL_TRIANGLES, 0, 3);
	SkyDepthResetCleanup();
}

/*--------------------------------------
	rnd::InitSkyDepthResetPass
--------------------------------------*/
bool rnd::InitSkyDepthResetPass()
{
	prog_attribute attributes[] =
	{
		{SKY_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	if(!InitProgram("sky depth reset", skyDepthResetPass.shaderProgram,
	skyDepthResetPass.vertexShader, &vertSimpleSource, 1, skyDepthResetPass.fragmentShader,
	&fragSimpleSource, 1, attributes, 0))
		return false;

	return true;
}

/*
################################################################################################


	SKY LIGHT


################################################################################################
*/

/*--------------------------------------
	rnd::SkyLightPass

OUTPUT:
* COLOR_BUFFER: Light buffer
--------------------------------------*/
void rnd::SkyLightPass()
{
	timers[TIMER_SKY_LIGHT_PASS].Start();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glUseProgram(skyLightPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glEnableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glVertexAttribPointer(SKY_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, 0);
	glStencilFunc(GL_EQUAL, skyMask, skyMask);
	glUniform1f(skyLightPass.uniIntensity, NormalizedIntensity(scn::sky.FinalIntensity()));
	glUniform1f(skyLightPass.uniSubPalette, PackedLightSubPalette(scn::sky.subPalette));
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableVertexAttribArray(SKY_PASS_ATTRIB_POS);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	timers[TIMER_SKY_LIGHT_PASS].Stop();
}

/*--------------------------------------
	rnd::InitSkyLightPass
--------------------------------------*/
bool rnd::InitSkyLightPass()
{
	prog_attribute attributes[] =
	{
		{SKY_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&skyLightPass.uniIntensity, "intensity"},
		{&skyLightPass.uniSubPalette, "subPalette"},
		{0, 0}
	};

	if(!InitProgram("sky light", skyLightPass.shaderProgram, skyLightPass.vertexShader,
	&vertSkyLightSource, 1, skyLightPass.fragmentShader, &fragSkyLightSource, 1, attributes,
	uniforms))
		return false;

	return true;
}

/*
################################################################################################


	SKY LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::SetSkyFace

IN	iFace, sFileName
--------------------------------------*/
int rnd::SetSkyFace(lua_State* l)
{
	if(!SetSkyFace(luaL_checkinteger(l, 1), luaL_checkstring(l, 2)))
		luaL_error(l, "Failed to set sky face");

	return 0;
}

/*--------------------------------------
LUA	rnd::SkySphereTexture

OUT	[texSky]
--------------------------------------*/
int rnd::SkySphereTexture(lua_State* l)
{
	const Texture* sky = SkySphereTexture();
	
	if(sky)
	{
		sky->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	rnd::SetSkySphereTexture

IN	[texSky]
--------------------------------------*/
int rnd::SetSkySphereTexture(lua_State* l)
{
	SetSkySphereTexture(rnd::Texture::OptionalLuaTo(1));
	return 0;
}