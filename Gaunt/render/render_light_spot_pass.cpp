// render_light_spot_pass.cpp
// Martynas Ceicys

#include "render_private.h"
#include "../hit/hit.h"

namespace rnd
{
	// Data kept between shadow pass and light pass
	struct spot_batch_state
	{
		GLfloat spotViewToClip[16];
		uint16_t overlays;
	};

	// SPOT SHADOW PASS
	void	DrawSpotShadowMap(const scn::Bulb& spot, GLint x, GLint y, float radius,
			const com::Vec3& pos, const com::Qua& ori, float outer,
			GLfloat (&spotViewToClipOut)[16], uint16_t& overlaysOut, bool& fadeProgIO,
			const Texture*& curTexIO);
	void	DrawSpotShadowBatch(const scn::Bulb** spots, spot_batch_state* states,
			size_t numSpots);
	void	ShadowMapViewportOffset(size_t index, GLint& xOut, GLint& yOut);
	void	ShadowMapCoordsOffset(size_t index, GLfloat& xOut, GLfloat& yOut);

	// SPOT LIGHT PASS
	void	DrawSpotLight(const scn::Bulb& spot);

	#if DRAW_POINT_SHADOWS
	void	DrawShadowedSpotLight(const scn::Bulb& spot, GLfloat coordX, GLfloat coordY,
			const GLfloat (&spotViewToClip)[16], uint16_t overlays);
	void	DrawSpotLightBatch(const scn::Bulb** spots, const spot_batch_state* states,
			size_t numSpots);
	#endif

	void	SpotFocalToClip(const com::Vec3& focalSpotPos, const com::Qua spotOri,
			const GLfloat (&spotViewToClip)[16], GLfloat (&spotFocalToClipOut)[16]);

	// SPOT PASS
	void	SpotPassState();
	void	SpotPassCleanup();
}

/*
################################################################################################


	SPOT SHADOW PASS


################################################################################################
*/

/*--------------------------------------
	rnd::DrawSpotShadowMap
--------------------------------------*/
void rnd::DrawSpotShadowMap(const scn::Bulb& spot, GLint x, GLint y, float radius,
	const com::Vec3& pos, const com::Qua& ori, float outer, GLfloat (&spotViewToClip)[16],
	uint16_t& overlaysOut, bool& fadeProg, const Texture*& curTex)
{
	const float SPOT_NEAR = 1.0f;
	static hit::Frustum frustum(0.0f, 0.0f, SPOT_NEAR, SPOT_NEAR + 1.0f);

	float outerBig = outer + spotShadowAddedAngle.Float();
	outerBig = COM_MIN(outerBig, RND_MAX_SPOT_ANGLE);

	if(usingFBOs.Bool())
		glViewport(x, y, allocShadowRes, allocShadowRes);
	else
		glClear(GL_DEPTH_BUFFER_BIT);

	if(fadeProg)
	{
		fadeProg = false;
		glUseProgram(shadowPass.shaderProgram);
	}

	ViewToClipPersLimited(1.0f, outerBig, SPOT_NEAR, radius, spotViewToClip);
	frustum.Set(outerBig, outerBig, SPOT_NEAR, radius);
	size_t numLitEnts;
	BulbDrawList(spot, radius, pos, litEnts, numLitEnts, overlaysOut);
	const Mesh* curMsh = 0;

	DrawShadowMapFace(spot, pos, ori, spotViewToClip, frustum, litEnts.o, numLitEnts, fadeProg,
		curMsh, curTex);

	if(!usingFBOs.Bool())
	{
		glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 0, 0, allocShadowRes, allocShadowRes);
	}
}

/*--------------------------------------
	rnd::DrawSpotShadowBatch
--------------------------------------*/
void rnd::DrawSpotShadowBatch(const scn::Bulb** spots, spot_batch_state* states,
	size_t numSpots)
{
	timers[TIMER_SPOT_SHADOW_PASS].Start();

	glDepthMask(GL_TRUE);

	if(usingFBOs.Bool())
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboFlatShadowBuffer);
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	else
		glViewport(0, 0, allocShadowRes, allocShadowRes);

	bool fadeProg = false;
	glUseProgram(shadowPass.shaderProgram);
	glUniform1f(shadowPass.uniVertexBias, vertexBiasSpot.Float());
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS1);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM0);
	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_NORM1);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_BLEND);
	bool polygonOffset = polyBiasSpot.Float() || polySlopeBiasSpot.Float();

	if(polygonOffset)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-polySlopeBiasSpot.Float(), -polyBiasSpot.Float());
	}

	const Texture* curTex = 0;

	for(size_t i = 0; i < numSpots; i++)
	{
		GLint x, y;
		ShadowMapViewportOffset(i, x, y);
		const scn::Bulb& spot = *spots[i];
		spot_batch_state& state = states[i];

		DrawSpotShadowMap(spot, x, y, spot.FinalRadius(), spot.FinalPos(), spot.FinalOri(),
			spot.FinalOuter(), state.spotViewToClip, state.overlays, fadeProg, curTex);
	}

	if(curTex)
	{
		glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
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

	timers[TIMER_SPOT_SHADOW_PASS].Stop();
}

/*--------------------------------------
	rnd::ShadowMapViewportOffset
--------------------------------------*/
void rnd::ShadowMapViewportOffset(size_t index, GLint& x, GLint& y)
{
	GLint allocAtlasRes = allocCascadeRes * 2;
	GLint offset = index * allocShadowRes;
	x = offset % allocAtlasRes;
	y = offset / allocAtlasRes * allocShadowRes;
}

/*--------------------------------------
	rnd::ShadowMapCoordsOffset
--------------------------------------*/
void rnd::ShadowMapCoordsOffset(size_t index, GLfloat& x, GLfloat& y)
{
	GLint allocAtlasRes = allocCascadeRes * 2;
	GLint offset = index * allocShadowRes;
	GLfloat factor = 1.0f / allocAtlasRes;
	GLint center = allocShadowRes / 2;
	x = (offset % allocAtlasRes + center) * factor;
	y = (offset / allocAtlasRes * allocShadowRes + center) * factor;
}

/*
################################################################################################


	SPOT LIGHT PASS


################################################################################################
*/

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniClipToFocal, uniFrustumRatio, uniTopInfluence, uniInfluenceFactor, uniSeed,
			uniFade, uniDepthProj, uniSpotPos, uniSpotDir, uniSpotOuter, uniSpotInvPenumbra,
			uniSpotInvRadius, uniSpotExponent, uniSpotSubPalette, uniSpotIntensity,
			uniSpotColorSmooth, uniSpotColorPriority;
	GLint	samGeometry, samDepth;

	#if DRAW_POINT_SHADOWS
	GLint	uniDepthBias, uniSurfaceBias, uniKernel, uniClipClamp, uniCoordsMultiply,
			uniCoordsOffset, uniSpotFocalToClip;
	GLint	samShadow;
	#endif

	bool	firstDraw;
} spotLightPass = {0};

/*--------------------------------------
	rnd::DrawSpotLight
--------------------------------------*/
void rnd::DrawSpotLight(const scn::Bulb& spot)
{
	timers[TIMER_SPOT_LIGHT_PASS].Start();

	float radius = spot.FinalRadius();
	float intensity = spot.FinalIntensity();
	float outer = spot.FinalOuter();
	outer = COM_MIN(outer, RND_MAX_SPOT_ANGLE);
	com::Vec3 pos = spot.FinalPos();
	com::Qua ori = spot.FinalOri();
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	DrawBulbStencil(lightHemi, radius, pos, ori);
	glUseProgram(spotLightPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);

	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, 1, overlayMask ^ ~overlayRelitMask);

	if(spotLightPass.firstDraw)
	{
		spotLightPass.firstDraw = false;
		glUniform1f(spotLightPass.uniFrustumRatio, 1.0f / nearClip.Float());
		glUniform1f(spotLightPass.uniTopInfluence, topInfluence);
		glUniform1f(spotLightPass.uniInfluenceFactor, influenceFactor);
		glUniform1f(spotLightPass.uniDepthProj, gViewToClip[11]);
		glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	}

	glUniform1f(spotLightPass.uniSeed, RandomSeed());
	glUniform1f(spotLightPass.uniFade, spot.subPalette ? (light16.Bool() ? lightFade.Float() : lightFadeLow.Float()) : 0.0f);
	com::Vec3 focalPos = pos - scn::ActiveCamera()->FinalPos();
	glUniform3f(spotLightPass.uniSpotPos, focalPos.x, focalPos.y, focalPos.z);
	com::Vec3 dir = ori.Dir();
	glUniform3f(spotLightPass.uniSpotDir, dir.x, dir.y, dir.z);
	GLfloat cosOuter = cos(outer);
	glUniform1f(spotLightPass.uniSpotOuter, cosOuter);
	GLfloat penumbra = cos(spot.FinalInner()) - cosOuter;

	if(penumbra <= 0.0f)
		glUniform1f(spotLightPass.uniSpotInvPenumbra, FLT_MAX);
	else
		glUniform1f(spotLightPass.uniSpotInvPenumbra, 1.0f / penumbra);

	glUniform1f(spotLightPass.uniSpotInvRadius, 1.0f / radius);
	glUniform1f(spotLightPass.uniSpotExponent, spot.FinalExponent());
	glUniform1f(spotLightPass.uniSpotSubPalette, PackedLightSubPalette(spot.subPalette));
	glUniform1f(spotLightPass.uniSpotIntensity, NormalizedIntensity(intensity));
	glUniform1f(spotLightPass.uniSpotColorSmooth, spot.colorSmooth);
	glUniform1f(spotLightPass.uniSpotColorPriority, spot.colorPriority);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	uint16_t overlays = 0;
	BulbLitFlags(spot, radius, pos, overlays);

	if(overlays)
	{
		bool changed = false;

		for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
		{
			const scn::Overlay& ovr = scn::Overlays()[i];
			const scn::Camera* overCam = ovr.cam;

			if(!overCam || !(ovr.flags & ovr.RELIT))
				continue;

			changed = true;
			glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gOverlayCTF[i]);
			com::Vec3 overFPos = pos - overCam->FinalPos();
			glUniform3f(spotLightPass.uniSpotPos, overFPos.x, overFPos.y, overFPos.z);
			GLuint ref = 1 << (overlayStartBit - i);
			glStencilFunc(GL_EQUAL, ref, ref);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		if(changed)
			glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	}

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);

	timers[TIMER_SPOT_LIGHT_PASS].Stop();
}

#if DRAW_POINT_SHADOWS
/*--------------------------------------
	rnd::DrawShadowedSpotLight
--------------------------------------*/
void rnd::DrawShadowedSpotLight(const scn::Bulb& spot, GLfloat coordX, GLfloat coordY,
	const GLfloat (&spotViewToClip)[16], uint16_t overlays)
{
	float radius = spot.FinalRadius();
	float intensity = spot.FinalIntensity();
	float outer = spot.FinalOuter();
	outer = COM_MIN(outer, RND_MAX_SPOT_ANGLE);
	com::Vec3 pos = spot.FinalPos();
	com::Qua ori = spot.FinalOri();
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	DrawBulbStencil(lightHemi, radius, pos, ori);
	glUseProgram(spotLightPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glVertexAttribPointer(RND_LIGHT_PASS_ATTRIB_POS0, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);

	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, 1, overlayMask ^ ~overlayRelitMask);

	if(spotLightPass.firstDraw)
	{
		spotLightPass.firstDraw = false;
		glUniform1f(spotLightPass.uniFrustumRatio, 1.0f / nearClip.Float());
		glUniform1f(spotLightPass.uniTopInfluence, topInfluence);
		glUniform1f(spotLightPass.uniInfluenceFactor, influenceFactor);
		glUniform1f(spotLightPass.uniDepthProj0, (gViewToClip[8] + 1.0f) * 0.5f);
		glUniform1f(spotLightPass.uniDepthProj1, gViewToClip[11] * 0.5f);
		glUniform1f(spotLightPass.uniDepthBias, depthBias.Float());
		glUniform1f(spotLightPass.uniSurfaceBias, surfaceBiasSpot.Float());

		// Convert kernel size to spot-light shadow map's texture coordinate scale
		GLfloat kernelFactor = (GLfloat)allocShadowRes / (allocCascadeRes * 2.0f);
		glUniform1f(spotLightPass.uniKernel, kernelSizeSpot.Float() * kernelFactor);

		// Clamp clip space shadow coordinate just enough to avoid PCF leaking into other shadow map
		glUniform1f(spotLightPass.uniClipClamp, 0.999f - kernelSizeSpot.Float());

		GLfloat coordsScale = 0.25f * (GLfloat)allocShadowRes / (GLfloat)allocCascadeRes;
		glUniform3f(spotLightPass.uniCoordsMultiply, coordsScale, coordsScale, 0.5f);
		glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	}

	glUniform1f(spotLightPass.uniSeed, RandomSeed());
	glUniform1f(spotLightPass.uniFade, spot.subPalette ? (light16.Bool() ? lightFade.Float() : lightFadeLow.Float()) : 0.0f);
	com::Vec3 focalPos = pos - scn::ActiveCamera()->FinalPos();
	glUniform3f(spotLightPass.uniCoordsOffset, coordX, coordY, 0.5f);
	GLfloat spotFocalToClip[16];
	SpotFocalToClip(focalPos, ori, spotViewToClip, spotFocalToClip);
	glUniformMatrix4fv(spotLightPass.uniSpotFocalToClip, 1, GL_FALSE, spotFocalToClip);
	glUniform3f(spotLightPass.uniSpotPos, focalPos.x, focalPos.y, focalPos.z);
	com::Vec3 dir = ori.Dir();
	glUniform3f(spotLightPass.uniSpotDir, dir.x, dir.y, dir.z);
	GLfloat cosOuter = cos(outer);
	glUniform1f(spotLightPass.uniSpotOuter, cosOuter);
	GLfloat penumbra = cos(spot.FinalInner()) - cosOuter;

	if(penumbra <= 0.0f)
		glUniform1f(spotLightPass.uniSpotInvPenumbra, FLT_MAX);
	else
		glUniform1f(spotLightPass.uniSpotInvPenumbra, 1.0f / penumbra);

	glUniform1f(spotLightPass.uniSpotInvRadius, 1.0f / radius);
	glUniform1f(spotLightPass.uniSpotExponent, spot.FinalExponent());
	glUniform1f(spotLightPass.uniSpotSubPalette, PackedLightSubPalette(spot.subPalette));
	glUniform1f(spotLightPass.uniSpotIntensity, NormalizedIntensity(intensity));
	glUniform1f(spotLightPass.uniSpotColorSmooth, spot.colorSmooth);
	glUniform1f(spotLightPass.uniSpotColorPriority, spot.colorPriority);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	if(overlays)
	{
		bool changed = false;

		for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
		{
			const scn::Overlay& ovr = scn::Overlays()[i];
			const scn::Camera* overCam = ovr.cam;

			if(!overCam || !(ovr.flags & ovr.RELIT))
				continue;

			changed = true;
			glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gOverlayCTF[i]);
			com::Vec3 overFPos = pos - overCam->FinalPos();
			SpotFocalToClip(overFPos, ori, spotViewToClip, spotFocalToClip);
			glUniformMatrix4fv(spotLightPass.uniSpotFocalToClip, 1, GL_FALSE, spotFocalToClip);
			glUniform3f(spotLightPass.uniSpotPos, overFPos.x, overFPos.y, overFPos.z);
			GLuint ref = 1 << (overlayStartBit - i);
			glStencilFunc(GL_EQUAL, ref, ref);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		if(changed)
			glUniformMatrix4fv(spotLightPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	}
}

/*--------------------------------------
	rnd::DrawSpotLightBatch
--------------------------------------*/
void rnd::DrawSpotLightBatch(const scn::Bulb** spots, const spot_batch_state* states,
	size_t numSpots)
{
	timers[TIMER_SPOT_LIGHT_PASS].Start();

	if(!usingFBOs.Bool())
		RestoreDepthBuffer();

	glDepthMask(GL_FALSE);

	for(size_t i = 0; i < numSpots; i++)
	{
		const scn::Bulb& spot = *spots[i];
		const spot_batch_state& state = states[i];
		GLfloat x, y;
		ShadowMapCoordsOffset(i, x, y);
		DrawShadowedSpotLight(spot, x, y, state.spotViewToClip, state.overlays);
	}

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);

	timers[TIMER_SPOT_LIGHT_PASS].Stop();
}
#endif

/*--------------------------------------
	rnd::SpotFocalToClip
--------------------------------------*/
void rnd::SpotFocalToClip(const com::Vec3& focalSpotPos, const com::Qua spotOri,
	const GLfloat (&spotViewToClip)[16], GLfloat (&spotFocalToClip)[16])
{
	GLfloat spotFocalToView[16];
	WorldToView(focalSpotPos, spotOri, spotFocalToView);
	com::Multiply4x4(spotFocalToView, spotViewToClip, spotFocalToClip);
}

/*
################################################################################################


	SPOT PASS


################################################################################################
*/

/*--------------------------------------
	rnd::SpotPassState
--------------------------------------*/
void rnd::SpotPassState()
{
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texDepthBuffer);

	#if DRAW_POINT_SHADOWS
	glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texFlatShadowBuffer);
	#endif

	glEnableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
}

/*--------------------------------------
	rnd::SpotPassCleanup
--------------------------------------*/
void rnd::SpotPassCleanup()
{
	glDisableVertexAttribArray(RND_LIGHT_PASS_ATTRIB_POS0);
}

/*--------------------------------------
	rnd::SpotPass
--------------------------------------*/
void rnd::SpotPass()
{
	if(!numVisSpots)
		return;

	spotLightPass.firstDraw = true;
	SpotPassState();
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);

	#if DRAW_POINT_SHADOWS
	// Batch as many lights that can fit in the flat shadow buffer
	size_t numBatch = (allocCascadeRes * 2) / allocShadowRes;
	numBatch *= numBatch;
	static com::Arr<spot_batch_state> states(16);
	states.Ensure(numBatch);

	for(size_t i = 0; i < numVisSpots; i += numBatch)
	{
		size_t numSpots = COM_MIN(numVisSpots - i, numBatch);
		DrawSpotShadowBatch(visSpots.o + i, states.o, numSpots);
		DrawSpotLightBatch(visSpots.o + i, states.o, numSpots);
	}
	#else
	glDepthMask(GL_FALSE);

	for(size_t i = 0; i < numVisSpots; i++)
		DrawSpotLight(*visSpots[i]);

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);
	#endif

	SpotPassCleanup();
}

/*--------------------------------------
	rnd::InitSpotLightPass
--------------------------------------*/
bool rnd::InitSpotLightPass()
{
	prog_attribute attributes[] =
	{
		{RND_LIGHT_PASS_ATTRIB_POS0, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&spotLightPass.uniClipToFocal, "clipToFocal"},
		{&spotLightPass.uniFrustumRatio, "frustumRatio"},
		{&spotLightPass.uniTopInfluence, "topInfluence"},
		{&spotLightPass.uniInfluenceFactor, "influenceFactor"},
		{&spotLightPass.uniSeed, "seed"},
		{&spotLightPass.uniFade, "fade"},
		{&spotLightPass.uniDepthProj, "depthProj"},
		{&spotLightPass.uniSpotPos, "spotPos"},
		{&spotLightPass.uniSpotDir, "spotDir"},
		{&spotLightPass.uniSpotOuter, "spotOuter"},
		{&spotLightPass.uniSpotInvPenumbra, "spotInvPenumbra"},
		{&spotLightPass.uniSpotInvRadius, "spotInvRadius"},
		{&spotLightPass.uniSpotExponent, "spotExponent"},
		{&spotLightPass.uniSpotSubPalette, "spotSubPalette"},
		{&spotLightPass.uniSpotIntensity, "spotIntensity"},
		{&spotLightPass.uniSpotColorSmooth, "spotColorSmooth"},
		{&spotLightPass.uniSpotColorPriority, "spotColorPriority"},
		{&spotLightPass.samGeometry, "geometry"},
		{&spotLightPass.samDepth, "depth"},

		#if DRAW_POINT_SHADOWS
		{&spotLightPass.uniDepthBias, "depthBias"},
		{&spotLightPass.uniSurfaceBias, "surfaceBias"},
		{&spotLightPass.uniKernel, "kernel"},
		{&spotLightPass.uniClipClamp, "clipClamp"},
		{&spotLightPass.uniCoordsMultiply, "coordsMultiply"},
		{&spotLightPass.uniCoordsOffset, "coordsOffset"},
		{&spotLightPass.uniSpotFocalToClip, "spotFocalToClip"},
		{&spotLightPass.samShadow, "shadow"},
		#endif

		{0, 0}
	};

	if(!InitProgram("spot light", spotLightPass.shaderProgram, spotLightPass.vertexShader,
	&vertBulbLightSource, 1, spotLightPass.fragmentShader, &fragSpotLightSource, 1, attributes,
	uniforms))
		return false;

	glUseProgram(spotLightPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(spotLightPass.samGeometry, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(spotLightPass.samDepth, RND_VARIABLE_2_TEXTURE_NUM);

	#if DRAW_POINT_SHADOWS
	glUniform1i(spotLightPass.samShadow, RND_VARIABLE_3_TEXTURE_NUM);
	#endif

	// Reset
	glUseProgram(0);

	return true;
}