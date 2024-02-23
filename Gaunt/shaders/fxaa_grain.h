/*============================================================================


					NVIDIA FXAA 3.11 by TIMOTHY LOTTES


------------------------------------------------------------------------------
COPYRIGHT (C) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.
------------------------------------------------------------------------------
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
*AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL NVIDIA
OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR
LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION,
OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE
THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

------------------------------------------------------------------------------
						   INTEGRATION CHECKLIST
------------------------------------------------------------------------------
(1.)
In the shader source, setup defines for the desired configuration.
When providing multiple shaders (for different presets),
simply setup the defines differently in multiple files.
Example,

  #define FXAA_PC 1
  #define FXAA_HLSL_5 1
  #define FXAA_QUALITY__PRESET 12

Or,

  #define FXAA_360 1
  
Or,

  #define FXAA_PS3 1
  
Etc.

(2.)
Then include this file,

  #include "Fxaa3_11.h"

(3.)
Then call the FXAA pixel shader from within your desired shader.
Look at the FXAA Quality FxaaPixelShader() for docs on inputs.
As for FXAA 3.11 all inputs for all shaders are the same 
to enable easy porting between platforms.

  return FxaaPixelShader(...);

(4.)
Insure pass prior to FXAA outputs RGBL (see next section).
Or use,

  #define FXAA_GREEN_AS_LUMA 1

(5.)
Setup engine to provide the following constants
which are used in the FxaaPixelShader() inputs,

  FxaaFloat2 fxaaQualityRcpFrame,
  FxaaFloat4 fxaaConsoleRcpFrameOpt,
  FxaaFloat4 fxaaConsoleRcpFrameOpt2,
  FxaaFloat4 fxaaConsole360RcpFrameOpt2,
  FxaaFloat fxaaQualitySubpix,
  FxaaFloat fxaaQualityEdgeThreshold,
  FxaaFloat fxaaQualityEdgeThresholdMin,
  FxaaFloat fxaaConsoleEdgeSharpness,
  FxaaFloat fxaaConsoleEdgeThreshold,
  FxaaFloat fxaaConsoleEdgeThresholdMin,
  FxaaFloat4 fxaaConsole360ConstDir

Look at the FXAA Quality FxaaPixelShader() for docs on inputs.

(6.)
Have FXAA vertex shader run as a full screen triangle,
and output "pos" and "fxaaConsolePosPos" 
such that inputs in the pixel shader provide,

  // {xy} = center of pixel
  FxaaFloat2 pos,

  // {xy__} = upper left of pixel
  // {__zw} = lower right of pixel
  FxaaFloat4 fxaaConsolePosPos,

(7.)
Insure the texture sampler(s) used by FXAA are set to bilinear filtering.


------------------------------------------------------------------------------
					INTEGRATION - RGBL AND COLORSPACE
------------------------------------------------------------------------------
FXAA3 requires RGBL as input unless the following is set, 

  #define FXAA_GREEN_AS_LUMA 1

In which case the engine uses green in place of luma,
and requires RGB input is in a non-linear colorspace.

RGB should be LDR (low dynamic range).
Specifically do FXAA after tonemapping.

RGB data as returned by a texture fetch can be non-linear,
or linear when FXAA_GREEN_AS_LUMA is not set.
Note an "sRGB format" texture counts as linear,
because the result of a texture fetch is linear data.
Regular "RGBA8" textures in the sRGB colorspace are non-linear.

If FXAA_GREEN_AS_LUMA is not set,
luma must be stored in the alpha channel prior to running FXAA.
This luma should be in a perceptual space (could be gamma 2.0).
Example pass before FXAA where output is gamma 2.0 encoded,

  color.rgb = ToneMap(color.rgb); // linear color output
  color.rgb = sqrt(color.rgb);	// gamma 2.0 color output
  return color;

To use FXAA,

  color.rgb = ToneMap(color.rgb);  // linear color output
  color.rgb = sqrt(color.rgb);	 // gamma 2.0 color output
  color.a = dot(color.rgb, FxaaFloat3(0.299, 0.587, 0.114)); // compute luma
  return color;

Another example where output is linear encoded,
say for instance writing to an sRGB formated render target,
where the render target does the conversion back to sRGB after blending,

  color.rgb = ToneMap(color.rgb); // linear color output
  return color;

To use FXAA,

  color.rgb = ToneMap(color.rgb); // linear color output
  color.a = sqrt(dot(color.rgb, FxaaFloat3(0.299, 0.587, 0.114))); // compute luma
  return color;

Getting luma correct is required for the algorithm to work correctly.


------------------------------------------------------------------------------
						  BEING LINEARLY CORRECT?
------------------------------------------------------------------------------
Applying FXAA to a framebuffer with linear RGB color will look worse.
This is very counter intuitive, but happends to be true in this case.
The reason is because dithering artifacts will be more visiable 
in a linear colorspace.


------------------------------------------------------------------------------
							 COMPLEX INTEGRATION
------------------------------------------------------------------------------
Q. What if the engine is blending into RGB before wanting to run FXAA?

A. In the last opaque pass prior to FXAA,
   have the pass write out luma into alpha.
   Then blend into RGB only.
   FXAA should be able to run ok
   assuming the blending pass did not any add aliasing.
   This should be the common case for particles and common blending passes.

A. Or use FXAA_GREEN_AS_LUMA.

============================================================================*/

/*============================================================================

							 INTEGRATION KNOBS

============================================================================*/
//
// FXAA_PS3 and FXAA_360 choose the console algorithm (FXAA3 CONSOLE).
// FXAA_360_OPT is a prototype for the new optimized 360 version.
//
// 1 = Use API.
// 0 = Don't use API.
//
/*--------------------------------------------------------------------------*/
#ifndef FXAA_PS3
	#define FXAA_PS3 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_360
	#define FXAA_360 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_360_OPT
	#define FXAA_360_OPT 0
#endif
/*==========================================================================*/
#ifndef FXAA_PC
	//
	// FXAA Quality
	// The high quality PC algorithm.
	//
	#define FXAA_PC 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_PC_CONSOLE
	//
	// The console algorithm for PC is included
	// for developers targeting really low spec machines.
	// Likely better to just run FXAA_PC, and use a really low preset.
	//
	#define FXAA_PC_CONSOLE 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_GLSL_120
	#define FXAA_GLSL_120 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_GLSL_130
	#define FXAA_GLSL_130 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_HLSL_3
	#define FXAA_HLSL_3 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_HLSL_4
	#define FXAA_HLSL_4 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_HLSL_5
	#define FXAA_HLSL_5 0
#endif
/*==========================================================================*/
#ifndef FXAA_GREEN_AS_LUMA
	//
	// For those using non-linear color,
	// and either not able to get luma in alpha, or not wanting to,
	// this enables FXAA to run using green as a proxy for luma.
	// So with this enabled, no need to pack luma in alpha.
	//
	// This will turn off AA on anything which lacks some amount of green.
	// Pure red and blue or combination of only R and B, will get no AA.
	//
	// Might want to lower the settings for both,
	//	fxaaConsoleEdgeThresholdMin
	//	fxaaQualityEdgeThresholdMin
	// In order to insure AA does not get turned off on colors 
	// which contain a minor amount of green.
	//
	// 1 = On.
	// 0 = Off.
	//
	#define FXAA_GREEN_AS_LUMA 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_EARLY_EXIT
	//
	// Controls algorithm's early exit path.
	// On PS3 turning this ON adds 2 cycles to the shader.
	// On 360 turning this OFF adds 10ths of a millisecond to the shader.
	// Turning this off on console will result in a more blurry image.
	// So this defaults to on.
	//
	// 1 = On.
	// 0 = Off.
	//
	#define FXAA_EARLY_EXIT 1
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_DISCARD
	//
	// Only valid for PC OpenGL currently.
	// Probably will not work when FXAA_GREEN_AS_LUMA = 1.
	//
	// 1 = Use discard on pixels which don't need AA.
	//	 For APIs which enable concurrent TEX+ROP from same surface.
	// 0 = Return unchanged color on pixels which don't need AA.
	//
	#define FXAA_DISCARD 0
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_FAST_PIXEL_OFFSET
	//
	// Used for GLSL 120 only.
	//
	// 1 = GL API supports fast pixel offsets
	// 0 = do not use fast pixel offsets
	//
	#ifdef GL_EXT_gpu_shader4
		#define FXAA_FAST_PIXEL_OFFSET 1
	#endif
	#ifdef GL_NV_gpu_shader5
		#define FXAA_FAST_PIXEL_OFFSET 1
	#endif
	#ifdef GL_ARB_gpu_shader5
		#define FXAA_FAST_PIXEL_OFFSET 1
	#endif
	#ifndef FXAA_FAST_PIXEL_OFFSET
		#define FXAA_FAST_PIXEL_OFFSET 0
	#endif
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_GATHER4_ALPHA
	//
	// 1 = API supports gather4 on alpha channel.
	// 0 = API does not support gather4 on alpha channel.
	//
	#if (FXAA_HLSL_5 == 1)
		#define FXAA_GATHER4_ALPHA 1
	#endif
	#ifdef GL_ARB_gpu_shader5
		#define FXAA_GATHER4_ALPHA 1
	#endif
	#ifdef GL_NV_gpu_shader5
		#define FXAA_GATHER4_ALPHA 1
	#endif
	#ifndef FXAA_GATHER4_ALPHA
		#define FXAA_GATHER4_ALPHA 0
	#endif
#endif
/*--------------------------------------------------------------------------*/
#ifndef BLUR_EDGES
	#define BLUR_EDGES 0
#endif

/*============================================================================
					  FXAA CONSOLE PS3 - TUNING KNOBS
============================================================================*/
#ifndef FXAA_CONSOLE__PS3_EDGE_SHARPNESS
	//
	// Consoles the sharpness of edges on PS3 only.
	// Non-PS3 tuning is done with shader input.
	//
	// Due to the PS3 being ALU bound,
	// there are only two safe values here: 4 and 8.
	// These options use the shaders ability to a free *|/ by 2|4|8.
	//
	// 8.0 is sharper
	// 4.0 is softer
	// 2.0 is really soft (good for vector graphics inputs)
	//
	#if 1
		#define FXAA_CONSOLE__PS3_EDGE_SHARPNESS 8.0
	#endif
	#if 0
		#define FXAA_CONSOLE__PS3_EDGE_SHARPNESS 4.0
	#endif
	#if 0
		#define FXAA_CONSOLE__PS3_EDGE_SHARPNESS 2.0
	#endif
#endif
/*--------------------------------------------------------------------------*/
#ifndef FXAA_CONSOLE__PS3_EDGE_THRESHOLD
	//
	// Only effects PS3.
	// Non-PS3 tuning is done with shader input.
	//
	// The minimum amount of local contrast required to apply algorithm.
	// The console setting has a different mapping than the quality setting.
	//
	// This only applies when FXAA_EARLY_EXIT is 1.
	//
	// Due to the PS3 being ALU bound,
	// there are only two safe values here: 0.25 and 0.125.
	// These options use the shaders ability to a free *|/ by 2|4|8.
	//
	// 0.125 leaves less aliasing, but is softer
	// 0.25 leaves more aliasing, and is sharper
	//
	#if 1
		#define FXAA_CONSOLE__PS3_EDGE_THRESHOLD 0.125
	#else
		#define FXAA_CONSOLE__PS3_EDGE_THRESHOLD 0.25
	#endif
#endif

/*============================================================================
						FXAA QUALITY - TUNING KNOBS
------------------------------------------------------------------------------
NOTE the other tuning knobs are now in the shader function inputs!
============================================================================*/

// Not tunable in grain version
// FIXME: this is the "extreme" setting, do 6 iterations normally
//*
#define FXAA_QUALITY__PS 12
#define FXAA_QUALITY__P0 1.0
#define FXAA_QUALITY__P1 1.0
#define FXAA_QUALITY__P2 1.0
#define FXAA_QUALITY__P3 1.0
#define FXAA_QUALITY__P4 1.0
#define FXAA_QUALITY__P5 1.5
#define FXAA_QUALITY__P6 2.0
#define FXAA_QUALITY__P7 2.0
#define FXAA_QUALITY__P8 2.0
#define FXAA_QUALITY__P9 2.0
#define FXAA_QUALITY__P10 4.0
#define FXAA_QUALITY__P11 8.0
//*/

// Perfect but size-limited edge detection; for debugging
/*
#define FXAA_QUALITY__PS 12
#define FXAA_QUALITY__P0 1.0
#define FXAA_QUALITY__P1 1.0
#define FXAA_QUALITY__P2 1.0
#define FXAA_QUALITY__P3 1.0
#define FXAA_QUALITY__P4 1.0
#define FXAA_QUALITY__P5 1.0
#define FXAA_QUALITY__P6 1.0
#define FXAA_QUALITY__P7 1.0
#define FXAA_QUALITY__P8 1.0
#define FXAA_QUALITY__P9 1.0
#define FXAA_QUALITY__P10 1.0
#define FXAA_QUALITY__P11 1.0
//*/

/*============================================================================

								API PORTING

============================================================================*/
#if (FXAA_GLSL_120 == 1) || (FXAA_GLSL_130 == 1)
	#define FxaaBool bool
	#define FxaaDiscard discard
	#define FxaaFloat float
	#define FxaaFloat2 vec2
	#define FxaaFloat3 vec3
	#define FxaaFloat4 vec4
	#define FxaaHalf float
	#define FxaaHalf2 vec2
	#define FxaaHalf3 vec3
	#define FxaaHalf4 vec4
	#define FxaaInt2 ivec2
	#define FxaaSat(x) clamp(x, 0.0, 1.0)
	#define FxaaTex sampler2D
#else
	#define FxaaBool bool
	#define FxaaDiscard clip(-1)
	#define FxaaFloat float
	#define FxaaFloat2 float2
	#define FxaaFloat3 float3
	#define FxaaFloat4 float4
	#define FxaaHalf half
	#define FxaaHalf2 half2
	#define FxaaHalf3 half3
	#define FxaaHalf4 half4
	#define FxaaSat(x) saturate(x)
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_GLSL_120 == 1)
	// Requires,
	//  #version 120
	// And at least,
	//  #extension GL_EXT_gpu_shader4 : enable
	//  (or set FXAA_FAST_PIXEL_OFFSET 1 to work like DX9)
	#define FxaaTexTop(t, p) texture2DLod(t, p, 0.0)
	#if (FXAA_FAST_PIXEL_OFFSET == 1)
		#define FxaaTexOff(t, p, o, r) texture2DLodOffset(t, p, 0.0, o)
	#else
		#define FxaaTexOff(t, p, o, r) texture2DLod(t, p + (o * r), 0.0)
	#endif
	#if (FXAA_GATHER4_ALPHA == 1)
		// use #extension GL_ARB_gpu_shader5 : enable
		#define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
		#define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
		#define FxaaTexGreen4(t, p) textureGather(t, p, 1)
		#define FxaaTexOffGreen4(t, p, o) textureGatherOffset(t, p, o, 1)
	#endif
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_GLSL_130 == 1)
	// Requires "#version 130" or better
	#define FxaaTexTop(t, p) textureLod(t, p, 0.0)
	#define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)
	#if (FXAA_GATHER4_ALPHA == 1)
		// use #extension GL_ARB_gpu_shader5 : enable
		#define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
		#define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
		#define FxaaTexGreen4(t, p) textureGather(t, p, 1)
		#define FxaaTexOffGreen4(t, p, o) textureGatherOffset(t, p, o, 1)
	#endif
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_HLSL_3 == 1) || (FXAA_360 == 1) || (FXAA_PS3 == 1)
	#define FxaaInt2 float2
	#define FxaaTex sampler2D
	#define FxaaTexTop(t, p) tex2Dlod(t, float4(p, 0.0, 0.0))
	#define FxaaTexOff(t, p, o, r) tex2Dlod(t, float4(p + (o * r), 0, 0))
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_HLSL_4 == 1)
	#define FxaaInt2 int2
	struct FxaaTex { SamplerState smpl; Texture2D tex; };
	#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
	#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_HLSL_5 == 1)
	#define FxaaInt2 int2
	struct FxaaTex { SamplerState smpl; Texture2D tex; };
	#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
	#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
	#define FxaaTexAlpha4(t, p) t.tex.GatherAlpha(t.smpl, p)
	#define FxaaTexOffAlpha4(t, p, o) t.tex.GatherAlpha(t.smpl, p, o)
	#define FxaaTexGreen4(t, p) t.tex.GatherGreen(t.smpl, p)
	#define FxaaTexOffGreen4(t, p, o) t.tex.GatherGreen(t.smpl, p, o)
#endif


/*============================================================================
				   GREEN AS LUMA OPTION SUPPORT FUNCTION
============================================================================*/
#if (FXAA_GREEN_AS_LUMA == 0)
	FxaaFloat FxaaLuma(FxaaFloat4 rgba) { return rgba.w; }
#else
	FxaaFloat FxaaLuma(FxaaFloat4 rgba) { return rgba.y; }
#endif	




/*============================================================================

							 FXAA3 QUALITY - PC

============================================================================*/

vec2 Random2(vec2 screen, float s)
{
	vec2 d2 = vec2(
		dot(screen, vec2(125.9898 + s * 3.5, 78.233 - s * 6.0)),
		dot(screen, vec2(58.0317 - s * 2.6, 934.737 + s * 1.4))
	);

	return fract(sin(d2) * 43758.5453 * s);
}

// FIXME: random offset from parameter
// FIXME: try different sum pows
float SimplexNoise(vec2 screen, float s)
{
	const float TO_SKEWED = 0.36602540378443864676372317075294; /* (sqrt(3) - 1) / 2 */
	const float TO_SCREEN = 0.21132486540518711774542560974902; /* (3 - sqrt(3)) / 6 */
	screen += 10.583; /* Offset so low-left corner isn't 0, 0 */
	vec2 skewed = screen + (screen.x + screen.y) * TO_SKEWED;
	vec2 origin = floor(skewed);
	vec2 dif = screen - origin + (origin.x + origin.y) * TO_SCREEN;
	vec2 difFar = dif - 0.57735026918962576450914878050196; /* dif - 1.0 + 2.0 * TO_SCREEN */
	float cmp = step(dif.y, dif.x);
	vec2 sideOff = vec2(cmp, 1.0 - cmp); /* dif.x > dif.y ? vec2(1, 0) : vec2(0, 1) */
		/* This works despite dif being in screen space because the boundary line between
		two simplices in a grid quad has the same angle (45 deg) skewed and unskewed */ \
	vec2 difSide = dif - sideOff + TO_SCREEN;
	float sum = 0.0;
	
	vec2 grad = 1.0 - 2.0 * Random2(origin, s);
	float f = max(0.0, 0.5 - dot(dif, dif));
	f *= f;
	sum += f * f * dot(grad, dif);
	
	grad = 1.0 - 2.0 * Random2(origin + 1.0, s);
	f = max(0.0, 0.5 - dot(difFar, difFar));
	f *= f;
	sum += f * f * dot(grad, difFar);
	
	grad = 1.0 - 2.0 * Random2(origin + sideOff, s);
	f = max(0.0, 0.5 - dot(difSide, difSide));
	f *= f;
	sum += f * f * dot(grad, difSide);
	
	/* Multiply by magic number to make largest sum ~1.0 (at triangle center if all gradients are facing it) */
	return sum * 70.0;
}

#if (FXAA_PC == 1)
/*--------------------------------------------------------------------------*/
// FIXME: remove unused parameters
FxaaFloat4 FxaaPixelShader(
	//
	// Use noperspective interpolation here (turn off perspective interpolation).
	// {xy} = center of pixel
	FxaaFloat2 pos,
	//
	// Used only for FXAA Console, and not used on the 360 version.
	// Use noperspective interpolation here (turn off perspective interpolation).
	// {xy__} = upper left of pixel
	// {__zw} = lower right of pixel
	FxaaFloat4 fxaaConsolePosPos,
	//
	// Input color texture.
	// {rgb_} = color in linear or perceptual color space
	// if (FXAA_GREEN_AS_LUMA == 0)
	//	 {___a} = luma in perceptual color space (not linear)
	FxaaTex tex,
	//
	// Only used on the optimized 360 version of FXAA Console.
	// For everything but 360, just use the same input here as for "tex".
	// For 360, same texture, just alias with a 2nd sampler.
	// This sampler needs to have an exponent bias of -1.
	FxaaTex fxaaConsole360TexExpBiasNegOne,
	//
	// Only used on the optimized 360 version of FXAA Console.
	// For everything but 360, just use the same input here as for "tex".
	// For 360, same texture, just alias with a 3nd sampler.
	// This sampler needs to have an exponent bias of -2.
	FxaaTex fxaaConsole360TexExpBiasNegTwo,
	//
	// Only used on FXAA Quality.
	// This must be from a constant/uniform.
	// {x_} = 1.0/screenWidthInPixels
	// {_y} = 1.0/screenHeightInPixels
	FxaaFloat2 fxaaQualityRcpFrame,
	//
	// Only used on FXAA Console.
	// This must be from a constant/uniform.
	// This effects sub-pixel AA quality and inversely sharpness.
	//   Where N ranges between,
	//	 N = 0.50 (default)
	//	 N = 0.33 (sharper)
	// {x___} = -N/screenWidthInPixels  
	// {_y__} = -N/screenHeightInPixels
	// {__z_} =  N/screenWidthInPixels  
	// {___w} =  N/screenHeightInPixels 
	FxaaFloat4 fxaaConsoleRcpFrameOpt,
	//
	// Only used on FXAA Console.
	// Not used on 360, but used on PS3 and PC.
	// This must be from a constant/uniform.
	// {x___} = -2.0/screenWidthInPixels  
	// {_y__} = -2.0/screenHeightInPixels
	// {__z_} =  2.0/screenWidthInPixels  
	// {___w} =  2.0/screenHeightInPixels 
	FxaaFloat4 fxaaConsoleRcpFrameOpt2,
	//
	// Only used on FXAA Console.
	// Only used on 360 in place of fxaaConsoleRcpFrameOpt2.
	// This must be from a constant/uniform.
	// {x___} =  8.0/screenWidthInPixels  
	// {_y__} =  8.0/screenHeightInPixels
	// {__z_} = -4.0/screenWidthInPixels  
	// {___w} = -4.0/screenHeightInPixels 
	FxaaFloat4 fxaaConsole360RcpFrameOpt2,
	//
	// Only used on FXAA Quality.
	// This used to be the FXAA_QUALITY__SUBPIX define.
	// It is here now to allow easier tuning.
	// Choose the amount of sub-pixel aliasing removal.
	// This can effect sharpness.
	//   1.00 - upper limit (softer)
	//   0.75 - default amount of filtering
	//   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
	//   0.25 - almost off
	//   0.00 - completely off
	FxaaFloat fxaaQualitySubpix,
	//
	// Only used on FXAA Quality.
	// This used to be the FXAA_QUALITY__EDGE_THRESHOLD define.
	// It is here now to allow easier tuning.
	// The minimum amount of local contrast required to apply algorithm.
	//   0.333 - too little (faster)
	//   0.250 - low quality
	//   0.166 - default
	//   0.125 - high quality 
	//   0.063 - overkill (slower)
	FxaaFloat fxaaQualityEdgeThreshold,
	//
	// Only used on FXAA Quality.
	// This used to be the FXAA_QUALITY__EDGE_THRESHOLD_MIN define.
	// It is here now to allow easier tuning.
	// Trims the algorithm from processing darks.
	//   0.0833 - upper limit (default, the start of visible unfiltered edges)
	//   0.0625 - high quality (faster)
	//   0.0312 - visible limit (slower)
	// Special notes when using FXAA_GREEN_AS_LUMA,
	//   Likely want to set this to zero.
	//   As colors that are mostly not-green
	//   will appear very dark in the green channel!
	//   Tune by looking at mostly non-green content,
	//   then start at zero and increase until aliasing is a problem.
	FxaaFloat fxaaQualityEdgeThresholdMin,
	// 
	// Only used on FXAA Console.
	// This used to be the FXAA_CONSOLE__EDGE_SHARPNESS define.
	// It is here now to allow easier tuning.
	// This does not effect PS3, as this needs to be compiled in.
	//   Use FXAA_CONSOLE__PS3_EDGE_SHARPNESS for PS3.
	//   Due to the PS3 being ALU bound,
	//   there are only three safe values here: 2 and 4 and 8.
	//   These options use the shaders ability to a free *|/ by 2|4|8.
	// For all other platforms can be a non-power of two.
	//   8.0 is sharper (default!!!)
	//   4.0 is softer
	//   2.0 is really soft (good only for vector graphics inputs)
	FxaaFloat fxaaConsoleEdgeSharpness,
	//
	// Only used on FXAA Console.
	// This used to be the FXAA_CONSOLE__EDGE_THRESHOLD define.
	// It is here now to allow easier tuning.
	// This does not effect PS3, as this needs to be compiled in.
	//   Use FXAA_CONSOLE__PS3_EDGE_THRESHOLD for PS3.
	//   Due to the PS3 being ALU bound,
	//   there are only two safe values here: 1/4 and 1/8.
	//   These options use the shaders ability to a free *|/ by 2|4|8.
	// The console setting has a different mapping than the quality setting.
	// Other platforms can use other values.
	//   0.125 leaves less aliasing, but is softer (default!!!)
	//   0.25 leaves more aliasing, and is sharper
	FxaaFloat fxaaConsoleEdgeThreshold,
	//
	// Only used on FXAA Console.
	// This used to be the FXAA_CONSOLE__EDGE_THRESHOLD_MIN define.
	// It is here now to allow easier tuning.
	// Trims the algorithm from processing darks.
	// The console setting has a different mapping than the quality setting.
	// This only applies when FXAA_EARLY_EXIT is 1.
	// This does not apply to PS3, 
	// PS3 was simplified to avoid more shader instructions.
	//   0.06 - faster but more aliasing in darks
	//   0.05 - default
	//   0.04 - slower and less aliasing in darks
	// Special notes when using FXAA_GREEN_AS_LUMA,
	//   Likely want to set this to zero.
	//   As colors that are mostly not-green
	//   will appear very dark in the green channel!
	//   Tune by looking at mostly non-green content,
	//   then start at zero and increase until aliasing is a problem.
	FxaaFloat fxaaConsoleEdgeThresholdMin,
	//	
	// Extra constants for 360 FXAA Console only.
	// Use zeros or anything else for other platforms.
	// These must be in physical constant registers and NOT immedates.
	// Immedates will result in compiler un-optimizing.
	// {xyzw} = float4(1.0, -1.0, 0.25, -0.25)
	FxaaFloat4 fxaaConsole360ConstDir,
	//
	// [1, 2], simplex noise seed
	FxaaFloat warpSeed,
	//
	// gl_FragCoord.xy multiplied to convert to grain screen space
	FxaaFloat2 warpPos,
	//
	// [0, 1]
	FxaaFloat warpStraight,
	//
	// [0, 1]
	FxaaFloat warpDiagonal
) {
/*--------------------------------------------------------------------------*/
	// Get lumas of middle, north, south, east, and west
	FxaaFloat2 posM;
	posM.x = pos.x;
	posM.y = pos.y;
	#if (FXAA_GATHER4_ALPHA == 1)
		#if (FXAA_DISCARD == 0)
			FxaaFloat4 rgbyM = FxaaTexTop(tex, posM);
			#if (FXAA_GREEN_AS_LUMA == 0)
				#define lumaM rgbyM.w
			#else
				#define lumaM rgbyM.y
			#endif
		#endif
		#if (FXAA_GREEN_AS_LUMA == 0)
			FxaaFloat4 luma4A = FxaaTexAlpha4(tex, posM);
			FxaaFloat4 luma4B = FxaaTexOffAlpha4(tex, posM, FxaaInt2(-1, -1));
		#else
			FxaaFloat4 luma4A = FxaaTexGreen4(tex, posM);
			FxaaFloat4 luma4B = FxaaTexOffGreen4(tex, posM, FxaaInt2(-1, -1));
		#endif
		#if (FXAA_DISCARD == 1)
			#define lumaM luma4A.w
		#endif
		#define lumaE luma4A.z
		#define lumaN luma4A.x
		#define lumaNE luma4A.y
		#define lumaSW luma4B.w
		#define lumaS luma4B.z
		#define lumaW luma4B.x
	#else
		FxaaFloat4 rgbyM = FxaaTexTop(tex, posM);
		#if (FXAA_GREEN_AS_LUMA == 0)
			#define lumaM rgbyM.w
		#else
			#define lumaM rgbyM.y
		#endif
		FxaaFloat lumaS = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 0,-1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1, 0), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaN = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 0, 1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 0), fxaaQualityRcpFrame.xy));
	#endif
/*--------------------------------------------------------------------------*/
	// Get difference of max and min lumas
	FxaaFloat maxSM = max(lumaS, lumaM);
	FxaaFloat minSM = min(lumaS, lumaM);
	FxaaFloat maxESM = max(lumaE, maxSM);
	FxaaFloat minESM = min(lumaE, minSM);
	FxaaFloat maxWN = max(lumaN, lumaW);
	FxaaFloat minWN = min(lumaN, lumaW);
	FxaaFloat rangeMax = max(maxWN, maxESM);
	FxaaFloat rangeMin = min(minWN, minESM);
	FxaaFloat rangeMaxScaled = rangeMax * fxaaQualityEdgeThreshold;
	FxaaFloat range = rangeMax - rangeMin;
	FxaaFloat rangeMaxClamped = max(fxaaQualityEdgeThresholdMin, rangeMaxScaled);
	FxaaBool earlyExit = range < rangeMaxClamped;
/*--------------------------------------------------------------------------*/
	// Exit early if luma range is small (not an edge)
	if(earlyExit)
		#if (FXAA_DISCARD == 1)
			FxaaDiscard;
		#else
			return rgbyM;
		#endif
/*--------------------------------------------------------------------------*/
	// Get lumas of corners
	#if (FXAA_GATHER4_ALPHA == 0)
		FxaaFloat lumaNW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaSE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1,-1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaNE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1, 1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaSW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1,-1), fxaaQualityRcpFrame.xy));
	#else
		FxaaFloat lumaSE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1,-1), fxaaQualityRcpFrame.xy));
		FxaaFloat lumaNW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 1), fxaaQualityRcpFrame.xy));
	#endif
/*--------------------------------------------------------------------------*/
	// Determine horizontal or vertical edge
	FxaaFloat lumaNS = lumaN + lumaS;
	FxaaFloat lumaWE = lumaW + lumaE;
	FxaaFloat edgeHorz1 = (-2.0 * lumaM) + lumaNS;
	FxaaFloat edgeVert1 = (-2.0 * lumaM) + lumaWE;
/*--------------------------------------------------------------------------*/
	FxaaFloat lumaNESE = lumaNE + lumaSE;
	FxaaFloat lumaNWNE = lumaNW + lumaNE;
	FxaaFloat edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
	FxaaFloat edgeVert2 = (-2.0 * lumaN) + lumaNWNE;
/*--------------------------------------------------------------------------*/
	FxaaFloat lumaNWSW = lumaNW + lumaSW;
	FxaaFloat lumaSWSE = lumaSW + lumaSE;
	FxaaFloat edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
	FxaaFloat edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
	FxaaFloat edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
	FxaaFloat edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
	FxaaFloat edgeHorz = abs(edgeHorz3) + edgeHorz4;
	FxaaFloat edgeVert = abs(edgeVert3) + edgeVert4;
/*--------------------------------------------------------------------------*/
	FxaaFloat2 lengthSign = FxaaFloat2(fxaaQualityRcpFrame.x, fxaaQualityRcpFrame.y);
	FxaaBool straightSpan = abs(edgeHorz - edgeVert) > 0.1;
	FxaaBool horzSpan; // If diagonal span, horzSpan really means inclineSpan
	if(straightSpan) horzSpan = edgeHorz >= edgeVert;
	if(!straightSpan) horzSpan = abs((-2.0 * lumaM) + lumaNE + lumaSW) <= abs((-2.0 * lumaM) + lumaNW + lumaSE);
		// FIXME: optimize calculations by doing muladds into variables
	//if(!straightSpan) lengthSign *= 0.70710678118654752440084436210485;
	FxaaBool smoothX = !straightSpan || !horzSpan;
	FxaaBool smoothY = !straightSpan || horzSpan;
/*--------------------------------------------------------------------------*/
	// FIXME: avoid nested and else branching
	if(straightSpan)
	{
		if(!horzSpan)
		{
			lumaN = lumaE;
			lumaS = lumaW;
		}
	}
	else
	{
		if(horzSpan) lengthSign.x = -lengthSign.x;

		/*
			Constants resolved from bilinear interpolation formula:
			fetch(x, y) = (1 - y) * ((1 - x) * M + x * E) + y * ((1 - x) * N + x * NE)
				Where x = 0, y = 0 is center of texel
			f = x = y
			fetch(f) = (1 - f) * ((1 - f) * M + f * E) + f * ((1 - f) * N + f * NE)
			Solve for f so M's multiplier is 0.5. Then multiply the resulting multipliers by 2
			since lumaN and lumaS are supposed to have a weight of 1.0 on their own.
		*/
		/*
		if(horzSpan)
		{
			// FIXME: just setting to the texel across the edge might be a good enough approximation
			lumaN = lumaNW * 0.1715728752538099023966225515806 + (lumaN + lumaW) * 0.4142135623730950488016887242097;
			lumaS = lumaSE * 0.1715728752538099023966225515806 + (lumaS + lumaE) * 0.4142135623730950488016887242097;
			lengthSign.x = -lengthSign.x;
		}
		else
		{
			lumaN = lumaNE * 0.1715728752538099023966225515806 + (lumaN + lumaE) * 0.4142135623730950488016887242097;
			lumaS = lumaSW * 0.1715728752538099023966225515806 + (lumaS + lumaW) * 0.4142135623730950488016887242097;
		}
		*/
	}

	FxaaFloat lumaNN = lumaN + lumaM;
	FxaaFloat lumaSS = lumaS + lumaM;
/*--------------------------------------------------------------------------*/
	// Choose which pair of rows/cols to follow (where's the edge on the 3x3)
	FxaaFloat gradientN = lumaN - lumaM;
	FxaaFloat gradientS = lumaS - lumaM;
	FxaaBool pairN = abs(gradientN) >= abs(gradientS);
	FxaaFloat gradient = max(abs(gradientN), abs(gradientS));
	if(!pairN) lengthSign = -lengthSign;
/*--------------------------------------------------------------------------*/
	/* Based on edge direction and pixel pair, set up offset incrementer and
	move search position in between edge's two pixels */
	FxaaFloat2 posB;
	posB.x = posM.x;
	posB.y = posM.y;
	FxaaFloat2 offNP;
	offNP.x = (smoothY) ? fxaaQualityRcpFrame.x : 0.0;
	offNP.y = (smoothX) ? fxaaQualityRcpFrame.y : 0.0;
	if(!straightSpan && !horzSpan) offNP.y = -offNP.y;

	// FIXME: avoid else branches
	if(straightSpan)
	{
		if(smoothX) posB.x += lengthSign.x * 0.5;
		if(smoothY) posB.y += lengthSign.y * 0.5;
	}
	else
	{
		posB.y += lengthSign.y * 0.5;

		// Offset so middle frag has 0.5 weight in bilinear fetch
		/*
		posB.x += lengthSign.x * 0.29289321881345247559915563789515;
		posB.y += lengthSign.y * 0.29289321881345247559915563789515;
		*/
	}
/*--------------------------------------------------------------------------*/
	// Begin search in both directions
	FxaaFloat2 posN;
	posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;
	posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;
	FxaaFloat2 posP;
	posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;
	posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;
	FxaaFloat lumaEndN = FxaaLuma(FxaaTexTop(tex, posN));
	FxaaFloat lumaEndP = FxaaLuma(FxaaTexTop(tex, posP));
/*--------------------------------------------------------------------------*/
	if(!pairN) lumaNN = lumaSS;
	FxaaFloat gradientScaled = gradient * 1.0/4.0;
	FxaaFloat lumaMM = lumaM - lumaNN * 0.5;
	FxaaBool lumaMLTZero = lumaMM < 0.0;
/*--------------------------------------------------------------------------*/
	lumaEndN -= lumaNN * 0.5;
	lumaEndP -= lumaNN * 0.5;
	FxaaBool doneN = abs(lumaEndN) >= gradientScaled;
	FxaaBool doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P1;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P1;
	FxaaBool doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P1;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P1;
/*--------------------------------------------------------------------------*/
	if(doneNP) {
		if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
		if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
		if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
		if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
		doneN = abs(lumaEndN) >= gradientScaled;
		doneP = abs(lumaEndP) >= gradientScaled;
		if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P2;
		if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P2;
		doneNP = (!doneN) || (!doneP);
		if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P2;
		if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P2;
/*--------------------------------------------------------------------------*/
		#if (FXAA_QUALITY__PS > 3)
		if(doneNP) {
			if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
			if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
			if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
			if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
			doneN = abs(lumaEndN) >= gradientScaled;
			doneP = abs(lumaEndP) >= gradientScaled;
			if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P3;
			if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P3;
			doneNP = (!doneN) || (!doneP);
			if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P3;
			if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P3;
/*--------------------------------------------------------------------------*/
			#if (FXAA_QUALITY__PS > 4)
			if(doneNP) {
				if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
				if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
				if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
				if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
				doneN = abs(lumaEndN) >= gradientScaled;
				doneP = abs(lumaEndP) >= gradientScaled;
				if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P4;
				if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P4;
				doneNP = (!doneN) || (!doneP);
				if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P4;
				if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P4;
/*--------------------------------------------------------------------------*/
				#if (FXAA_QUALITY__PS > 5)
				if(doneNP) {
					if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
					if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
					if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
					if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
					doneN = abs(lumaEndN) >= gradientScaled;
					doneP = abs(lumaEndP) >= gradientScaled;
					if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P5;
					if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P5;
					doneNP = (!doneN) || (!doneP);
					if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P5;
					if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P5;
/*--------------------------------------------------------------------------*/
					#if (FXAA_QUALITY__PS > 6)
					if(doneNP) {
						if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
						if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
						if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
						if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
						doneN = abs(lumaEndN) >= gradientScaled;
						doneP = abs(lumaEndP) >= gradientScaled;
						if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P6;
						if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P6;
						doneNP = (!doneN) || (!doneP);
						if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P6;
						if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P6;
/*--------------------------------------------------------------------------*/
						#if (FXAA_QUALITY__PS > 7)
						if(doneNP) {
							if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
							if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
							if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
							if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
							doneN = abs(lumaEndN) >= gradientScaled;
							doneP = abs(lumaEndP) >= gradientScaled;
							if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P7;
							if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P7;
							doneNP = (!doneN) || (!doneP);
							if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P7;
							if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P7;
/*--------------------------------------------------------------------------*/
	#if (FXAA_QUALITY__PS > 8)
	if(doneNP) {
		if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
		if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
		if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
		if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
		doneN = abs(lumaEndN) >= gradientScaled;
		doneP = abs(lumaEndP) >= gradientScaled;
		if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P8;
		if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P8;
		doneNP = (!doneN) || (!doneP);
		if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P8;
		if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P8;
/*--------------------------------------------------------------------------*/
		#if (FXAA_QUALITY__PS > 9)
		if(doneNP) {
			if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
			if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
			if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
			if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
			doneN = abs(lumaEndN) >= gradientScaled;
			doneP = abs(lumaEndP) >= gradientScaled;
			if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P9;
			if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P9;
			doneNP = (!doneN) || (!doneP);
			if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P9;
			if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P9;
/*--------------------------------------------------------------------------*/
			#if (FXAA_QUALITY__PS > 10)
			if(doneNP) {
				if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
				if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
				if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
				if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
				doneN = abs(lumaEndN) >= gradientScaled;
				doneP = abs(lumaEndP) >= gradientScaled;
				if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P10;
				if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P10;
				doneNP = (!doneN) || (!doneP);
				if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P10;
				if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P10;
/*--------------------------------------------------------------------------*/
				#if (FXAA_QUALITY__PS > 11)
				if(doneNP) {
					if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
					if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
					if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
					if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
					doneN = abs(lumaEndN) >= gradientScaled;
					doneP = abs(lumaEndP) >= gradientScaled;
					if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P11;
					if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P11;
					doneNP = (!doneN) || (!doneP);
					if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P11;
					if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P11;
/*--------------------------------------------------------------------------*/
					#if (FXAA_QUALITY__PS > 12)
					if(doneNP) {
						if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
						if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
						if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
						if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
						doneN = abs(lumaEndN) >= gradientScaled;
						doneP = abs(lumaEndP) >= gradientScaled;
						if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P12;
						if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P12;
						doneNP = (!doneN) || (!doneP);
						if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P12;
						if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P12;
/*--------------------------------------------------------------------------*/
					}
					#endif
/*--------------------------------------------------------------------------*/
				}
				#endif
/*--------------------------------------------------------------------------*/
			}
			#endif
/*--------------------------------------------------------------------------*/
		}
		#endif
/*--------------------------------------------------------------------------*/
	}
	#endif
/*--------------------------------------------------------------------------*/
						}
						#endif
/*--------------------------------------------------------------------------*/
					}
					#endif
/*--------------------------------------------------------------------------*/
				}
				#endif
/*--------------------------------------------------------------------------*/
			}
			#endif
/*--------------------------------------------------------------------------*/
		}
		#endif
/*--------------------------------------------------------------------------*/
	}
/*--------------------------------------------------------------------------*/
	// Fetch texel with sub-pixel offset based on frag's distance to edge end
	FxaaFloat dstN = posM.x - posN.x;
	FxaaFloat dstP = posP.x - posM.x;
	// FIXME: make a FxaaBool straightHorz
	if(straightSpan && !horzSpan) dstN = posM.y - posN.y;
	if(straightSpan && !horzSpan) dstP = posP.y - posM.y;

	/* Decrement diagonal search counts so texels that connect straight spans
	and diagonal spans are only counted in the straight span's search */
		/* FIXME: Commented out because some diagonal searches stop on the
		straight span and already don't count it, e.g. diagonal incline x+
		connected to a vertical span. This case could be detected by using
		lumaEndN and lumaEndP to figure out whether the connected spans are
		vertical or horizontal. While this isn't fixed, know that spanLength
		has some error on diagonal spans. */
	/*
	if(!straightSpan) dstN = max(0.0, dstN - fxaaQualityRcpFrame.x);
	if(!straightSpan) dstP = max(0.0, dstP - fxaaQualityRcpFrame.x);
	*/
/*--------------------------------------------------------------------------*/
	FxaaBool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
	FxaaFloat spanLength = (dstP + dstN);
	FxaaBool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
	FxaaFloat spanLengthRcp = 1.0/spanLength;
/*--------------------------------------------------------------------------*/
	FxaaBool directionN = dstN < dstP;
	FxaaFloat dst = min(dstN, dstP);
	FxaaBool goodSpan = directionN ? goodSpanN : goodSpanP;
	FxaaFloat pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
/*--------------------------------------------------------------------------*/
	/* Get integer number of texels on the edge. Note that each search
	iterator goes one texel past the edge; 2 texels are subtracted here to
	account for this. Also, the current frag isn't counted (its distance from
	itself is 0); 1 texel is added to account for this. */
	FxaaFloat spanPixels = spanLength / fxaaQualityRcpFrame.x - 1.0;
	if(straightSpan && !horzSpan) spanPixels = spanLength / fxaaQualityRcpFrame.y - 1.0;
/*--------------------------------------------------------------------------*/
	// Increase pixelOffset if edge is short to make near-diagonals less jaggy
	// Also helps hide the blend-seam between diagonal and straight edges
	/* This requires spanPixels to be precise for JAGGY_EDGE_SIZE pixels, make
	sure at least that amount of FXAA_QUALITY__P... defines are set to 1.0 */
	#define JAGGY_EDGE_SIZE 3.0
	FxaaFloat curveFrac = clamp(1.0 - (spanPixels - 1.0) / JAGGY_EDGE_SIZE, 0.0, 1.0);
	pixelOffset += 0.3 * curveFrac;

	/* FIXME: This doesn't fix the seam between straight and diagonal spans
	perfectly because the amount of blend on the end of the diagonal span
	depends on its length; the straight span can't account for it to adjust
	its own blend. Maybe the pixelOffset calculation should be changed
	to always meet an exact offset when dst is 1. Straight spans would then
	have a higher max blend, either because of the diagonal span's error in
	its length calculation or due to deliberate clamping in the shader (can
	clamp dst to 2 pixels to support both cases). That would also accomplish
	the job this curve modifier is doing now. */

	/* FIXME: There's still bad aliasing on edges that go:

		##
		  ##
		    #
			 ##
			   ##
			     #

	Good example is the edge of the aircraft in backwards view. Related to the
	FIXME above? */

	/*
	if(abs(spanPixels - 2.0) <= 0.1 && goodSpan == false)
	{
		pixelOffset *= 0.5;
		goodSpan = true;
	}
	else
	{
		pixelOffset += 0.2 * curveFrac;
	}
	*/

	/*if(curveFrac > 0.0)
		pixelOffset += 0.1;*/
	/*if(curveFrac < 0.97)
		pixelOffset += pixelOffset * curveFrac;
	else
		pixelOffset += 0.25;*/
/*--------------------------------------------------------------------------*/
	FxaaFloat pixelOffsetGood = (goodSpan) ? pixelOffset : 0.0;
	//FxaaFloat pixelOffsetGood = pixelOffset * (goodSpan ? 1.0 : curveFrac);
		// Make small edges get blended on both sides throughout the whole edge
	if(!straightSpan || spanPixels <= (JAGGY_EDGE_SIZE + 0.1)) pixelOffsetGood = max(0.05, pixelOffsetGood);
		/* Force at least some blending on pixels without adjacents and on
		small edges */
	FxaaFloat pixelOffsetFinal = pixelOffsetGood;
	if(smoothX) posM.x += pixelOffsetFinal * lengthSign.x;
	if(smoothY) posM.y += pixelOffsetFinal * lengthSign.y;
/*--------------------------------------------------------------------------*/
#if (BLUR_EDGES == 1)
	FxaaBool incline; /* True if the edge is visually climbing the screen when
					  going from left to right */
	if(straightSpan) incline = (goodSpanN && pairN) || (goodSpanP && !pairN);
	if(!straightSpan) incline = horzSpan;
/*--------------------------------------------------------------------------*/
	FxaaFloat2 ccw; /* Edge direction rotated counter clockwise (in OpenGL);
					not normalized, instead is the length of a single pixel
					step */
	FxaaFloat diag;

	// FIXME: Do precise slope calc for diagonal spans too?
	if(straightSpan && spanPixels > 1.0)
	{
		FxaaFloat slope = 1.5 / spanPixels;

		ccw.x = slope;
		ccw.y = 1.0;

		if(!horzSpan)
		{
			ccw.x = 1.0;
			ccw.y = slope;
		}

		diag = min(slope, 1.0);
	}
	else
	{
		ccw.x = 1.0;
		ccw.y = 1.0;
		diag = 1.0;
	}

	if(incline) ccw.x = -ccw.x;
/*--------------------------------------------------------------------------*/
	// FIXME: Blurring is still creating some aliasing noticeable to the eye

	/* FIXME: Warp is less noticeable on angled edges. Probably caused by several things:
		*Blur amplifies aliasing which might hide warp in the noise
		*Diagonal ccw doesn't take into account entire edge's angle
		*Pixel geometry on angled edges inherently doesn't allow for as noticeable of a warp
			Currently increasing warp amount for angled edges, helps a little
	*/

	FxaaFloat4 blurA = FxaaTexTop(tex, posM + ccw * fxaaQualityRcpFrame);
	FxaaFloat lumaA = FxaaLuma(blurA);
	FxaaFloat goodBlurA = min(gradient / max(0.0001, abs(lumaA - lumaM)), 1.0);
		/* Reduce blur as fetched texel's luminance goes out of range of
		original edge's gradient; prevents sparkles caused by background edges
		blurring foreground edges */

	FxaaFloat4 blurB = FxaaTexTop(tex, posM - ccw * fxaaQualityRcpFrame);
	FxaaFloat lumaB = FxaaLuma(blurB);
	FxaaFloat goodBlurB = min(gradient / max(0.0001, abs(lumaB - lumaM)), 1.0);

	FxaaFloat innerBlur = goodSpan ? 1.0 : clamp((dst - (horzSpan ? fxaaQualityRcpFrame.x : fxaaQualityRcpFrame.y) * 0.5) * spanLengthRcp, 0, 1);
		/* Reduce blur as we approach the "outer" end of the edge because it's
		about to step and blurring here would create aliasing as a result */

	FxaaFloat baseBlurWeight = innerBlur;
	FxaaFloat blurWeightA = goodBlurA * baseBlurWeight;
	FxaaFloat blurWeightB = goodBlurB * baseBlurWeight;
	FxaaFloat middleWeight = 1.0; // + gradient;

/*--------------------------------------------------------------------------*/
#if (WARP_EDGES == 1)
	// Warp edge to emulate grains' inability to render perfect lines
	FxaaFloat warp = SimplexNoise(warpPos, warpSeed) * mix(warpStraight, warpDiagonal, diag); //* (1.0 - gradient);
	blurWeightA = max(0.0, blurWeightA + warp);
	blurWeightB = max(0.0, blurWeightB - warp);
#endif
/*--------------------------------------------------------------------------*/
	return (FxaaTexTop(tex, posM) * middleWeight + blurA * blurWeightA + blurB * blurWeightB) / (middleWeight + blurWeightA + blurWeightB);
#else
	return FxaaTexTop(tex, posM);
#endif
/*--------------------------------------------------------------------------*/
	// FIXME: Optimize, remove unused branches and parameters
		
	//return vec4(max(0.0, -perp.x), max(0.0, perp.x), perp.y, 1.0);
	//return vec4(dir.x, max(0.0, dir.y), max(0.0, -dir.y), 1.0);
	//return vec4(goodSpanN ? 1 : 0, goodSpanP ? 1 : 0, lengthSign > 0.0 ? 1 : 0, 1.0);
	//return vec4(incline ? 0 : 1, incline ? 1 : 0, 0.0, 1.0);
	//return vec4(lengthSign > 0.0 ? 1 : 0, lengthSign < 0.0 ? 1 : 0, 0.0, 1.0);
	//return vec4(goodSpanN ? 1 : 0, goodSpanP ? 1 : 0, (!goodSpanN && !goodSpanP) ? 1 : 0, 1.0);
	//return vec4(pairN ? 0 : 1, pairN ? 1 : 0, 0.0, 1.0);
	//return vec4(goodSpanN ? 1 : 0, goodSpanP ? 1 : 0, pairN ? 1 : 0, 1.0);
	//return vec4(1.0 - gradient, gradient, 0.0, 1.0);
	//return vec4(blurMix / 0.4, 0.0, 0.0, 1.0);
	//return vec4(diagonal, 0.0, 0.0, 1.0);
	//return vec4(lengthSign < 0.0 ? pixelOffsetGood : 0.0, lengthSign > 0.0 ? pixelOffsetGood : 0.0, goodSpan ? 0.0 : pixelOffset, 1.0);
	//return vec4(curveFrac, 0.0, 0.0, 1.0);
	//return vec4(curveFrac >= 0.97, 0.0, 0.0, 1.0);
	//return vec4(max(pixelOffset, pixelOffsetGood), 0.0, 0.0, 1.0);
	//return vec4(abs(edgeHorz - edgeVert), 0.0, 0.0, 1.0);
	//return vec4((straightSpan ? 1 : 0) * (10.0 / 255.0) + (horzSpan ? 1 : 0) * (5.0 / 255.0), spanPixels / 255.0, blurWeightA, 1.0);
	/*
	if(!straightSpan)
		//return vec4(0.0, horzSpan, !horzSpan, 1.0);
		//return vec4(!pairN, pairN, floor(spanPixels + 0.5) / 255.0, 1.0);
		return vec4(pixelOffset, 0.0, 0.0, 1.0);
	else
		return FxaaTexTop(tex, pos);
	//*/
	/*
	if(straightSpan)
		return vec4(!pairN, pairN, floor(spanPixels + 0.5) / 255.0, 1.0);
	else
		return FxaaTexTop(tex, pos);
	//*/
	//return vec4(straightSpan, horzSpan, floor(spanPixels + 0.5) / 255.0, 1.0);
	//return vec4(straightSpan, pixelOffset, floor(spanPixels + 0.5) / 255.0, 1.0);
	//return vec4(1.0 - goodBlurA, 1.0 - goodBlurB, 0.0, 1.0);
	//return vec4(0.0, diag, 0.0, 1.0);
	//return (FxaaTexTop(tex, posM) + FxaaTexTop(tex, posM + perp * fxaaQualityRcpFrame) + FxaaTexTop(tex, posM - perp * fxaaQualityRcpFrame)) / 3.0;
	//return mix(FxaaTexTop(tex, posM), FxaaTexTop(tex, posM + perp * fxaaQualityRcpFrame), blurMix);
	//return FxaaTexTop(tex, posM);
}
/*==========================================================================*/
#endif