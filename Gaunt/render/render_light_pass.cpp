// render_light_pass.cpp
// Martynas Ceicys

// FIXME: split into source file for each major pass

#include "render.h"
#include "render_private.h"
#include "../console/console.h"
#include "../hit/hit.h"
#include "../wrap/wrap.h"

namespace rnd
{
	com::Arr<const scn::Zone*> litZones;
	com::Arr<const scn::Entity*> litEnts;

	shadow_pass_type shadowPass = {0};
	shadow_fade_pass_type shadowFadePass = {0};

	// AMBIENT PASS
	void	AmbientPassState();
	void	AmbientPassCleanup();
	void	AmbientPass();
	bool	InitAmbientPass();

	// SHADOW SUN PASS
	void	DrawSunShadowMap(const com::Vec3& dir, GLfloat (&sunViewToClipsOut)[4][16],
			com::Vec3 (&centersOut)[4], com::Vec3 (&locksOut)[4]);
	void	DrawCascade(GLint x, GLint y, const com::Vec3& dir, float n, float f, GLfloat cBias,
			GLfloat cSlopeBias, GLfloat (&sunViewToClipOut)[16], com::Vec3& centerOut,
			com::Vec3& lockOut, const Texture*& curTexIO);
	template <typename pass>
	void	SetSharedSunShadowUniforms(const pass& p, const scn::Entity& ent, const MeshGL& msh,
			GLfloat (&sunMTW)[16], const GLfloat (&sunWTC)[16], GLfloat (&sunMTC)[16],
			size_t (&offsetsOut)[2]);
	bool	InitShadowSunPass();
	bool	InitShadowSunFadePass();

	// SUN PASS
	void	SunPassState(const com::Vec3& dir);
	void	SunPassCleanup();
	void	SunPass(bool drawSun);
	bool	InitSunPass();
	void	FocalToSunClip(float pitch, float yaw, const com::Vec3& center,
			const com::Vec3& lock, const GLfloat (&sunViewToClip)[16],
			GLfloat (&focalToSunClipOut)[16]);

	// SHADOW POINT PASS
	void	DrawShadowMap(const scn::Bulb& bulb, GLfloat& depthProjOut, float radius,
			const com::Vec3& pos, uint16_t& overlaysOut);
	bool	InitShadowPass();
	bool	InitShadowFadePass();

	// BULB PASS
	void	BulbPassState();
	void	BulbPassCleanup();
	void	BulbDrawVisible();
	void	BulbDraw(const scn::Bulb& bulb);
	void	BulbStencilUpdateMatrices(float radius, const com::Vec3& pos, const com::Qua& ori);
	void	BulbPass();
	bool	InitBulbStencilPass();
	bool	InitBulbLightPass();

	// LIGHT PASS
	void	LightPassState();
	void	LightPassCleanup();
}

/*
################################################################################################


	AMBIENT PASS


################################################################################################
*/

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniTopInfluence, uniInfluenceFactor, uniSeed, uniAmbSubPalette, uniAmbIntensity,
			uniAmbColorSmooth, uniAmbColorPriority;
} ambientPass = {0};

/*--------------------------------------
	rnd::AmbientPassState
--------------------------------------*/
void rnd::AmbientPassState()
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glUseProgram(ambientPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);

	// Update uniforms
	glUniform1f(ambientPass.uniTopInfluence, topInfluence);
	glUniform1f(ambientPass.uniInfluenceFactor, influenceFactor);
}

/*--------------------------------------
	rnd::AmbientPassCleanup
--------------------------------------*/
void rnd::AmbientPassCleanup()
{
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*--------------------------------------
	rnd::AmbientPass
--------------------------------------*/
void rnd::AmbientPass()
{
	float ambientIntensity = scn::ambient.FinalIntensity();
	float cloudIntensity = scn::cloud.FinalIntensity();
	bool lightClouds = numVisCloudEnts && cloudIntensity > 0.0f;

	if(ambientIntensity <= 0.0f && !lightClouds)
		return;

	timers[TIMER_AMBIENT_PASS].Start();

	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer); // FIXME: redundant? fbo is set outside of LightPass

	AmbientPassState();
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	if(ambientIntensity > 0.0f)
	{
		glStencilFunc(GL_EQUAL, 0, skyMask); // Ignore sky FIXME: also ignore cloud?
		glUniform1f(ambientPass.uniSeed, RandomSeed());
		glUniform1f(ambientPass.uniAmbSubPalette,
			PackedLightSubPalette(scn::ambient.subPalette));
		glUniform1f(ambientPass.uniAmbIntensity, NormalizedIntensity(ambientIntensity));
		glUniform1f(ambientPass.uniAmbColorSmooth, scn::ambient.colorSmooth);
		glUniform1f(ambientPass.uniAmbColorPriority, scn::ambient.colorPriority);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	if(lightClouds)
	{
		glStencilFunc(GL_EQUAL, cloudMask, cloudMask);
		glUniform1f(ambientPass.uniSeed, RandomSeed());
		glUniform1f(ambientPass.uniAmbSubPalette,
			PackedLightSubPalette(scn::cloud.subPalette));
		glUniform1f(ambientPass.uniAmbIntensity, NormalizedIntensity(cloudIntensity));
		glUniform1f(ambientPass.uniAmbColorSmooth, scn::cloud.colorSmooth);
		glUniform1f(ambientPass.uniAmbColorPriority, scn::cloud.colorPriority);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	AmbientPassCleanup();

	timers[TIMER_AMBIENT_PASS].Stop();
}

/*--------------------------------------
	rnd::InitAmbientPass
--------------------------------------*/
bool rnd::InitAmbientPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&ambientPass.uniTopInfluence, "topInfluence"},
		{&ambientPass.uniInfluenceFactor, "influenceFactor"},
		{&ambientPass.uniSeed, "seed"},
		{&ambientPass.uniAmbSubPalette, "ambSubPalette"},
		{&ambientPass.uniAmbIntensity, "ambIntensity"},
		{&ambientPass.uniAmbColorSmooth, "ambColorSmooth"},
		{&ambientPass.uniAmbColorPriority, "ambColorPriority"},
		{0, 0}
	};

	if(!InitProgram("ambient", ambientPass.shaderProgram, ambientPass.vertexShader,
	&vertAmbientSource, 1, ambientPass.fragmentShader, &fragAmbientSource, 1, attributes,
	uniforms))
		return false;

	return true;
}

/*
################################################################################################


	SHADOW SUN PASS


################################################################################################
*/

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniToClip;
} shadowSunPass = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniToClip, uniOpacity, uniSeed, samTexture;
} shadowSunFadePass = {0};

/*--------------------------------------
	rnd::DrawSunShadowMap
--------------------------------------*/
void rnd::DrawSunShadowMap(const com::Vec3& dir, GLfloat (&sunViewToClips)[4][16],
	com::Vec3 (&centers)[4], com::Vec3 (&locks)[4])
{
	glDepthMask(GL_TRUE);

	if(usingFBOs.Bool())
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboFlatShadowBuffer);
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	else
	{
		glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_2D, texFlatShadowBuffer);
		glViewport(0, 0, allocCascadeRes, allocCascadeRes);
	}

	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS1);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	const Texture* curTex = 0;

	DrawCascade(0, 0, dir, camHull.NearDist(), cascadeDist0.Float(), polyBias0.Float(),
		polySlopeBias0.Float(), sunViewToClips[0], centers[0], locks[0], curTex);

	DrawCascade(allocCascadeRes, 0, dir, cascadeDist0.Float(), cascadeDist1.Float(),
		polyBias1.Float(), polySlopeBias1.Float(), sunViewToClips[1], centers[1], locks[1],
		curTex);

	DrawCascade(0, allocCascadeRes, dir, cascadeDist1.Float(), cascadeDist2.Float(),
		polyBias2.Float(), polySlopeBias2.Float(), sunViewToClips[2], centers[2], locks[2],
		curTex);

	DrawCascade(allocCascadeRes, allocCascadeRes, dir, cascadeDist2.Float(), camHull.FarDist(),
		polyBias3.Float(), polySlopeBias3.Float(), sunViewToClips[3], centers[3], locks[3],
		curTex);

	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer);

	if(curTex)
	{
		glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	}

	glViewport(0, 0, wrp::VideoWidth(), wrp::VideoHeight());
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // FIXME: mask out g and b since they're not affected here?
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS1);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
}

/*--------------------------------------
	rnd::DrawCascade

FIXME: option to allow a cascade to not be locked so shadows jitter less on an object moving and rotating with the camera

FIXME: disable back face culling for clouds if they're two-sided
--------------------------------------*/
void rnd::DrawCascade(GLint x, GLint y, const com::Vec3& dir, float n, float f, GLfloat cBias,
	GLfloat cSlopeBias, GLfloat (&sunViewToClip)[16], com::Vec3& center, com::Vec3& lock,
	const Texture*& curTex)
{
	if(n >= f)
		return;

	GLfloat sunWorldToView[16];
	GLfloat sunWorldToClip[16];
	GLfloat sunModelToWorld[16];
	GLfloat sunModelToClip[16];
	const scn::Camera& cam = *scn::ActiveCamera();
	float radius;
	center = com::VecRot(camHull.MinSphere(radius, n, f), cam.FinalOri());
	com::Vec3 pos = cam.FinalPos() + center;
	GLfloat pitch, yaw;
	(-dir).PitchYaw(pitch, yaw);
	WorldToSunView(pitch, yaw, -pos, sunWorldToView);

	// Translate in texel-sized increments to prevent shimmering
	GLfloat texelSize = radius * 2.0f / allocCascadeRes;
	lock.x = fmod(sunWorldToView[3], texelSize);
	lock.y = fmod(sunWorldToView[7], texelSize);
	lock.z = fmod(sunWorldToView[11], texelSize);
	sunWorldToView[3] -= lock.x;
	sunWorldToView[7] -= lock.y;
	sunWorldToView[11] -= lock.z;

	/* Extend sun frustum beyond camera's view radius or to world bounds, whichever is farther
		Wastes precision, but ensures shadows are cast by visible entities outside of world
		bounds and geometry within world but outside camera frustum */
	float boundsNear, boundsFar;
	hit::Hull worldBox(scn::WorldMin() - texelSize, scn::WorldMax() + texelSize);
	worldBox.BoxSpan(-dir, boundsNear, boundsFar);
	float boundsClipOffset = com::Dot(dir, pos);
	boundsNear += boundsClipOffset;
	boundsFar += boundsClipOffset;
	float visFar = camHull.Vertices()[0].pos.Mag();
	float visNear = -visFar;
	float visClipOffset = com::Dot(dir, center);
	visNear += visClipOffset;
	visFar += visClipOffset;
	float sunNear = COM_MIN(boundsNear, visNear);
	float sunFar = COM_MAX(boundsFar, visFar);

	com::Vec3 fMin(sunNear, -radius, -radius);
	com::Vec3 fMax(sunFar, radius, radius);
	ViewToClipOrth(-fMax.y, -fMin.y, fMin.z, fMax.z, fMin.x, fMax.x, sunViewToClip);
	com::Multiply4x4(sunWorldToView, sunViewToClip, sunWorldToClip);
	bool fadeProg = false;
	glUseProgram(shadowSunPass.shaderProgram);

	bool polygonOffset = cBias || cSlopeBias;

	if(polygonOffset)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-cSlopeBias, -cBias); // Negated for reverse depth
	}

	if(usingFBOs.Bool())
		glViewport(x, y, allocCascadeRes, allocCascadeRes);
	else
		glClear(GL_DEPTH_BUFFER_BIT);

	size_t numLitZones, numLitEnts;
	CascadeDrawLists(pos, pitch, yaw, fMin, fMax, litZones, numLitZones, litEnts, numLitEnts);
	glUniform1f(shadowSunPass.uniLerp, 0.0f);
	glUniformMatrix4fv(shadowSunPass.uniToClip, 1, GL_FALSE, sunWorldToClip);

	glBindBuffer(GL_ARRAY_BUFFER, worldVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldElementBuffer);

	for(size_t i = 0; i < 2; i++)
	{
		// FIXME: already switching the shader prog for transparent ents, do a non-lerping shader for the world
		glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0 + i, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_world), (void*)0);
	}

	for(uint32_t i = 0; i < numLitZones; i++)
		WorldDrawZone(litZones[i]);

	const Mesh* curMsh = 0;

	for(size_t j = 0; j < numLitEnts; j++)
	{
		const scn::Entity& ent = *litEnts[j];
		const MeshGL* msh = (MeshGL*)ent.Mesh();
		const TextureGL* tex = (TextureGL*)ent.tex.Value();

		// Skip if mesh is smaller than a texel
		// FIXME: optional size/factor for each cascade?
		if(msh->Radius() * 2.0f <= texelSize)
			continue;

		float opacity = ent.FinalOpacity();
		size_t offsets[2];

		if(opacity == 1.0f && !tex->Alpha())
		{
			if(fadeProg)
			{
				glUseProgram(shadowSunPass.shaderProgram);
				glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
				fadeProg = false;
			}

			SetSharedSunShadowUniforms(shadowSunPass, ent, *msh, sunModelToWorld,
				sunWorldToClip, sunModelToClip, offsets);
		}
		else
		{
			if(!fadeProg)
			{
				glUseProgram(shadowSunFadePass.shaderProgram);
				glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
				glUniform1f(shadowSunFadePass.uniSeed, shaderRandomSeed); // FIXME: create Uniform class that Ensures a value
				fadeProg = true;
			}

			com::Vec2 uv = ent.FinalUV();
			glUniform2f(shadowSunFadePass.uniTexShift, uv.x, uv.y);
			glUniform1f(shadowSunFadePass.uniOpacity, opacity);
			SetSharedSunShadowUniforms(shadowSunFadePass, ent, *msh, sunModelToWorld,
				sunWorldToClip, sunModelToClip, offsets);
		}

		if(curMsh != msh)
		{
			glBindBuffer(GL_ARRAY_BUFFER, msh->vBufName);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, msh->iBufName);
			curMsh = msh;
		}
		
		if(fadeProg && curTex != tex)
		{
			glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
			glBindTexture(GL_TEXTURE_2D, tex->texName);
			curTex = tex;
		}

		for(size_t l = 0; l < 2; l++)
		{
			glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0 + l, 3, GL_FLOAT, GL_FALSE,
				sizeof(vertex_mesh_place), (void*)offsets[l]);
		}

		if(fadeProg)
		{
			glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
				sizeof(vertex_mesh_tex), (void*)0);
		}

		ModelDrawElements(*msh);
	}

	if(!usingFBOs.Bool())
	{
		glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 0, 0, allocCascadeRes, allocCascadeRes);
	}

	if(polygonOffset)
		glDisable(GL_POLYGON_OFFSET_FILL);
}

/*--------------------------------------
	rnd::SetSharedSunShadowUniforms
--------------------------------------*/
template <typename pass>
void rnd::SetSharedSunShadowUniforms(const pass& p, const scn::Entity& ent, const MeshGL& msh,
	GLfloat (&sunMTW)[16], const GLfloat (&sunWTC)[16], GLfloat (&sunMTC)[16],
	size_t (&offsets)[2])
{
	ModelToWorld(ent, sunMTW);
	com::Multiply4x4(sunMTW, sunWTC, sunMTC);
	glUniformMatrix4fv(p.uniToClip, 1, GL_FALSE, sunMTC);

	float lerp;
	ModelVertexAttribOffset(ent, msh, offsets, lerp);
	glUniform1f(p.uniLerp, lerp);
}

/*--------------------------------------
	rnd::InitShadowSunPass
--------------------------------------*/
bool rnd::InitShadowSunPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos0"},
		{RND_LIGHT_PASS_ATTRIB_POS1, "pos1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&shadowSunPass.uniLerp, "lerp"},
		{&shadowSunPass.uniToClip, "toClip"},
		{0, 0}
	};

	if(!InitProgram("shadow sun", shadowSunPass.shaderProgram, shadowSunPass.vertexShader,
	&vertShadowSunSource, 1, shadowSunPass.fragmentShader, &fragShadowSource, 1, attributes,
	uniforms))
		return false;

	return true;
}

/*--------------------------------------
	rnd::InitShadowSunFadePass
--------------------------------------*/
bool rnd::InitShadowSunFadePass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos0"},
		{RND_LIGHT_PASS_ATTRIB_POS1, "pos1"},
		{RND_LIGHT_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&shadowSunFadePass.uniLerp, "lerp"},
		{&shadowSunFadePass.uniTexShift, "texShift"},
		{&shadowSunFadePass.uniToClip, "toClip"},
		{&shadowSunFadePass.uniOpacity, "opacity"},
		{&shadowSunFadePass.uniSeed, "seed"},
		{&shadowSunFadePass.samTexture, "texture"},
		{0, 0}
	};

	if(!InitProgram("shadow sun fade", shadowSunFadePass.shaderProgram,
	shadowSunFadePass.vertexShader, &vertShadowSunFadeSource, 1,
	shadowSunFadePass.fragmentShader, &fragShadowFadeSource, 1, attributes, uniforms))
		return false;

	glUseProgram(shadowSunFadePass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(shadowSunFadePass.samTexture, RND_VARIABLE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}

/*
################################################################################################


	SUN PASS


################################################################################################
*/

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniClipToFocal, uniFrustumRatio, uniTopInfluence, uniInfluenceFactor, uniSeed,
			uniDepthProj, uniDepthBias, uniSurfaceBiases, uniFocalToSunClips, uniKernels,
			uniLitExps, uniPureSegment, uniSunDir, uniSunSubPalette, uniSunIntensity,
			uniSunColorSmooth, uniSunColorPriority;
	GLint	samGeometry, samDepth, samShadow;
} sunPass = {0};

/*--------------------------------------
	rnd::SunPassState
--------------------------------------*/
void rnd::SunPassState(const com::Vec3& dir)
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glUseProgram(sunPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texDepthBuffer);
	glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texFlatShadowBuffer);

	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);

	// Update uniforms
	glUniform1f(sunPass.uniTopInfluence, topInfluence);
	glUniform1f(sunPass.uniInfluenceFactor, influenceFactor);
	glUniform1f(sunPass.uniSeed, RandomSeed());
	glUniform1f(sunPass.uniSunSubPalette, PackedLightSubPalette(scn::sun.subPalette));
	glUniform1f(sunPass.uniSunIntensity, NormalizedIntensity(scn::sun.FinalIntensity()));
	glUniform1f(sunPass.uniSunColorSmooth, scn::sun.colorSmooth);
	glUniform1f(sunPass.uniSunColorPriority, scn::sun.colorPriority);
	glUniform3f(sunPass.uniSunDir, dir.x, dir.y, dir.z);
	glUniform1f(sunPass.uniFrustumRatio, 1.0f / nearClip.Float());

	/*
	depthProj0 and depthProj1 are used to convert depth back to linear depth (view x)

	depth = (linear * viewToClip[8] + viewToClip[11]) / linear
	depth = viewToClip[8] + viewToClip[11] / linear
	depth - viewToClip[8] = viewToClip[11] / linear
	(depth - viewToClip[8]) * linear = viewToClip[11]
	linear = viewToClip[11] / (depth - viewToClip[8])
	*/

#if 0
	glUniform1f(sunPass.uniDepthProj0, (gViewToClip[8] + 1.0f) * 0.5f);
	glUniform1f(sunPass.uniDepthProj1, gViewToClip[11] * 0.5f);

	glUniform1f(sunPass.uniDepthBias, depthBias.Float());
#endif

	/*
	depth = viewToClip[11] / linear
	depth * linear = viewToClip[11]
	linear = viewToClip[11] / depth
	*/

	glUniform1f(sunPass.uniDepthProj, gViewToClip[11]);
	glUniform1f(sunPass.uniDepthBias, depthBias.Float());

	GLfloat surfaceBiases[4] = {
		surfaceBias0.Float(),
		surfaceBias1.Float(),
		surfaceBias2.Float(),
		surfaceBias3.Float()
	};

	glUniform1fv(sunPass.uniSurfaceBiases, 4, surfaceBiases);

	GLfloat kernels[4] = {
		kernelSize0.Float(),
		kernelSize1.Float(),
		kernelSize2.Float(),
		kernelSize3.Float()
	};

	glUniform1fv(sunPass.uniKernels, 4, kernels);

	GLfloat litExps[4] = {
		cascadeExp0.Float(),
		cascadeExp1.Float(),
		cascadeExp2.Float(),
		cascadeExp3.Float()
	};

	glUniform1fv(sunPass.uniLitExps, 4, litExps);
	glUniform1f(sunPass.uniPureSegment, cascadePurity.Float());
}

/*--------------------------------------
	rnd::SunPassCleanup
--------------------------------------*/
void rnd::SunPassCleanup()
{
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*--------------------------------------
	rnd::SunPass

FIXME: ignoring sky stencil is not saving much time on integrated GPU
--------------------------------------*/
void rnd::SunPass(bool drawSun)
{
	com::Vec3 dir = scn::sun.FinalPos().Normalized();

	if(!drawSun || dir == 0.0f)
		return;

	timers[TIMER_SUN_SHADOW_PASS].Start();

	GLfloat sunViewToClips[4][16];
	com::Vec3 centers[4];
	com::Vec3 locks[4];
	DrawSunShadowMap(dir, sunViewToClips, centers, locks);

	timers[TIMER_SUN_SHADOW_PASS].Stop();
	timers[TIMER_SUN_LIGHT_PASS].Start();

	SunPassState(dir);
	glStencilFunc(GL_EQUAL, 0, skyMask | overlayRelitMask); // Ignore sky and relit overlays
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glUniformMatrix4fv(sunPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	GLfloat focalToSunClips[4][16];
	float pitch, yaw;
	(-dir).PitchYaw(pitch, yaw);

	// Calculate each cascade's matrix to convert a focal position to shadow map's clip space
	for(size_t i = 0; i < 4; i++)
		FocalToSunClip(pitch, yaw, centers[i], locks[i], sunViewToClips[i], focalToSunClips[i]);

	// FIXME TEMP
#if 0
	int go = 0;
	if(go){
		unsigned width = wrp::VideoWidth();
		unsigned height = wrp::VideoHeight();
		GLfloat pixel[2] = {500, 670};
		GLfloat pos[4] = {(pixel[0] / width) * 2.0 - 1.0, (1.0 - (pixel[1] / height)) * 2.0 - 1.0, 1.0, 1.0};

			// emulate vertex shader
		GLfloat tdc[2] = {(pos[0] + 1.0) * 0.5, (pos[1] + 1.0) * 0.5};
		GLfloat focal[4];
		com::Multiply4x4Vec4(pos, gClipToFocal, focal);
		com::Vec3 ray;
		for(size_t i = 0; i < 3; i++) ray[i] = focal[i] / focal[3] * (1.0 / nearClip.Float());
			// emulate fragment shader
		size_t bufferSize = width * height * 4;
		GLfloat* buffer = new GLfloat[bufferSize];
		size_t texelStart = (size_t)(tdc[0] * width) + (size_t)(tdc[1] * height * width);
		size_t texelStartRGBA = texelStart * 4;
		glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buffer);
		GLfloat geometryFrag[4] = {buffer[texelStartRGBA], buffer[texelStartRGBA + 1], buffer[texelStartRGBA + 2], buffer[texelStartRGBA + 3]};
		com::Vec3 tempVec;
		tempVec.x = geometryFrag[1];
		tempVec.y = geometryFrag[2];
		tempVec.z = geometryFrag[3];
		tempVec = (tempVec * 2.0 - 1.0).Normalized();
		GLfloat norm[4] = {tempVec.x, tempVec.y, tempVec.z, 1.0};
		glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, buffer);
		GLfloat depthVal = buffer[texelStart];
		delete[] buffer;
		GLfloat biasedLinDepth = gViewToClip[11] / (depthVal + depthBias.Float());
		com::Vec3 focalFrag = ray * biasedLinDepth;
		GLfloat temp[4];
		for(size_t i = 0; i < 3; i++) temp[i] = focalFrag[i] + norm[i] * surfaceBias0.Float();
		temp[3] = 1.0;
		GLfloat coords4[4];
		com::Multiply4x4Vec4(temp, focalToSunClips[0], coords4);
		com::Vec3 coords;
		coords.Copy(coords4);
		int cascade = 0;
		GLfloat mac = com::Max(fabs(coords.x), fabs(coords.y));
		if(mac > 0.995)
		{
			for(size_t i = 0; i < 3; i++) temp[i] = focalFrag[i] + norm[i] * surfaceBias1.Float();
			temp[3] = 1.0;
			com::Multiply4x4Vec4(temp, focalToSunClips[1], coords4);
			coords.Copy(coords4);
			cascade = 1;
			mac = com::Max(fabs(coords.x), fabs(coords.y));

			if(mac > 0.995)
			{
				for(size_t i = 0; i < 3; i++) temp[i] = focalFrag[i] + norm[i] * surfaceBias2.Float();
				temp[3] = 1.0;
				com::Multiply4x4Vec4(temp, focalToSunClips[2], coords4);
				coords.Copy(coords4);
				cascade = 2;
				mac = com::Max(fabs(coords.x), fabs(coords.y));

				if(mac > 0.995)
				{
					for(size_t i = 0; i < 3; i++) temp[i] = focalFrag[i] + norm[i] * surfaceBias3.Float();
					temp[3] = 1.0;
					com::Multiply4x4Vec4(temp, focalToSunClips[3], coords4);
					coords.Copy(coords4);
					cascade = 3;
					mac = 0.0;
				}
			}
		}
		com::Vec3 cascadeOffsets[4] = {
			com::Vec3(0.25, 0.25, 0.5),
			com::Vec3(0.75, 0.25, 0.5),
			com::Vec3(0.25, 0.75, 0.5),
			com::Vec3(0.75, 0.75, 0.5)
		};
		coords = coords * com::Vec3(0.25, 0.25, 0.5) + cascadeOffsets[cascade];
		unsigned shadowDim = allocCascadeRes * 2;
		bufferSize = shadowDim * shadowDim;
		buffer = new GLfloat[bufferSize];
		texelStart = (size_t)(coords[0] * shadowDim) + (size_t)(coords[1] * shadowDim * shadowDim);
		glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, buffer);
		GLfloat shadowFrag = buffer[texelStart];
		delete[] buffer;
		GLfloat lit = shadowFrag >= coords.z;
		int a = 0;

		{
			com::Vec3 focalVec;
			focalVec.Copy(focal);
			focalVec /= focal[3];
			com::Vec3 world = focalVec + scn::ActiveCamera()->FinalPos();
			com::Vec3 clip = com::Multiply4x4Homogeneous(world, gWorldToClip);
			com::Vec3 view = com::Multiply4x4Homogeneous(world, gWorldToView);
			com::Vec3 viewRay = view / view.x;
			com::Vec3 rayAlt = com::VecRot(viewRay, scn::ActiveCamera()->FinalOri());
			float depthVal = clip.z;
			float linDepth = gViewToClip[11] / (depthVal + depthBias.Float());
			com::Vec3 focalFromRay = rayAlt * linDepth;
			com::Vec3 sunClip = com::Multiply4x4Homogeneous(focalVec, focalToSunClips[0]);
			int a = 0;

			/*
			com::Vec3 world(-1910, -3466, 121.0498046875);
			com::Vec3 focalControl = world - scn::ActiveCamera()->FinalPos();
			com::Vec3 clip = com::Multiply4x4Homogeneous(world, gWorldToClip);
			com::Vec3 focal = com::Multiply4x4Homogeneous(clip, gClipToFocal);
			com::Vec3 view = com::Multiply4x4Homogeneous(world, gWorldToView);
			com::Vec3 viewRay = view / view.x;
			com::Vec3 ray = com::VecRot(viewRay, scn::ActiveCamera()->FinalOri());
			float depthVal = clip.z;
			float linDepth = gViewToClip[11] / (depthVal + depthBias.Float());
			com::Vec3 focalFromRay = ray * linDepth;
			com::Vec3 sunClip = com::Multiply4x4Homogeneous(focal, focalToSunClips[0]);
			*/
		}
	}
#endif

	glUniformMatrix4fv(sunPass.uniFocalToSunClips, 4, GL_FALSE, *focalToSunClips);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Light "relit" overlays
	com::Vec3 camPos = scn::ActiveCamera()->FinalPos();

	for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
	{
		const scn::Overlay& ovr = scn::Overlays()[i];
		const scn::Camera* overCam = ovr.cam;

		if(!overCam || !(ovr.flags & ovr.RELIT))
			continue;

		glUniformMatrix4fv(sunPass.uniClipToFocal, 1, GL_FALSE, gOverlayCTF[i]);

		/* Recalculate cascade's focal-to-clip matrix accounting for overlay cam's position
		relative to active cam so the correct shadow map texel can be retrieved */
		com::Vec3 relPos = camPos - overCam->FinalPos();

		for(size_t i = 0; i < 4; i++)
		{
			FocalToSunClip(pitch, yaw, relPos + centers[i], locks[i], sunViewToClips[i],
				focalToSunClips[i]);
		}

		glUniformMatrix4fv(sunPass.uniFocalToSunClips, 4, GL_FALSE, *focalToSunClips);
		GLuint ref = 1 << (overlayStartBit - i);
		glStencilFunc(GL_EQUAL, ref, ref);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	SunPassCleanup();

	timers[TIMER_SUN_LIGHT_PASS].Stop();
}

/*--------------------------------------
	rnd::InitSunPass
--------------------------------------*/
bool rnd::InitSunPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&sunPass.uniClipToFocal, "clipToFocal"},
		{&sunPass.uniFrustumRatio, "frustumRatio"},
		{&sunPass.uniTopInfluence, "topInfluence"},
		{&sunPass.uniInfluenceFactor, "influenceFactor"},
		{&sunPass.uniSeed, "seed"},
		{&sunPass.uniDepthProj, "depthProj"},
		{&sunPass.uniDepthBias, "depthBias"},
		{&sunPass.uniSurfaceBiases, "surfaceBiases"},
		{&sunPass.uniFocalToSunClips, "focalToSunClips"},
		{&sunPass.uniKernels, "kernels"},
		{&sunPass.uniLitExps, "litExps"},
		{&sunPass.uniPureSegment, "pureSegment"},
		{&sunPass.uniSunDir, "sunDir"},
		{&sunPass.uniSunSubPalette, "sunSubPalette"},
		{&sunPass.uniSunIntensity, "sunIntensity"},
		{&sunPass.uniSunColorSmooth, "sunColorSmooth"},
		{&sunPass.uniSunColorPriority, "sunColorPriority"},
		{&sunPass.samGeometry, "geometry"},
		{&sunPass.samDepth, "depth"},
		{&sunPass.samShadow, "shadow"},
		{0, 0}
	};

	if(!InitProgram("sun", sunPass.shaderProgram, sunPass.vertexShader, &vertBulbLightSource, 1,
	sunPass.fragmentShader, &fragSunSource, 1, attributes, uniforms))
		return false;

	glUseProgram(sunPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(sunPass.samGeometry, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(sunPass.samDepth, RND_VARIABLE_2_TEXTURE_NUM);
	glUniform1i(sunPass.samShadow, RND_VARIABLE_3_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}

/*--------------------------------------
	rnd::FocalToSunClip
--------------------------------------*/
void rnd::FocalToSunClip(float pitch, float yaw, const com::Vec3& center, const com::Vec3& lock,
	const GLfloat (&sunViewToClip)[16], GLfloat (&focalToSunClip)[16])
{
	GLfloat focalToSunView[16];
	WorldToSunView(pitch, yaw, -center, focalToSunView);
	focalToSunView[3] -= lock.x;
	focalToSunView[7] -= lock.y;
	focalToSunView[11] -= lock.z;
	com::Multiply4x4(focalToSunView, sunViewToClip, focalToSunClip);
}

/*
################################################################################################


	SHADOW POINT PASS


################################################################################################
*/

/*--------------------------------------
	rnd::DrawShadowMap

RND_LIGHT_PASS_ATTRIB_POS0 must be enabled.
--------------------------------------*/
void rnd::DrawShadowMap(const scn::Bulb& bulb, GLfloat& depthProjOut, float radius,
	const com::Vec3& pos, uint16_t& overlaysOut)
{
	const float BULB_NEAR = 1.0f;
	static hit::Frustum frustum(COM_PI * 0.25f, COM_PI * 0.25f, BULB_NEAR, BULB_NEAR + 1.0f);

	GLfloat bulbViewToClip[16];

	const GLenum TARGETS[6] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	// FIXME: Can pre-process orientation matrices
	const com::Qua ORIS[6] = {
		{RND_SQRT_HALF, 0.0f, 0.0f, RND_SQRT_HALF},		// Euler: pi / 2, 0, 0
		{0.0f, -RND_SQRT_HALF, RND_SQRT_HALF, 0.0f},	// -pi / 2, 0, pi
		{0.0f, 0.0f, RND_SQRT_HALF, RND_SQRT_HALF},		// 0, 0, pi / 2
		{RND_SQRT_HALF, -RND_SQRT_HALF, 0.0f, 0.0f},	// pi, 0, -pi / 2
		{0.5f, -0.5f, 0.5f, 0.5f},						// pi / 2, -pi / 2, 0
		{0.5f, 0.5f, -0.5f, 0.5f}						// pi / 2, pi / 2, 0
	};

	bool fadeProg = false;
	glUseProgram(shadowPass.shaderProgram);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS1);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM1);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	bool polygonOffset = polyBias.Float() || polySlopeBias.Float();

	if(polygonOffset)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-polySlopeBias.Float(), -polyBias.Float());
	}

	glUniform1f(shadowPass.uniVertexBias, vertexBias.Float());

	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboShadowBuffer);
	else
	{
		glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texShadowBuffer);
	}

	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glViewport(0, 0, allocShadowRes, allocShadowRes);
	ViewToClipPersLimited(1.0f, COM_PI * 0.25f, BULB_NEAR, radius, bulbViewToClip);
	depthProjOut = bulbViewToClip[11];
	frustum.SetFarDist(radius);
	size_t numLitEnts;
	BulbDrawList(bulb, radius, pos, litEnts, numLitEnts, overlaysOut);
	const Mesh* curMsh = 0;
	const Texture* curTex = 0;

	for(size_t i = 0; i < 6; i++)
	{
		if(usingFBOs.Bool())
		{
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, TARGETS[i],
				texShadowBuffer, 0);
		}

		glClear(GL_DEPTH_BUFFER_BIT);
		DrawShadowMapFace(bulb, pos, ORIS[i], bulbViewToClip, frustum, litEnts.o, numLitEnts,
			fadeProg, curMsh, curTex);

		if(!usingFBOs.Bool())
		{
			glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
			glCopyTexSubImage2D(TARGETS[i], 0, 0, 0, 0, 0, allocShadowRes, allocShadowRes);
		}
	}

	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer);

	glViewport(0, 0, wrp::VideoWidth(), wrp::VideoHeight());
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS1);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM0);
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM1);

	if(polygonOffset)
		glDisable(GL_POLYGON_OFFSET_FILL);

	if(curTex)
	{
		glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	}
}

/*--------------------------------------
	rnd::DrawShadowMapFace

FIXME: can prevent clipping nearby entities via depth clamping
--------------------------------------*/
void rnd::DrawShadowMapFace(const scn::Bulb& bulb, const com::Vec3& pos, const com::Qua& ori,
	const GLfloat (&bvtc)[16], const hit::Frustum& frustum, const scn::Entity** litEnts,
	size_t numLitEnts, bool& fadeProg, const Mesh*& curMsh, const Texture*& curTex)
{
	if(fadeProg)
	{
		glUseProgram(shadowPass.shaderProgram);
		glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
		fadeProg = false;
	}

	GLfloat bulbWorldToView[16];
	GLfloat bulbWorldToClip[16];
	GLfloat bulbModelToClip[16];
	WorldToView(pos, ori, bulbWorldToView);
	com::Multiply4x4(bulbWorldToView, bvtc, bulbWorldToClip);
	glUniformMatrix4fv(shadowPass.uniToClip, 1, GL_FALSE, bulbWorldToClip);
	glUniform1f(shadowPass.uniLerp, 0.0f);
	glUniform3f(shadowPass.uniBulbPos, pos.x, pos.y, pos.z);

	glBindBuffer(GL_ARRAY_BUFFER, worldVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldElementBuffer);

	for(size_t j = 0; j < 2; j++)
	{
		glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0 + j, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_world), (void*)0);

		glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_NORM0 + j, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_world), (void*)(sizeof(GLfloat) * 5));
	}

	for(size_t j = 0; j < bulb.NumZoneLinks(); j++)
		WorldDrawZone(bulb.ZoneLinks()[j].obj);

	curMsh = 0;

	for(size_t j = 0; j < numLitEnts; j++)
	{
		const scn::Entity& ent = *litEnts[j];
		const MeshGL* msh = (MeshGL*)ent.Mesh();
		const TextureGL* tex = (TextureGL*)ent.tex.Value();
		hit::Result res = hit::TestHullHull(frustum, pos, pos, ori, msh->Bounds(),
			ent.FinalPos(), ent.FinalOri());

		if(res.contact == res.NONE)
			continue;

		float opacity = ent.FinalOpacity();
		size_t offsets[2];

		if(opacity == 1.0f && !tex->Alpha())
		{
			if(fadeProg)
			{
				glUseProgram(shadowPass.shaderProgram);
				glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
				fadeProg = false;
			}

			SetSharedShadowUniforms(shadowPass, pos, ent, *msh, bulbWorldToClip,
				bulbModelToClip, offsets);
		}
		else
		{
			if(!fadeProg)
			{
				glUseProgram(shadowFadePass.shaderProgram);
				glUniform1f(shadowFadePass.uniVertexBias, vertexBias.Float());
				glUniform1f(shadowFadePass.uniSeed, shaderRandomSeed);
				glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_TEXCOORD);
				fadeProg = true;
			}

			com::Vec2 uv = ent.FinalUV();
			glUniform2f(shadowFadePass.uniTexShift, uv.x, uv.y);
			glUniform1f(shadowFadePass.uniOpacity, opacity);
			SetSharedShadowUniforms(shadowFadePass, pos, ent, *msh, bulbWorldToClip,
				bulbModelToClip, offsets);
		}

		if(curMsh != msh)
		{
			glBindBuffer(GL_ARRAY_BUFFER, msh->vBufName);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, msh->iBufName);
			curMsh = msh;
		}
			
		if(fadeProg && curTex != tex)
		{
			glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
			glBindTexture(GL_TEXTURE_2D, tex->texName);
			curTex = tex;
		}

		for(size_t l = 0; l < 2; l++)
		{
			glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0 + l, 3, GL_FLOAT, GL_FALSE,
				sizeof(vertex_mesh_place), (void*)offsets[l]);

			glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_NORM0 + l, 3, GL_FLOAT, GL_FALSE,
				sizeof(vertex_mesh_place), (void*)(offsets[l] + sizeof(GLfloat) * 3));
		}

		if(fadeProg)
		{
			glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
				sizeof(vertex_mesh_tex), (void*)0);
		}

		ModelDrawElements(*msh);
	}
}

/*--------------------------------------
	rnd::CmpShadowEntities

Sort by opaque/transparent, then mesh, then texture.
--------------------------------------*/
int rnd::CmpShadowEntities(const void* av, const void* bv)
{
	const scn::Entity* a = *(const scn::Entity**)av;
	const scn::Entity* b = *(const scn::Entity**)bv;
	float ao = a->FinalOpacity(), bo = b->FinalOpacity();

	if((ao == 1.0f) != (bo == 1.0f))
		return ao == 1.0f ? -1 : 1;

	const TextureGL* at = (TextureGL*)a->tex.Value();
	const TextureGL* bt = (TextureGL*)b->tex.Value();

	if(at->Alpha() != bt->Alpha())
		return at->Alpha() ? 1 : -1;

	const MeshGL* am = (MeshGL*)a->Mesh();
	const MeshGL* bm = (MeshGL*)b->Mesh();

	if(am->vBufName == bm->vBufName)
	{
		if(at->texName == bt->texName)
			return 0;

		return at->texName < bt->texName ? -1 : 1;
	}
	else
		return am->vBufName < bm->vBufName ? -1 : 1;
}

/*--------------------------------------
	rnd::InitShadowPass
--------------------------------------*/
bool rnd::InitShadowPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos0"},
		{RND_LIGHT_PASS_ATTRIB_POS1, "pos1"},
		{RND_LIGHT_PASS_ATTRIB_NORM0, "norm0"},
		{RND_LIGHT_PASS_ATTRIB_NORM1, "norm1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&shadowPass.uniLerp, "lerp"},
		{&shadowPass.uniToClip, "toClip"},
		{&shadowPass.uniVertexBias, "vertexBias"},
		{&shadowPass.uniBulbPos, "bulbPos"},
		{0, 0}
	};

	if(!InitProgram("shadow", shadowPass.shaderProgram, shadowPass.vertexShader,
	&vertShadowSource, 1, shadowPass.fragmentShader, &fragShadowSource, 1, attributes,
	uniforms))
		return false;

	return true;
}

/*--------------------------------------
	rnd::InitShadowFadePass
--------------------------------------*/
bool rnd::InitShadowFadePass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos0"},
		{RND_LIGHT_PASS_ATTRIB_POS1, "pos1"},
		{RND_LIGHT_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{RND_LIGHT_PASS_ATTRIB_NORM0, "norm0"},
		{RND_LIGHT_PASS_ATTRIB_NORM1, "norm1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&shadowFadePass.uniTexShift, "texShift"},
		{&shadowFadePass.uniLerp, "lerp"},
		{&shadowFadePass.uniToClip, "toClip"},
		{&shadowFadePass.uniVertexBias, "vertexBias"},
		{&shadowFadePass.uniBulbPos, "bulbPos"},
		{&shadowFadePass.uniOpacity, "opacity"},
		{&shadowFadePass.uniSeed, "seed"},
		{&shadowFadePass.samTexture, "texture"},
		{0, 0}
	};

	if(!InitProgram("shadow fade", shadowFadePass.shaderProgram, shadowFadePass.vertexShader,
	&vertShadowFadeSource, 1, shadowFadePass.fragmentShader, &fragShadowFadeSource, 1,
	attributes, uniforms))
		return false;

	glUseProgram(shadowFadePass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(shadowFadePass.samTexture, RND_VARIABLE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}

/*
################################################################################################


	BULB PASS


################################################################################################
*/

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniModelToClip;
} bulbStencilPass = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniTopInfluence, uniInfluenceFactor, uniSeed, uniFade, uniBulbPos, uniBulbInvRadius,
			uniBulbExponent, uniBulbSubPalette, uniBulbIntensity, uniBulbColorSmooth,
			uniBulbColorPriority, uniDepthProj, uniClipToFocal, uniFrustumRatio;
	GLint	samGeometry, samDepth;

	#if DRAW_POINT_SHADOWS
	GLint	uniDepthBias, uniSurfaceBias, uniKernel, uniBulbDepthProj0, uniBulbDepthProj1;
	GLint	samShadow;
	#endif

	bool	firstDraw;
} bulbLightPass = {0};

/*--------------------------------------
	rnd::BulbPassState
--------------------------------------*/
void rnd::BulbPassState()
{
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texDepthBuffer);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
}

/*--------------------------------------
	rnd::BulbPassCleanup
--------------------------------------*/
void rnd::BulbPassCleanup()
{
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
}

/*--------------------------------------
	rnd::BulbDrawVisible
--------------------------------------*/
void rnd::BulbDrawVisible()
{
	for(size_t i = 0; i < numVisBulbs; i++)
		BulbDraw(*visBulbs[i]);
}

/*--------------------------------------
	rnd::BulbDraw
--------------------------------------*/
void rnd::BulbDraw(const scn::Bulb& bulb)
{
	float radius = bulb.FinalRadius();
	float intensity = bulb.FinalIntensity();
	com::Vec3 pos = bulb.FinalPos();
	uint16_t overlays = 0;

#if DRAW_POINT_SHADOWS
	timers[TIMER_BULB_SHADOW_PASS].Start();

	GLfloat bulbDepthProj0, bulbDepthProj1;
	DrawShadowMap(bulb, bulbDepthProj0, bulbDepthProj1, radius, pos, overlays);

	if(!usingFBOs.Bool())
		RestoreDepthBuffer();

	timers[TIMER_BULB_SHADOW_PASS].Stop();
	timers[TIMER_BULB_LIGHT_PASS].Start();
#else
	timers[TIMER_BULB_LIGHT_PASS].Start();

	BulbLitFlags(bulb, radius, pos, overlays);
#endif

	glDepthMask(GL_FALSE);
	DrawBulbStencil(lightSphere, radius, pos, com::QUA_IDENTITY);
	glUseProgram(bulbLightPass.shaderProgram);

#if DRAW_POINT_SHADOWS
	glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texShadowBuffer);
#endif

	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, 1, overlayMask ^ ~overlayRelitMask & ~cloudMask);
		// Only draw where light is effective
		// Non-relit overlay bits are masked 0 so they don't fail the bulb stencil check
		// Same with cloud bit
		// Relit and sky bits are masked 1 so they automatically fail the bulb stencil check, being over 1

	if(bulbLightPass.firstDraw)
	{
		bulbLightPass.firstDraw = false;
		glUniform1f(bulbLightPass.uniTopInfluence, topInfluence);
		glUniform1f(bulbLightPass.uniInfluenceFactor, influenceFactor);
		glUniform1f(bulbLightPass.uniDepthProj, gViewToClip[11]);
		glUniform1f(bulbLightPass.uniFrustumRatio, 1.0f / nearClip.Float());
		glUniformMatrix4fv(bulbLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);

		#if DRAW_POINT_SHADOWS
		glUniform1f(bulbLightPass.uniDepthBias, depthBias.Float());
		glUniform1f(bulbLightPass.uniSurfaceBias, surfaceBias.Float());
		glUniform1f(bulbLightPass.uniKernel, kernelSize.Float());
		#endif
	}

	glUniform1f(bulbLightPass.uniSeed, RandomSeed());
	glUniform1f(bulbLightPass.uniFade, bulb.subPalette ? (light16.Bool() ? lightFade.Float() : lightFadeLow.Float()) : 0.0f);
	com::Vec3 focalPos = pos - scn::ActiveCamera()->FinalPos();
	glUniform3f(bulbLightPass.uniBulbPos, focalPos.x, focalPos.y, focalPos.z);
	glUniform1f(bulbLightPass.uniBulbInvRadius, 1.0f / radius);
	glUniform1f(bulbLightPass.uniBulbExponent, bulb.FinalExponent());
	glUniform1f(bulbLightPass.uniBulbSubPalette, PackedLightSubPalette(bulb.subPalette));
	glUniform1f(bulbLightPass.uniBulbIntensity, NormalizedIntensity(intensity));
	glUniform1f(bulbLightPass.uniBulbColorSmooth, bulb.colorSmooth);
	glUniform1f(bulbLightPass.uniBulbColorPriority, bulb.colorPriority);

	#if DRAW_POINT_SHADOWS
	glUniform1f(bulbLightPass.uniBulbDepthProj0, bulbDepthProj0);
	glUniform1f(bulbLightPass.uniBulbDepthProj1, bulbDepthProj1);
	#endif

	glDrawArrays(GL_TRIANGLES, 0, 3);

	if(overlays)
	{
		// Light overlays based on their real-world placement, not on-screen placement
		bool changed = false;

		for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
		{
			const scn::Overlay& ovr = scn::Overlays()[i];
			const scn::Camera* overCam = ovr.cam;

			if(!overCam || !(ovr.flags & ovr.RELIT))
				continue;

			changed = true;
			glUniformMatrix4fv(bulbLightPass.uniClipToFocal, 1, GL_FALSE, gOverlayCTF[i]);
			com::Vec3 overFPos = pos - overCam->FinalPos();
			glUniform3f(bulbLightPass.uniBulbPos, overFPos.x, overFPos.y, overFPos.z);
			GLuint ref = 1 << (overlayStartBit - i);
			glStencilFunc(GL_EQUAL, ref, ref);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		if(changed)
			glUniformMatrix4fv(bulbLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	}

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);

	timers[TIMER_BULB_LIGHT_PASS].Stop();
}

/*--------------------------------------
	rnd::BulbStencilUpdateMatrices
--------------------------------------*/
void rnd::BulbStencilUpdateMatrices(float radius, const com::Vec3& pos, const com::Qua& ori)
{
	GLfloat modelToWorld[16];

	if(ori.Identity())
	{
		modelToWorld[0] = radius;	modelToWorld[1] = 0.0f;		modelToWorld[2] = 0.0f;		modelToWorld[3] = pos.x;
		modelToWorld[4] = 0.0f;		modelToWorld[5] = radius;	modelToWorld[6] = 0.0f;		modelToWorld[7] = pos.y;
		modelToWorld[8] = 0.0f;		modelToWorld[9] = 0.0f;		modelToWorld[10] = radius;	modelToWorld[11] = pos.z;
		modelToWorld[12] = 0.0f;	modelToWorld[13] = 0.0f;	modelToWorld[14] = 0.0f;	modelToWorld[15] = 1.0f;
	}
	else
	{
		GLfloat matRotate[16];
		com::MatQua(ori, matRotate);
		GLfloat matPosScale[16];
		matPosScale[0] = radius;	matPosScale[1] = 0.0f;		matPosScale[2] = 0.0f;		matPosScale[3] = pos.x;
		matPosScale[4] = 0.0f;		matPosScale[5] = radius;	matPosScale[6] = 0.0f;		matPosScale[7] = pos.y;
		matPosScale[8] = 0.0f;		matPosScale[9] = 0.0f;		matPosScale[10] = radius;	matPosScale[11] = pos.z;
		matPosScale[12] = 0.0f;		matPosScale[13] = 0.0f;		matPosScale[14] = 0.0f;		matPosScale[15] = 1.0f;
		com::Multiply4x4(matRotate, matPosScale, modelToWorld);
	}

	GLfloat modelToClip[16];
	com::Multiply4x4(modelToWorld, gWorldToClip, modelToClip);
	glUniformMatrix4fv(bulbStencilPass.uniModelToClip, 1, GL_FALSE, modelToClip);
}

/*--------------------------------------
	rnd::DrawBulbStencil

Note: bulbs don't light up sky because glDepthFunc is GL_LESS and the sky is at depth 1.0.

FIXME: Try to skip stencilling, create frags with sphere, and discard based on depth in shader
	Want to see if this helps integrated GPU
--------------------------------------*/
void rnd::DrawBulbStencil(const simple_mesh* mesh, float radius, const com::Vec3& pos,
	const com::Qua& ori)
{
	glUseProgram(bulbStencilPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vBufName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->iBufName);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glClear(GL_STENCIL_BUFFER_BIT);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 0, -1);
	glEnable(GL_DEPTH_CLAMP); // So light isn't clipped when far side is clipped by far plane
	BulbStencilUpdateMatrices(radius, pos, ori);

	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_simple),
		(void*)0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);

#if 0
	glCullFace(GL_FRONT);
	glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	glDrawElements(GL_TRIANGLES, lightSphere->numFrameIndices, lightSphere->indexType, 0);

	glCullFace(GL_BACK);
	glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	glDrawElements(GL_TRIANGLES, lightSphere->numFrameIndices, lightSphere->indexType, 0);
#else
	glDisable(GL_CULL_FACE);
	glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
	glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
	glDrawElements(GL_TRIANGLES, lightSphere->numIndices, lightSphere->indexType, 0);
	glEnable(GL_CULL_FACE);
#endif

	glDisable(GL_DEPTH_CLAMP);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

/*--------------------------------------
	rnd::BulbPass
--------------------------------------*/
void rnd::BulbPass()
{
	bulbLightPass.firstDraw = true;
	BulbPassState();
	BulbDrawVisible();
	BulbPassCleanup();
}

/*--------------------------------------
	rnd::InitBulbStencilPass
--------------------------------------*/
bool rnd::InitBulbStencilPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&bulbStencilPass.uniModelToClip, "modelToClip"},
		{0, 0}
	};

	if(!InitProgram("bulb stencil", bulbStencilPass.shaderProgram, bulbStencilPass.vertexShader,
	&vertBulbStencilSource, 1, bulbStencilPass.fragmentShader, &fragBulbStencilSource, 1,
	attributes, uniforms))
		return false;

	return true;
}

/*--------------------------------------
	rnd::InitBulbLightPass
--------------------------------------*/
bool rnd::InitBulbLightPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&bulbLightPass.uniTopInfluence, "topInfluence"},
		{&bulbLightPass.uniInfluenceFactor, "influenceFactor"},
		{&bulbLightPass.uniSeed, "seed"},
		{&bulbLightPass.uniFade, "fade"},
		{&bulbLightPass.uniBulbPos, "bulbPos"},
		{&bulbLightPass.uniBulbInvRadius, "bulbInvRadius"},
		{&bulbLightPass.uniBulbExponent, "bulbExponent"},
		{&bulbLightPass.uniBulbSubPalette, "bulbSubPalette"},
		{&bulbLightPass.uniBulbIntensity, "bulbIntensity"},
		{&bulbLightPass.uniBulbColorSmooth, "bulbColorSmooth"},
		{&bulbLightPass.uniBulbColorPriority, "bulbColorPriority"},
		{&bulbLightPass.uniDepthProj, "depthProj"},
		{&bulbLightPass.uniClipToFocal, "clipToFocal"},
		{&bulbLightPass.uniFrustumRatio, "frustumRatio"},
		{&bulbLightPass.samGeometry, "geometry"},
		{&bulbLightPass.samDepth, "depth"},

		#if DRAW_POINT_SHADOWS
		{&bulbLightPass.uniDepthBias, "depthBias"},
		{&bulbLightPass.uniSurfaceBias, "surfaceBias"},
		{&bulbLightPass.uniKernel, "kernel"},
		{&bulbLightPass.uniBulbDepthProj0, "bulbDepthProj0"},
		{&bulbLightPass.uniBulbDepthProj1, "bulbDepthProj1"},
		{&bulbLightPass.samShadow, "shadow"},
		#endif

		{0, 0}
	};

	if(!InitProgram("bulb light", bulbLightPass.shaderProgram, bulbLightPass.vertexShader,
	&vertBulbLightSource, 1, bulbLightPass.fragmentShader, &fragBulbLightSource, 1, attributes,
	uniforms))
		return false;

	glUseProgram(bulbLightPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(bulbLightPass.samGeometry, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(bulbLightPass.samDepth, RND_VARIABLE_2_TEXTURE_NUM);

	#if DRAW_POINT_SHADOWS
	glUniform1i(bulbLightPass.samShadow, RND_VARIABLE_3_TEXTURE_NUM);
	#endif

	// Reset
	glUseProgram(0);

	return true;
}

/*
################################################################################################


	LIGHT PASS


################################################################################################
*/

/*--------------------------------------
	rnd::LightPassState
--------------------------------------*/
void rnd::LightPassState()
{
	glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthFunc(GL_GREATER);
	glStencilMask(~(skyMask | overlayMask | cloudMask)); // Preserve specific stencil bits
}

/*--------------------------------------
	rnd::LightPassCleanup
--------------------------------------*/
void rnd::LightPassCleanup()
{
	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glStencilMask(-1);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

/*--------------------------------------
	rnd::LightPass

OUTPUT:
* COLOR_BUFFER: Light buffer
--------------------------------------*/
void rnd::LightPass(bool drawSun)
{
	LightPassState();
	AmbientPass();
	SunPass(drawSun);
	SpotPass();
	BulbPass();
	LightPassCleanup();
}

/*--------------------------------------
	rnd::InitLightPass
--------------------------------------*/
bool rnd::InitLightPass()
{
	return InitAmbientPass() && InitShadowSunPass() && InitShadowSunFadePass() &&
		InitSunPass() && InitSpotLightPass() && InitShadowPass() && InitShadowFadePass() &&
		InitBulbStencilPass() && InitBulbLightPass();
}