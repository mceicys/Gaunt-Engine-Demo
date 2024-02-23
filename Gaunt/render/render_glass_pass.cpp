// render_glass_pass.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"

#define GLASS_PASS_ATTRIB_POS0		0
#define GLASS_PASS_ATTRIB_POS1		1
#define GLASS_PASS_ATTRIB_TEXCOORD	2

namespace rnd
{
	class AddPassType;
	class AmbientPassType;
	class MulPassType;
	class RecolorProgType;

	void	SetTextureState(Texture* tex, bool filter);
	void	UndoTextureState(Texture* tex, bool filter);
	template <bool (*Filter)(const scn::Entity&), class Pass>
	void	GlassDrawOverlays(Pass& p, const Mesh*& mshIO, const Texture*& texIO);
	void	GlassSetVertexAttribPointers(const Mesh& msh, const size_t (&offsets)[2]);
	template <class Pass>
	void	GlassSetFrameState(Pass& p);
	void	GlassState();
	void	GlassCleanup();

	template <class Pass>
	bool	InitGlassIntensityPass(const char* title, Pass& p, const char* fragSrc,
			prog_uniform* extraUniforms = 0);
	bool	InitAddPass();
	bool	InitMinPass();
	bool	InitMulPass();
}

/*======================================
	rnd::AddPassType
======================================*/
static class rnd::AddPassType
{
public:
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniModelToClip, uniOpacity, uniMipBias, uniAdditiveNormalization;
	GLint	samTexture;

	void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2], float lerp,
		const GLfloat (&wtc)[16], const com::Qua* normOri)
	{
		SetTextureState(ent.tex, filterTextureBrightness.Bool());

		GlassSetVertexAttribPointers(*ent.Mesh(), offsets);
		ModelUploadMTC(uniModelToClip, ent, wtc);
		ModelUploadLerp(uniLerp, lerp);
		ModelUploadTexShift(uniTexShift, ent);
		ModelUploadOpacity(uniOpacity, ent);
	}

	void UndoStateForEntity(const scn::Entity& ent)
	{
		UndoTextureState(ent.tex, filterTextureBrightness.Bool());
	}

	static bool Filter(const scn::Entity& ent)
	{
		return (ent.flags & ent.REGULAR_GLASS) && ent.FinalOpacity() > 0.0f;
	}
} addPass = {0};

/*======================================
	rnd::AmbientPassType
======================================*/
static class rnd::AmbientPassType
{
public:
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniModelToClip, uniOpacity, uniMipBias;
	GLint	samTexture;

	void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2], float lerp,
		const GLfloat (&wtc)[16], const com::Qua* normOri)
	{
		SetTextureState(ent.tex, filterTextureBrightness.Bool());

		GlassSetVertexAttribPointers(*ent.Mesh(), offsets);
		ModelUploadMTC(uniModelToClip, ent, wtc);
		ModelUploadLerp(uniLerp, lerp);
		ModelUploadTexShift(uniTexShift, ent);
		
		glUniform1f(uniOpacity, ent.FinalOpacity() / CurrentMaxIntensity());
	}

	void UndoStateForEntity(const scn::Entity& ent)
	{
		UndoTextureState(ent.tex, filterTextureBrightness.Bool());
	}

	static bool AmbientFilter(const scn::Entity& ent)
	{
		return (ent.flags & ent.AMBIENT_GLASS) && ent.FinalOpacity() > 0.0f;
	}

	static bool MinimumFilter(const scn::Entity& ent)
	{
		return (ent.flags & ent.MINIMUM_GLASS) && ent.FinalOpacity() > 0.0f;
	}
} ambientPass = {0};

/*======================================
	rnd::MulPassType
======================================*/
static class rnd::MulPassType
{
public:
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniModelToClip, uniOpacity, uniMipBias;
	GLint	samTexture;

	void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2], float lerp,
		const GLfloat (&wtc)[16], const com::Qua* normOri)
	{
		SetTextureState(ent.tex, filterTextureBrightness.Bool());

		GlassSetVertexAttribPointers(*ent.Mesh(), offsets);
		ModelUploadMTC(uniModelToClip, ent, wtc);
		ModelUploadLerp(uniLerp, lerp);
		ModelUploadTexShift(uniTexShift, ent);

		glUniform1f(uniOpacity, -ent.FinalOpacity());
	}

	void UndoStateForEntity(const scn::Entity& ent)
	{
		UndoTextureState(ent.tex, filterTextureBrightness.Bool());
	}

	static bool Filter(const scn::Entity& ent)
	{
		return (ent.flags & ent.REGULAR_GLASS) && ent.FinalOpacity() < 0.0f;
	}
} mulPass = {0};

/*======================================
	rnd::RecolorProgType
======================================*/
static class rnd::RecolorProgType
{
public:
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniTexDims, uniModelToClip, uniPackedSub, uniSubPalette,
			uniDither, uniMipBias, uniMipFade;
	GLint	samTexture;

	void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2], float lerp,
		const GLfloat (&wtc)[16], const com::Qua* normOri)
	{
		GlassSetVertexAttribPointers(*ent.Mesh(), offsets);
		ModelUploadMTC(uniModelToClip, ent, wtc);
		ModelUploadLerp(uniLerp, lerp);
		ModelUploadTexShift(uniTexShift, ent);

		if(uniTexDims != -1)
			ModelUploadTexDims(uniTexDims, ent);

		glColorMask(GL_FALSE, GL_FALSE, GL_TRUE,
			(ent.flags & ent.WEAK_GLASS) ? GL_FALSE : GL_TRUE);

		glUniform1f(uniPackedSub, PackedLightSubPalette(ent.subPalette));

		glUniform1f(uniSubPalette,
			ent.subPalette * (CurrentPaletteGL())->subCoordScale);
	}

	void UndoStateForEntity(const scn::Entity&) {}

	static bool StrongFilter(const scn::Entity& ent)
	{
		return ent.subPalette &&
			(ent.flags & (ent.REGULAR_GLASS | ent.AMBIENT_GLASS | ent.MINIMUM_GLASS)) &&
			(ent.flags & ent.WEAK_GLASS) == 0;
	}

	static bool WeakFilter(const scn::Entity& ent)
	{
		return ent.subPalette &&
			(ent.flags & (ent.REGULAR_GLASS | ent.AMBIENT_GLASS | ent.MINIMUM_GLASS)) &&
			(ent.flags & ent.WEAK_GLASS);
	}
} recolorProg = {0};

/*--------------------------------------
	rnd::SetTextureState

Assumes tex is bound.
--------------------------------------*/
void rnd::SetTextureState(Texture* tex, bool filter)
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((TextureGL*)tex)->MinFilter(filter));

	if(filter)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*--------------------------------------
	rnd::UndoTextureState
--------------------------------------*/
void rnd::UndoTextureState(Texture* tex, bool filter)
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((TextureGL*)tex)->MipFilter());

	if(filter)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

/*--------------------------------------
	rnd::GlassDrawOverlays
--------------------------------------*/
template <bool (*Filter)(const scn::Entity&), class Pass>
void rnd::GlassDrawOverlays(Pass& p, const Mesh*& curMsh, const Texture*& curTex)
{
	for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
	{
		const scn::Overlay& ovr = scn::Overlays()[i];

		if(const scn::Camera* cam = ovr.cam)
		{
			ModelDrawArray<Filter, true>(p, gOverlayWTC[i], 0, ovr.Entities(),
				ovr.NumEntities(), curMsh, curTex);
		}
	}
}

/*--------------------------------------
	rnd::GlassSetVertexAttribPointers
--------------------------------------*/
void rnd::GlassSetVertexAttribPointers(const Mesh& msh, const size_t (&offsets)[2])
{
	for(size_t j = 0; j < 2; j++)
	{
		glVertexAttribPointer(GLASS_PASS_ATTRIB_POS0 + j, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_mesh_place), (void*)offsets[j]);
	}

	glVertexAttribPointer(GLASS_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex_mesh_tex), (void*)0);
}

/*--------------------------------------
	rnd::GlassSetFrameState
--------------------------------------*/
template <class Pass>
void rnd::GlassSetFrameState(Pass& p)
{
	glUniform1f(p.uniMipBias, mipBias.Float());
}

/*--------------------------------------
	rnd::GlassState
--------------------------------------*/
void rnd::GlassState()
{
	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL); // Helps reduce z-fighting between smoke and fire
	glEnableVertexAttribArray(GLASS_PASS_ATTRIB_POS0);
	glEnableVertexAttribArray(GLASS_PASS_ATTRIB_POS1);
	glEnableVertexAttribArray(GLASS_PASS_ATTRIB_TEXCOORD);
}

/*--------------------------------------
	rnd::GlassCleanup
--------------------------------------*/
void rnd::GlassCleanup()
{
	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glDisableVertexAttribArray(GLASS_PASS_ATTRIB_POS0);
	glDisableVertexAttribArray(GLASS_PASS_ATTRIB_POS1);
	glDisableVertexAttribArray(GLASS_PASS_ATTRIB_TEXCOORD);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

/*--------------------------------------
	rnd::GlassPass

OUTPUT:
* COLOR_BUFFER: Light buffer
* DEPTH_BUFFER: Colored glass entities (depth buffer pretty useless beyond this point)

Glass entities are drawn twice to add/multiply light and change the light sub-palette.

FIXME: Minimum glass has a harsh transition to real lighting; the seam between the two gradients
	is obvious to the viewer.
		Is there a blending scheme that could make it slightly additive that fades away as real
		light surpasses the minimum intensity?
			One thing that makes this difficult is the normalization of intensity; 1.0 in the
			color buffer actually means CurrentMaxIntensity() in the composition shader, and
			increasing the max intensity setting should not affect the final image except where
			intensity was getting clamped
		If not, a separate component in the light buffer is required

FIXME: Should ambient glass affect influence?
--------------------------------------*/
void rnd::GlassPass()
{
	if(!numVisGlassEnts)
	{
		// No world glass ents; make sure there's no glass overlays
		size_t i = 0;
		for(; i < scn::NUM_OVERLAYS; i++)
		{
			const scn::Overlay& ovr = scn::Overlays()[i];

			if(const scn::Camera* cam = ovr.cam)
			{
				const res::Ptr<const scn::Entity>* ents = ovr.Entities();
				size_t numEnts = ovr.NumEntities();
				size_t j = 0;
				for(; j < numEnts && !ents[j]->Glass(); j++);
				
				if(j < numEnts)
					break;
			}
		}

		if(i == scn::NUM_OVERLAYS)
			return;
	}

	timers[TIMER_GLASS_PASS].Start();

	GlassState();
	glEnable(GL_BLEND);
	const rnd::Mesh* curMsh = 0;
	const rnd::Texture* curTex = 0;

	// Additive
	glUseProgram(addPass.shaderProgram);
	GlassSetFrameState(addPass);
	float maxRampSpan = CurrentPalette()->MaxRampSpan();
	float rampVariance = RampVariance();
	glUniform1f(addPass.uniAdditiveNormalization, maxRampSpan / (maxRampSpan + rampVariance));
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	ModelDrawArray<AddPassType::Filter, true>(addPass, gWorldToClip, 0, visGlassEnts.o,
		numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<AddPassType::Filter>(addPass, curMsh, curTex);

	// Ambient
	glUseProgram(ambientPass.shaderProgram);
	GlassSetFrameState(ambientPass);
	//glBlendEquation(GL_FUNC_ADD);
	//glBlendFunc(GL_ONE, GL_ONE);
	glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
	//glDepthMask(GL_FALSE);

	ModelDrawArray<AmbientPassType::AmbientFilter, true>(ambientPass, gWorldToClip, 0,
		visGlassEnts.o, numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<AmbientPassType::AmbientFilter>(ambientPass, curMsh, curTex);

	// Minimum
	//glUseProgram(ambientPass.shaderProgram);
	//GlassSetFrameState(ambientPass);
	glBlendEquation(GL_MAX);
	//glBlendFunc(GL_ONE, GL_ONE);
	//glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
	//glDepthMask(GL_FALSE);

	ModelDrawArray<AmbientPassType::MinimumFilter, true>(ambientPass, gWorldToClip, 0,
		visGlassEnts.o, numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<AmbientPassType::MinimumFilter>(ambientPass, curMsh, curTex);

	// Multiplicative
	glUseProgram(mulPass.shaderProgram);
	GlassSetFrameState(mulPass);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	//glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
	//glDepthMask(GL_FALSE);

	ModelDrawArray<MulPassType::Filter, true>(mulPass, gWorldToClip, 0, visGlassEnts.o,
		numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<MulPassType::Filter>(mulPass, curMsh, curTex);

	// Recolor
	// Entities are drawn into the depth buffer so the closest frag's sub-palette is used
	glUseProgram(recolorProg.shaderProgram);
	GlassSetFrameState(recolorProg);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	ModelDrawArray<RecolorProgType::StrongFilter, true>(recolorProg, gWorldToClip, 0,
		visGlassEnts.o, numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<RecolorProgType::StrongFilter>(recolorProg, curMsh, curTex);

	ModelDrawArray<RecolorProgType::WeakFilter, true>(recolorProg, gWorldToClip, 0,
		visGlassEnts.o, numVisGlassEnts, curMsh, curTex);

	GlassDrawOverlays<RecolorProgType::WeakFilter>(recolorProg, curMsh, curTex);

	GlassCleanup();

	timers[TIMER_GLASS_PASS].Stop();
}

/*--------------------------------------
	rnd::InitGlassPass
--------------------------------------*/
bool rnd::InitGlassPass()
{
	return InitGlassIntensityPass("glass add", addPass, fragModelAddSource) &&
		InitGlassIntensityPass("glass ambient", ambientPass, fragModelAmbientSource) &&
		InitGlassIntensityPass("glass multiply", mulPass, fragModelMulSource) &&
		ReinitGlassRecolorProgram();
}

/*--------------------------------------
	rnd::InitGlassIntensityPass
--------------------------------------*/
template <class Pass>
bool rnd::InitGlassIntensityPass(const char* title, Pass& p, const char* fragSrc,
	prog_uniform* extraUniforms)
{
	prog_attribute attributes[] =
	{
		{GLASS_PASS_ATTRIB_POS0, "pos0"},
		{GLASS_PASS_ATTRIB_POS1, "pos1"},
		{GLASS_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{0, 0}
	};

	prog_uniform sharedUniforms[] =
	{
		{&p.uniLerp, "lerp"},
		{&p.uniTexShift, "texShift"},
		{&p.uniModelToClip, "modelToClip"},
		{&p.uniOpacity, "opacity"},
		{&p.uniMipBias, "mipBias"},
		{&p.samTexture, "texture"},
		{0, 0}
	};

	prog_uniform* uniforms = sharedUniforms;

	if(extraUniforms)
	{
		size_t numShared = sizeof(sharedUniforms) / sizeof(prog_uniform) - 1;
		size_t numExtra = 0;
		for(; extraUniforms[numExtra].identifier; numExtra++);
		uniforms = new prog_uniform[numShared + numExtra + 1];

		for(size_t i = 0; i < numShared; i++)
			uniforms[i] = sharedUniforms[i];

		for(size_t i = numShared, j = 0; j < numExtra; i++, j++)
			uniforms[i] = extraUniforms[j];

		const prog_uniform SENTINEL = {0, 0};
		uniforms[numShared + numExtra] = SENTINEL;
	}

	const char* vertSrcs[] = {
		"#version 120\n",
		vertModelNoNormalSource
	};

	bool goodProg = InitProgram(title, p.shaderProgram, p.vertexShader, vertSrcs,
		sizeof(vertSrcs) / sizeof(char*), p.fragmentShader, &fragSrc, 1, attributes, uniforms);

	if(extraUniforms)
	{
		delete[] uniforms;
		uniforms = 0;
	}

	if(!goodProg)
		return false;

	glUseProgram(p.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(p.samTexture, RND_VARIABLE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}

/*--------------------------------------
	rnd::ReinitGlassRecolorProgram
--------------------------------------*/
bool rnd::ReinitGlassRecolorProgram()
{
	DeleteGlassRecolorProgram();

	prog_attribute attributes[] =
	{
		{GLASS_PASS_ATTRIB_POS0, "pos0"},
		{GLASS_PASS_ATTRIB_POS1, "pos1"},
		{GLASS_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&recolorProg.uniLerp, "lerp"},
		{&recolorProg.uniTexShift, "texShift"},
		{&recolorProg.uniTexDims, "texDims"},
		{&recolorProg.uniModelToClip, "modelToClip"},
		{&recolorProg.uniPackedSub, "packedSub"},
		{&recolorProg.uniSubPalette, "subPalette"},
		{&recolorProg.uniDither, "dither"},
		{&recolorProg.uniMipBias, "mipBias"},
		{&recolorProg.uniMipFade, "mipFade"},
		{&recolorProg.samTexture, "texture"},
		{0, 0}
	};

	const char* vertSrcs[] = {
		"#version 120\n",
		//"#define ORIG_TEXEL 1\n",
		vertModelNoNormalSource
	};

	const char* fragSrcs[] = {
		"#version 130\n", // FIXME: set to 120 if not using textureLod
		shaderDefineMipMapping,
		shaderDefineMipFade,
		shaderDefineFilterColor,
		"\n",
		fragModelRecolorSource
	};

	if(!InitProgram("glass recolor", recolorProg.shaderProgram, recolorProg.vertexShader,
	vertSrcs, sizeof(vertSrcs) / sizeof(char*), recolorProg.fragmentShader, fragSrcs,
	sizeof(fragSrcs) / sizeof(char*), attributes, uniforms))
		goto fail;

	glUseProgram(recolorProg.shaderProgram); // RESET

	// Set constant uniforms
	if(recolorProg.uniDither != -1)
	{
		GLfloat dither[16];
		MakeDitherArray(dither);
		glUniform1fv(recolorProg.uniDither, 16, dither);
	}

	glUniform1i(recolorProg.samTexture, RND_VARIABLE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;

fail:
	// FIXME: set low quality options and try again like in ReinitWorldProgram
	DeleteGlassRecolorProgram();
	return false;
}

/*--------------------------------------
	rnd::DeleteGlassRecolorProgram
--------------------------------------*/
void rnd::DeleteGlassRecolorProgram()
{
	if(recolorProg.shaderProgram) glDeleteProgram(recolorProg.shaderProgram);
	if(recolorProg.vertexShader) glDeleteShader(recolorProg.vertexShader);
	if(recolorProg.fragmentShader) glDeleteShader(recolorProg.fragmentShader);

	memset(&recolorProg, 0, sizeof(recolorProg));
}

/*--------------------------------------
	rnd::EnsureGlassRecolorProgram
--------------------------------------*/
void rnd::EnsureGlassRecolorProgram()
{
	if(!recolorProg.shaderProgram)
	{
		if(!ReinitGlassRecolorProgram())
			WRP_FATAL("Could not compile glass-recolor shader program");
	}
}