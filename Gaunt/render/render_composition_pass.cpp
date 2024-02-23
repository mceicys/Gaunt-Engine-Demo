// render_composition_pass.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"
#include "../console/console.h"
#include "../wrap/wrap.h"

#define COMP_PASS_ATTRIB_POS 0

namespace rnd
{
	/*======================================
		rnd::CloudCompositionPassClass
	======================================*/
	static class CloudCompositionPassClass
	{
	public:
		GLuint	vertexShader, fragmentShader, shaderProgram;
		GLint	uniLerp, uniModelToClip, uniModelToNormal, uniModelCam, uniModelToVoxel,
				uniVoxelInvDims, uniVoxelScale, uniVidInv, uniInvInfluenceFactor,
				uniUnpackFactor, uniMaxIntensity, uniRampDistScale, uniExposure, uniGamma,
				uniMidCurveSegment, uniDitherRandom, uniDitherFactor, uniGrainScale, uniPattern,
				uniSeed;
		GLint	samVoxels, samGeometry, samLight, samSubPalettes, samRamps, samRampLookup,
				samLightCurve;

		void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2],
			float lerp, const GLfloat (&wtc)[16], const com::Qua* normOri)
		{
			ModelSetPlaceVertexAttribPointers(*ent.Mesh(), offsets);
			ModelUploadMTCAndMTN(uniModelToClip, uniModelToNormal, ent, wtc, 0);
			ModelUploadLerp(uniLerp, lerp);

			CloudSetVoxelState(uniModelCam, uniModelToVoxel, uniVoxelInvDims, uniVoxelScale,
				ent);
		}

		void UndoStateForEntity(const scn::Entity&) {}
	} cloudCompPass = {};

	void	CompositionPassState();
	void	CompositionPassCleanup();
	GLfloat	FinalExposure();

	// REGULAR COMPOSITION
	void	RegularCompositionPassState();
	void	RegularCompositionPassCleanup();
	bool	InitRegularCompositionPass();

	// CLOUD COMPOSITION
	void	CloudCompositionPassState();
	void	CloudCompositionPassCleanup();
	bool	InitCloudCompositionPass();
}

/*--------------------------------------
	rnd::CompositionPassState

All state set here is constant until the composition pass is done.
--------------------------------------*/
void rnd::CompositionPassState()
{
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texExtraDepthBuffer);
	glActiveTexture(RND_VARIABLE_3_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texLightBuffer);

	if(rgbBlend.Bool())
	{
		glActiveTexture(RND_RAMP_TEXTURE_UNIT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

/*--------------------------------------
	rnd::CompositionPassCleanup
--------------------------------------*/
void rnd::CompositionPassCleanup()
{
	if(rgbBlend.Bool())
	{
		glActiveTexture(RND_RAMP_TEXTURE_UNIT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

/*--------------------------------------
	rnd::FinalExposure
--------------------------------------*/
GLfloat rnd::FinalExposure()
{
	return exposure.Float() +
		(exposure.Float() * exposureRandom.Float() * (shaderRandomSeed - 1.0f));
}

/*--------------------------------------
	rnd::CompositionPass

OUTPUT:
* COLOR_BUFFER: Scene RGB, Luma (for FXAA)
--------------------------------------*/
void rnd::CompositionPass()
{
	timers[TIMER_COMPOSITION_PASS].Start();

	CompositionPassState();
	bool composingClouds = numVisCloudEnts != 0;

	if(composingClouds)
	{
		glEnable(GL_STENCIL_TEST);

		CloudCompositionPassState();
		const Mesh* curMsh = 0; const Texture* curTex = 0;

		ModelDrawArray<0, false>(cloudCompPass, gWorldToClip, 0, visCloudEnts.o, numVisCloudEnts,
			curMsh, curTex);

		CloudCompositionPassCleanup();
	}
	else
		glDisable(GL_STENCIL_TEST);

	RegularCompositionPassState();
	glDrawArrays(GL_TRIANGLES, 0, 3);
	RegularCompositionPassCleanup();

	if(composingClouds)
		glDisable(GL_STENCIL_TEST);

	CompositionPassCleanup();

	timers[TIMER_COMPOSITION_PASS].Stop();
}

/*--------------------------------------
	rnd::InitCompositionPass
--------------------------------------*/
bool rnd::InitCompositionPass()
{
	if(!InitRegularCompositionPass())
	{
		con::AlertF("Regular composition pass initialization failed");
		return false;
	}

	if(!InitCloudCompositionPass())
	{
		con::AlertF("Cloud composition pass initialization failed");
		return false;
	}

	return true;
}

/*
################################################################################################


	REGULAR COMPOSITION


################################################################################################
*/

static struct
{
	GLuint			vertexShader, fragmentShader, shaderProgram;
	GLint			uniClipToFocal, uniFrustumRatio, uniDepthProj, uniInvInfluenceFactor,
					uniUnpackFactor, uniMaxIntensity, uniRampDistScale, uniExposure, uniGamma,
					uniMidCurveSegment, uniDitherRandom, uniDitherGrain, uniDitherFactor,
					uniGrainScale, uniPattern, uniSeed, uniRampAdd, uniAdditiveDenormalization,
					uniSunPos, uniFogCenterFocal, uniFogRadius, uniFogThinRadius,
					uniFogThinExponent, uniFogGeoSubStartDist, uniFogGeoSubHalfDist,
					uniFogGeoSubFactor, uniFogGeoSubCoord, uniFogFadeStartDist,
					uniFogFadeHalfDist, uniFogFadeAmount, uniFogFadeRampPos,
					uniFogFadeRampPosSun, uniFogAddStartDist, uniFogAddHalfDist,
					uniFogAddAmount, uniFogAddAmountSun, uniFogSunRadius, uniFogSunExponent;
	GLint			samGeometry, samDepth, samLight, samSubPalettes, samRamps, samRampLookup,
					samLightCurve;
} compPass = {0};

/*--------------------------------------
	rnd::RegularCompositionPassState
--------------------------------------*/
void rnd::RegularCompositionPassState()
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glStencilFunc(GL_EQUAL, 0, 1);
	glUseProgram(compPass.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glEnableVertexAttribArray(COMP_PASS_ATTRIB_POS);

	glVertexAttribPointer(COMP_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(screen_vertex),
		(void*)0);

	// Update uniforms
	const PaletteGL* curPal = CurrentPaletteGL();
	glUniform1f(compPass.uniFrustumRatio, 1.0f / nearClip.Float());
	glUniformMatrix4fv(compPass.uniClipToFocal, 1, GL_FALSE, gClipToFocal);
	glUniform1f(compPass.uniDepthProj, gViewToClip[11]);
	glUniform1f(compPass.uniSeed, shaderRandomSeed);
	glUniform1f(compPass.uniMaxIntensity, CurrentMaxIntensity());
	glUniform1f(compPass.uniInvInfluenceFactor, 1.0f / influenceFactor);
	glUniform1f(compPass.uniUnpackFactor, unpackFactor);
	glUniform1f(compPass.uniRampDistScale, curPal->rampDistScale);
	glUniform1f(compPass.uniExposure, FinalExposure());
	glUniform1f(compPass.uniGamma, lightGamma.Float());
	glUniform1f(compPass.uniMidCurveSegment, 0.5f / numCurveSegments.Float());
	glUniform1f(compPass.uniDitherRandom, ditherRandom.Float());
	glUniform1f(compPass.uniDitherGrain, ditherGrain.Float());
	glUniform1f(compPass.uniDitherFactor, ditherAmount.Float() * curPal->invNumRampTexels);
	glUniform1f(compPass.uniGrainScale, 1.0f / ditherGrainSize.Float());
	glUniform1f(compPass.uniRampAdd, rampAdd.Float() * curPal->invNumRampTexels);
	float rampVariance = RampVariance();
	float maxRampSpan = curPal->MaxRampSpan();
	glUniform1f(compPass.uniAdditiveDenormalization, (maxRampSpan + rampVariance) / maxRampSpan);
	com::Vec3 sunPosNorm = scn::sun.FinalPos().Normalized();
	glUniform3f(compPass.uniSunPos, sunPosNorm.x, sunPosNorm.y, sunPosNorm.z);
	const scn::Fog& fog = scn::fog;
	com::Vec3 camPos = scn::ActiveCamera()->FinalPos();
	com::Vec3 fogCenterFocal = fog.center - camPos;
	glUniform3f(compPass.uniFogCenterFocal, fogCenterFocal.x, fogCenterFocal.y, fogCenterFocal.z);
	glUniform1f(compPass.uniFogRadius, fog.radius);
	glUniform1f(compPass.uniFogThinRadius, fog.thinRadius);
	glUniform1f(compPass.uniFogThinExponent, fog.thinExponent);
	glUniform1f(compPass.uniFogGeoSubStartDist, fog.geoSubStartDist);
	glUniform1f(compPass.uniFogGeoSubHalfDist, fog.geoSubHalfDist);
	glUniform1f(compPass.uniFogGeoSubFactor, fog.geoSubFactor);
	glUniform1f(compPass.uniFogGeoSubCoord, NormalizeSubPalette(fog.geoSubPalette));
	glUniform1f(compPass.uniFogFadeStartDist, fog.fadeStartDist);
	glUniform1f(compPass.uniFogFadeHalfDist, fog.fadeHalfDist);
	glUniform1f(compPass.uniFogFadeAmount, fog.fadeAmount);
	glUniform1f(compPass.uniFogFadeRampPos, fog.fadeRampPos);
	glUniform1f(compPass.uniFogFadeRampPosSun, fog.fadeRampPosSun);
	glUniform1f(compPass.uniFogAddStartDist, fog.addStartDist);
	glUniform1f(compPass.uniFogAddHalfDist, fog.addHalfDist);
	glUniform1f(compPass.uniFogAddAmount, fog.addAmount);
	glUniform1f(compPass.uniFogAddAmountSun, fog.addAmountSun);
	glUniform1f(compPass.uniFogSunRadius, fog.sunRadius);
	glUniform1f(compPass.uniFogSunExponent, fog.sunExponent);
}

/*--------------------------------------
	rnd::RegularCompositionPassCleanup
--------------------------------------*/
void rnd::RegularCompositionPassCleanup()
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisableVertexAttribArray(COMP_PASS_ATTRIB_POS);
}

/*--------------------------------------
	rnd::InitRegularCompositionPass
--------------------------------------*/
bool rnd::InitRegularCompositionPass()
{
	prog_attribute attributes[] =
	{
		{COMP_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&compPass.uniClipToFocal, "clipToFocal"},
		{&compPass.uniFrustumRatio, "frustumRatio"},
		{&compPass.uniDepthProj, "depthProj"},
		{&compPass.uniInvInfluenceFactor, "invInfluenceFactor"},
		{&compPass.uniUnpackFactor, "unpackFactor"},
		{&compPass.uniMaxIntensity, "maxIntensity"},
		{&compPass.uniRampDistScale, "rampDistScale"},
		{&compPass.uniExposure, "exposure"},
		{&compPass.uniGamma, "gamma"},
		{&compPass.uniMidCurveSegment, "midCurveSegment"},
		{&compPass.uniDitherRandom, "ditherRandom"},
		{&compPass.uniDitherGrain, "ditherGrain"},
		{&compPass.uniDitherFactor, "ditherFactor"},
		{&compPass.uniGrainScale, "grainScale"},
		{&compPass.uniPattern, "pattern"},
		{&compPass.uniSeed, "seed"},
		{&compPass.uniRampAdd, "rampAdd"},
		{&compPass.uniAdditiveDenormalization, "additiveDenormalization"},
		{&compPass.uniSunPos, "sunPos"},
		{&compPass.uniFogCenterFocal, "fogCenterFocal"},
		{&compPass.uniFogRadius, "fogRadius"},
		{&compPass.uniFogThinRadius, "fogThinRadius"},
		{&compPass.uniFogThinExponent, "fogThinExponent"},
		{&compPass.uniFogGeoSubStartDist, "fogGeoSubStartDist"},
		{&compPass.uniFogGeoSubHalfDist, "fogGeoSubHalfDist"},
		{&compPass.uniFogGeoSubFactor, "fogGeoSubFactor"},
		{&compPass.uniFogGeoSubCoord, "fogGeoSubCoord"},
		{&compPass.uniFogFadeStartDist, "fogFadeStartDist"},
		{&compPass.uniFogFadeHalfDist, "fogFadeHalfDist"},
		{&compPass.uniFogFadeAmount, "fogFadeAmount"},
		{&compPass.uniFogFadeRampPos, "fogFadeRampPos"},
		{&compPass.uniFogFadeRampPosSun, "fogFadeRampPosSun"},
		{&compPass.uniFogAddStartDist, "fogAddStartDist"},
		{&compPass.uniFogAddHalfDist, "fogAddHalfDist"},
		{&compPass.uniFogAddAmount, "fogAddAmount"},
		{&compPass.uniFogAddAmountSun, "fogAddAmountSun"},
		{&compPass.uniFogSunRadius, "fogSunRadius"},
		{&compPass.uniFogSunExponent, "fogSunExponent"},
		{&compPass.samGeometry, "geometry"},
		{&compPass.samDepth, "depth"},
		{&compPass.samLight, "light"},
		{&compPass.samSubPalettes, "subPalettes"},
		{&compPass.samRamps, "ramps"},
		{&compPass.samRampLookup, "rampLookup"},
		{&compPass.samLightCurve, "lightCurve"},
		{0, 0}
	};

	if(!InitProgram("composition", compPass.shaderProgram, compPass.vertexShader,
	&vertCompSource, 1, compPass.fragmentShader, &fragCompSource, 1, attributes, uniforms))
		return false;

	glUseProgram(compPass.shaderProgram); // RESET

	// Set constant uniforms
	GLfloat dither[16];
	MakeSignedDitherArray(dither);
	glUniform1fv(compPass.uniPattern, 16, dither);
	glUniform1i(compPass.samGeometry, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(compPass.samDepth, RND_VARIABLE_2_TEXTURE_NUM);
	glUniform1i(compPass.samLight, RND_VARIABLE_3_TEXTURE_NUM);
	glUniform1i(compPass.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);
	glUniform1i(compPass.samRamps, RND_RAMP_TEXTURE_NUM);
	glUniform1i(compPass.samRampLookup, RND_RAMP_LOOKUP_TEXTURE_NUM);
	glUniform1i(compPass.samLightCurve, RND_CURVE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}

/*
################################################################################################


	CLOUD COMPOSITION


################################################################################################
*/

/*--------------------------------------
	rnd::CloudCompositionPassState
--------------------------------------*/
void rnd::CloudCompositionPassState()
{
	glUseProgram(cloudCompPass.shaderProgram);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL);

	// FIXME FIXME: Depth offset fixes the z problem but variance in the triangles can also create different 2D frags, revealing some unblended frags to the user
		// This needs to be fixed at the shader level; generated frags and their coords must be exactly the same in both stages
			// Expressions of relevant variables must be identical and only use 'invariant' tagged inputs
			// Note that alpha and stipple discard must be calculated identically in both stages too

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0, 0.0); // Avoid z-artifacts from drawing same cloud geometry
		// FIXME: negated this after changing depth to reverse depth but haven't tested yet

	// Set a stencil bit so regular pass can ignore clouds
	glStencilMask(1);
	glClear(GL_STENCIL_BUFFER_BIT); // FIXME: Is this necessary?
	glStencilFunc(GL_ALWAYS, 1, 1);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS0);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS1);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM0);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM1);

	// Update uniforms
	// FIXME: precalculate uniforms shared by comp and cloud-comp in CompositionPass()
	const PaletteGL* curPal = CurrentPaletteGL();
	glUniform2f(cloudCompPass.uniVidInv, 1.0f / wrp::VideoWidth(), 1.0f / wrp::VideoHeight());
	glUniform1f(cloudCompPass.uniSeed, shaderRandomSeed);
	glUniform1f(cloudCompPass.uniMaxIntensity, CurrentMaxIntensity());
	glUniform1f(cloudCompPass.uniInvInfluenceFactor, 1.0f / influenceFactor);
	glUniform1f(cloudCompPass.uniUnpackFactor, unpackFactor);
	glUniform1f(cloudCompPass.uniRampDistScale, curPal->rampDistScale);
	glUniform1f(cloudCompPass.uniExposure, FinalExposure());
	glUniform1f(cloudCompPass.uniGamma, lightGamma.Float());
	glUniform1f(cloudCompPass.uniMidCurveSegment, 0.5f / numCurveSegments.Float());
	glUniform1f(cloudCompPass.uniDitherRandom, ditherRandom.Float());
	glUniform1f(cloudCompPass.uniDitherFactor, ditherAmount.Float() * curPal->invNumRampTexels);
	glUniform1f(cloudCompPass.uniGrainScale, 1.0f / ditherGrainSize.Float());
}

/*--------------------------------------
	rnd::CloudCompositionPassCleanup
--------------------------------------*/
void rnd::CloudCompositionPassCleanup()
{
	glDisable(GL_POLYGON_OFFSET_FILL);

	glStencilMask(-1);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS0);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS1);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM0);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM1);
}

/*--------------------------------------
	rnd::InitCloudCompositionPass
--------------------------------------*/
bool rnd::InitCloudCompositionPass()
{
	prog_attribute attributes[] =
	{
		{RND_MODEL_PASS_ATTRIB_POS0, "pos0"},
		{RND_MODEL_PASS_ATTRIB_POS1, "pos1"},
		{RND_MODEL_PASS_ATTRIB_NORM0, "norm0"},
		{RND_MODEL_PASS_ATTRIB_NORM1, "norm1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&cloudCompPass.uniLerp, "lerp"},
		{&cloudCompPass.uniModelToClip, "modelToClip"},
		{&cloudCompPass.uniModelToNormal, "modelToNormal"},
		{&cloudCompPass.uniModelCam, "modelCam"},
		{&cloudCompPass.uniModelToVoxel, "modelToVoxel"},
		{&cloudCompPass.uniVoxelInvDims, "voxelInvDims"},
		{&cloudCompPass.uniVoxelScale, "voxelScale"},
		{&cloudCompPass.samVoxels, "voxels"},
		{&cloudCompPass.uniVidInv, "vidInv"},
		{&cloudCompPass.uniInvInfluenceFactor, "invInfluenceFactor"},
		{&cloudCompPass.uniUnpackFactor, "unpackFactor"},
		{&cloudCompPass.uniMaxIntensity, "maxIntensity"},
		{&cloudCompPass.uniRampDistScale, "rampDistScale"},
		{&cloudCompPass.uniExposure, "exposure"},
		{&cloudCompPass.uniGamma, "gamma"},
		{&cloudCompPass.uniMidCurveSegment, "midCurveSegment"},
		{&cloudCompPass.uniDitherRandom, "ditherRandom"},
		{&cloudCompPass.uniDitherFactor, "ditherFactor"},
		{&cloudCompPass.uniGrainScale, "grainScale"},
		{&cloudCompPass.uniPattern, "pattern"},
		{&cloudCompPass.uniSeed, "seed"},
		{&cloudCompPass.samGeometry, "geometry"},
		{&cloudCompPass.samLight, "light"},
		{&cloudCompPass.samSubPalettes, "subPalettes"},
		{&cloudCompPass.samRamps, "ramps"},
		{&cloudCompPass.samRampLookup, "rampLookup"},
		{&cloudCompPass.samLightCurve, "lightCurve"},
		{0, 0}
	};

	if(!InitProgram("cloud composition", cloudCompPass.shaderProgram,
	cloudCompPass.vertexShader, &vertCloudCompSource, 1, cloudCompPass.fragmentShader,
	&fragCloudCompSourceHigh, 1, attributes, uniforms))
		return false;

	glUseProgram(cloudCompPass.shaderProgram); // RESET

	// Set constant uniforms
	GLfloat dither[16];
	MakeSignedDitherArray(dither);
	glUniform1i(cloudCompPass.samVoxels, RND_VARIABLE_2_TEXTURE_NUM);
	glUniform1fv(cloudCompPass.uniPattern, 16, dither);
	glUniform1i(cloudCompPass.samGeometry, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(cloudCompPass.samLight, RND_VARIABLE_3_TEXTURE_NUM);
	glUniform1i(cloudCompPass.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);
	glUniform1i(cloudCompPass.samRamps, RND_RAMP_TEXTURE_NUM);
	glUniform1i(cloudCompPass.samRampLookup, RND_RAMP_LOOKUP_TEXTURE_NUM);
	glUniform1i(cloudCompPass.samLightCurve, RND_CURVE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}