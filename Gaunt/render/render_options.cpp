// render_options.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"

namespace rnd
{
	void ResetGeometryPrograms();
	void ResetGeometryPrograms(con::Option& opt, float set);
	void ResetAntiAliasingProgram();
	void ResetAntiAliasingProgram(con::Option& opt, float set);
	void SetMipMapping(con::Option& opt, float set);
	void SetNearClip(con::Option& opt, float set);
	void SetFarClip(con::Option& opt, float set);
	void SetTextureDimension(con::Option& opt, float set);
	void ResetViewportBool(con::Option& opt, float set);
	void SetMaxIntensity(con::Option& opt, float set);
	void SetNumCurveSegments(con::Option& opt, float set);
	void SetGrainRadius(con::Option& opt, float set);
	void ResetPalette(con::Option& opt, float set);
	void SetLineWidth(con::Option& opt, float set);
	void SetAntiAliasing(con::Option& opt, float set);

	con::Option
		checkErrors("rnd_check_errors", false),
		mipMapping("rnd_mip_mapping", true, SetMipMapping), // FIXME: mipmaps with brights look bad
		mipBias("rnd_mip_bias", 0.7f),
		mipFade("rnd_mip_fade", 1.0f, ResetGeometryPrograms), // FIXME: set to 1/3 for classic mode; filmic mode makes full fade look nice
		filterTextureBrightness("rnd_filter_texture_brightness", FILTER_TEXTURE_BRIGHTNESS_DITHER, ResetGeometryPrograms),
		filterTextureColor("rnd_filter_texture_color", true, ResetGeometryPrograms),
		anchorTexelNormals("rnd_anchor_texel_normals", false, ResetGeometryPrograms),
		skySphereSeamMipBias("rnd_sky_sphere_seam_mip_bias", -10.0f),
		usingFBOs("rnd_using_fbos", false, con::ReadOnly),
		forceNoFBOs("rnd_force_no_fbos", false, ResetViewportBool),
		nearClip("rnd_near_clip", 0.1f, SetNearClip),
		farClip("rnd_far_clip", 5000.0f, SetFarClip), // FIXME: remove and always do infinite far plane or should a finite far plane be an option?
		preciseClipRange("rnd_precise_clip_range", false, con::ReadOnly), // Clip space z is [1, 0] instead of [1, -1]
		highDepthBuffer("rnd_high_depth_buffer", false, con::ReadOnly),
		forceLowDepthBuffer("rnd_force_low_depth_buffer", false, ResetViewportBool),
		shadowRes("rnd_shadow_res", 1024, SetTextureDimension),
		sunShadowRes("rnd_sun_shadow_res", 2048, SetTextureDimension), // Res of each cascade
		kernelSize("rnd_kernel_size", 0.6f),
		kernelSizeSpot("rnd_kernel_size_spot", 1.0f / 700.0f),
		kernelSize0("rnd_kernel_size_c0", 1.0f / 700.0f),
		kernelSize1("rnd_kernel_size_c1", 1.0f / 700.0f),
		kernelSize2("rnd_kernel_size_c2", 1.0f / 700.0f),
		kernelSize3("rnd_kernel_size_c3", 1.0f / 2000.0f), // Reduce smoothing to keep shadows at similar sharpness, but distorted
		cascadeDist0("rnd_cascade_dist_0", 120.0f),
		cascadeDist1("rnd_cascade_dist_1", 400.0f),
		cascadeDist2("rnd_cascade_dist_2", 1000.0f),
		cascadeDist3("rnd_cascade_dist_3", 5000.0f), // FIXME: implement; untie last cascade from farClip since there is no far plane anymore
		cascadePurity("rnd_cascade_purity", 0.8f, con::PositiveOnly),
		cascadeExp0("rnd_shadow_exp_c0", 1.0f),
		cascadeExp1("rnd_shadow_exp_c1", 2.0f), // Square secondary cascade shadows to line them up w/ first cascade better
		cascadeExp2("rnd_shadow_exp_c2", 2.0f),
		cascadeExp3("rnd_shadow_exp_c3", 2.0f),
		polyBias("rnd_poly_bias", 0.0f),
		polyBiasSpot("rnd_poly_bias_spot", 0.0f),
		polyBias0("rnd_poly_bias_c0", 0.0f),
		polyBias1("rnd_poly_bias_c1", 0.0f),
		polyBias2("rnd_poly_bias_c2", 0.0f),
		polyBias3("rnd_poly_bias_c3", 0.0f),
		polySlopeBias("rnd_poly_slope_bias", 0.0f),
		polySlopeBiasSpot("rnd_poly_slope_bias_spot", 1.0f),
		polySlopeBias0("rnd_poly_slope_bias_c0", 3.0f), // Higher than c1 to reduce dark PCF edges some more
		polySlopeBias1("rnd_poly_slope_bias_c1", 2.0f),
		polySlopeBias2("rnd_poly_slope_bias_c2", 2.5f),
		polySlopeBias3("rnd_poly_slope_bias_c3", 0.2f), // Surface bias compensates for small slope bias here, less light leaking
		depthBias("rnd_depth_bias", 2e-007f),
		surfaceBias("rnd_surface_bias", 1.5f),
		surfaceBiasSpot("rnd_surface_bias_spot", 1.3f),
		surfaceBias0("rnd_surface_bias_c0", 0.5f), // FIXME: getting occasional artifacts on walls w/ sun at -1.0, -0.2, 0.3
		surfaceBias1("rnd_surface_bias_c1", 2.0f),
		surfaceBias2("rnd_surface_bias_c2", 3.0f),
		surfaceBias3("rnd_surface_bias_c3", 16.0f), // Shifts shadows a lot, but hides small blocky shadows
		vertexBias("rnd_vertex_bias", 4.0f),
		vertexBiasSpot("rnd_vertex_bias_spot", 2.0f),
		spotShadowAddedAngle("rnd_spot_shadow_added_angle", 0.02f),
		tighterSpotCull("rnd_tighter_spot_cull", true),
		light16("rnd_high_light_buffer", false, con::ReadOnly),
		forceLowLightBuffer("rnd_force_low_light_buffer", false, ResetViewportBool),
		maxIntensity("rnd_max_intensity", 8.0f, SetMaxIntensity),
		maxIntensityLow("rnd_max_intensity_low", 2.0f, SetMaxIntensity),
		numColorBands("rnd_num_color_bands", 1024, ResetPalette),
		numColorBandsLow("rnd_num_color_bands_low", 32, ResetPalette),
		lightFade("rnd_light_fade", 0.0005f), // FIXME: increase so lights on min-shaded surfaces aren't so hard-edged?
		lightFadeLow("rnd_light_fade_low", 0.01f),
		exposure("rnd_exposure", 1.0f),
		exposureRandom("rnd_exposure_random", 0.04f),
		lightGamma("rnd_light_gamma", 0.454545f),
		lightCurve("rnd_light_curve", true, ResetViewportBool),
		numCurveSegments("rnd_num_curve_segments", 256, SetNumCurveSegments),
		worldAmbientFactor("rnd_world_ambient_factor", 4.0f),
		worldAmbientExponent("rnd_world_ambient_exponent", 2.0f),
		ditherRandom("rnd_dither_random", 0.0f), // FIXME: make these bools
		ditherGrain("rnd_dither_grain", 1.0f),
		ditherGrainSize("rnd_dither_grain_size", 3.0f, con::PositiveOnly),
		ditherAmount("rnd_dither_amount", 2.0f), // FIXME: optimize shaders if these are disabled
		rgbBlend("rnd_rgb_blend", 0, ResetPalette), // FIXME: affects min shade brightness for some reason
			// FIXME: make this a bool
			// FIXME: broken by per-frame binding of ramp texture
		rampAdd("rnd_ramp_add", -2.0f),
		noiseTime("rnd_noise_time", 41.667f),
		portalColor("rnd_portal_color", -1),
		sunPortalColor("rnd_sun_portal_color", -1),
		entityColor("rnd_entity_color", -1),
		sunEntityColor("rnd_sun_entity_color", -1),
		bulbColor("rnd_bulb_color", -1),
		cameraColor("rnd_camera_color", -1),
		curveColor("rnd_curve_color", -1),
		leafColor("rnd_leaf_color", -1),
		solidLeafColor("rnd_solid_leaf_color", -1),
		lineWidth("rnd_line_width", 1, SetLineWidth),
		timeRender("rnd_time_render", 0),
		lockCullCam("rnd_lock_cull_cam", false),
		antiAliasing("rnd_anti_aliasing", ANTI_ALIASING_SOFT_FXAA, SetAntiAliasing),
		fxaaQualitySubpix("rnd_fxaa_subpix", 0.0f), // Only affects original FXAA
		fxaaQualityEdgeThreshold("rnd_fxaa_edge_threshold", 0.3f), // FIXME: set to 0.166 for "classic mode" rigorous FXAA and 0.3 for "filmic mode" soft FXAA
		fxaaQualityEdgeThresholdMin("rnd_fxaa_edge_threshold_min", 0.0625f),
		forceNoGather4("rnd_fxaa_force_no_gather_4", false, ResetAntiAliasingProgram),
		warpGrainSize("rnd_fxaa_warp_grain_size", 6.0f, con::PositiveOnly),
		warpAmount("rnd_fxaa_warp_amount", 0.4f),
		warpStraight("rnd_fxaa_warp_straight", 0.7f),
		warpDiagonal("rnd_fxaa_warp_diagonal", 1.0f);
}

/*--------------------------------------
	rnd::ResetGeometryPrograms
--------------------------------------*/
void rnd::ResetGeometryPrograms()
{
	SetShaderDefines();
	DeleteWorldProgram();
	DeleteModelProgram();
	DeleteSkySphereProgram();
	DeleteGlassRecolorProgram();
}

/*--------------------------------------
	rnd::ResetGeometryPrograms
--------------------------------------*/
void rnd::ResetGeometryPrograms(con::Option& opt, float set)
{
	opt.ForceValue(set);
	ResetGeometryPrograms();
}

/*--------------------------------------
	rnd::ResetAntiAliasingProgram
--------------------------------------*/
void rnd::ResetAntiAliasingProgram()
{
	SetShaderDefines();
	DeleteAntiAliasingProgram();
}

/*--------------------------------------
	rnd::ResetAntiAliasingProgram
--------------------------------------*/
void rnd::ResetAntiAliasingProgram(con::Option& opt, float set)
{
	opt.ForceValue(set);
	ResetAntiAliasingProgram();
}

/*--------------------------------------
	rnd::SetMipMapping
--------------------------------------*/
void rnd::SetMipMapping(con::Option& opt, float set)
{
	opt.ForceValue(set);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);

	for(com::linker<Texture>* it = Texture::List().f; it; it = it->next)
	{
		TextureGL* tex = (TextureGL*)it->o;
		glBindTexture(GL_TEXTURE_2D, tex->texName);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex->MipFilter());
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	ResetGeometryPrograms();
}

/*--------------------------------------
	rnd::SetNearClip
--------------------------------------*/
void rnd::SetNearClip(con::Option& opt, float set)
{
	if(set >= farClip.Float())
	{
		con::AlertF("Near clip must be < far clip");
		return;
	}

	opt.ForceValue(set);
}

/*--------------------------------------
	rnd::SetFarClip
--------------------------------------*/
void rnd::SetFarClip(con::Option& opt, float set)
{
	if(set <= nearClip.Float())
	{
		con::AlertF("Far clip must be > near clip");
		return;
	}

	opt.ForceValue(set);
}

/*--------------------------------------
	rnd::SetTextureDimension
--------------------------------------*/
void rnd::SetTextureDimension(con::Option& opt, float set)
{
	int i = set;

	if(!com::IsPow2(i) || i < 4)
	{
		con::AlertF("Invalid %s %d; must be a power of two >= 4", opt.Name(), i);
		return;
	}

	opt.ForceValue(i);
}

/*--------------------------------------
	rnd::ResetViewportBool
--------------------------------------*/
void rnd::ResetViewportBool(con::Option& opt, float set)
{
	opt.ForceValue(set ? 1.0f : 0.0f);
	UpdateViewport();
}

/*--------------------------------------
	rnd::SetMaxIntensity
--------------------------------------*/
void rnd::SetMaxIntensity(con::Option& opt, float set)
{
	con::PositiveOnly(opt, set);
	CalculateCurveTexture();
}

/*--------------------------------------
	rnd::SetNumCurveSegments
--------------------------------------*/
void rnd::SetNumCurveSegments(con::Option& opt, float set)
{
	SetTextureDimension(opt, set);
	CalculateCurveTexture();
}

/*--------------------------------------
	rnd::ResetPalette
--------------------------------------*/
void rnd::ResetPalette(con::Option& opt, float set)
{
	opt.ForceValue(set);

	if(CurrentPalette())
		SetPalette(*CurrentPalette(), true);
}

/*--------------------------------------
	rnd::SetLineWidth
--------------------------------------*/
void rnd::SetLineWidth(con::Option& opt, float set)
{
	glLineWidth(set);
	GLfloat f;
	glGetFloatv(GL_LINE_WIDTH, &f);
	opt.ForceValue(f);
}

/*--------------------------------------
	rnd::SetAntiAliasing
--------------------------------------*/
void rnd::SetAntiAliasing(con::Option& opt, float set)
{
	int si = set;

	if(si < 0 || si >= NUM_ANTI_ALIASING_OPTIONS)
	{
		con::AlertF("Invalid anti-aliasing option \"%d\", must be 0 (none), 1 (original FXAA), 2 "
			"(rigorous FXAA), 3 (soft FXAA), or 4 (warped FXAA)", si);

		return;
	}

	opt.ForceValue(set);
	ResetAntiAliasingProgram();
}