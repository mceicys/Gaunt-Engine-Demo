// render_shader.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"

/*
SPACES

m	Model Space - Coordinates relative to a model's position/direction. Right handed. Z is
	height.

w	World Space - Space used in simulation. Model vertices are rotated by the model's world
	orientation and translated by the model's world position.

f	Focal Space - World space transformed so the camera's position is a zero vector.

v	View Space - World space transformed so the camera's position is a zero vector and its
	orientation is the identity.

c	Clip Space - View space multiplied by projection matrix. Handed off to OpenGL. XY range
	[-1.0, 1.0]. Z range [1, 0] (reverse depth).

n	NDC Space - Clip space divided by W.

t	TDC Space - Texture device coordinates. NDC in range [0.0, 1.0]. For fetching from
	framebuffers.

s	Screen Space - Pixel coordinates on the viewport. x right, y up. Depth is [0.0, 1.0].
	gl_FragCoord returns this.

u	User Space - Equal to screen space. UI coordinates use this space.

	Normal Space - Normals are put into this space before encoding them to the geometry buffer.
	Equal to world space.
*/

/*
	MATRIX NOTE

	Matrix properties were chosen to avoid transposition when sending matrices to OpenGL while
	still keeping memory layout intuitive with C++ syntax and keeping multiplication order
	consistent across the engine and OpenGL.

	Engine
	* Memory order by row
	* Column-dot-row
		* "Dimension rule" flipped as a result
	* Column vectors
	* Multiplication order is operation order

	ex.
	[x]		[1,	0,	0,	tx]		[x + tx]
	[y]		[0,	1,	0,	ty]		[y + ty]
	[z]	*	[0,	0,	1,	tz]	=	[z + tz]
	[1]		[0,	0,	0,	1 ]		[  1   ]
	4x1		4x4					4x1

	OpenGL
	* Memory order by column (immutable)
	* Row-dot-column (immutable)
	* Row vectors
	* Multiplication order is operation order

	ex.
						[1,		0,	0,	0]
						[0,		1,	0,	0]
	[x,	y,	z,	1]	*	[0,		0,	1,	0]	=	[x + tx,	y + ty,	z + tz,	1]
						[tx,	ty,	tz,	1]
	1x4					4x4						1x4

	The example vector and matrix have an equivalent memory layout in the engine and OpenGL.
*/

namespace rnd
{
	const char
		*shaderDefineMipMapping,
		*shaderDefineMipFade,
		*shaderDefineFilterBrightness,
		*shaderDefineFilterColor,
		*shaderDefineAnchorTexelNormals,
		*shaderDefineFXAAGather4;
}

/*--------------------------------------
	rnd::SetShaderDefines
--------------------------------------*/
void rnd::SetShaderDefines()
{
	#define tempStr "\n#define MIP_MAPPING "
	shaderDefineMipMapping = mipMapping.Bool() ? tempStr "1" : tempStr "0";

	#undef tempStr
	#define tempStr "\n#define MIP_FADE "
	shaderDefineMipFade = mipFade.Bool() ? tempStr "1" : tempStr "0";

	#undef tempStr
	#define tempStr "\n#define FILTER_BRIGHTNESS "

	switch(filterTextureBrightness.Integer())
	{
		case 1: shaderDefineFilterBrightness = tempStr "1"; break;
		case 2: shaderDefineFilterBrightness = tempStr "2"; break;
		default: shaderDefineFilterBrightness = tempStr "0";
	}
	
	#undef tempStr
	#define tempStr "\n#define FILTER_COLOR "
	shaderDefineFilterColor = filterTextureColor.Bool() ? tempStr "1" : tempStr "0";

	#undef tempStr
	#define tempStr "\n#define ANCHOR_TEXEL_NORMALS "
	shaderDefineAnchorTexelNormals = anchorTexelNormals.Bool() ? tempStr "1" : tempStr "0";

	shaderDefineFXAAGather4 = forceNoGather4.Bool() ? "\n#define FXAA_GATHER4_ALPHA 0" : "";

	#undef tempStr
}

/*
################################################################################################


	GENERAL SHADER FUNCS


################################################################################################
*/

#define SHADER_FUNC_DITHER_INDEX \
	"int DitherIndex()" \
	"{" \
		"return int(mod(gl_FragCoord.x, 4)) + int(mod(gl_FragCoord.y, 4)) * 4;" \
	"}"

#define SHADER_FUNC_HALF_DITHER_INDEX \
	"int HalfDitherIndex()" \
	"{" \
		"return int(mod(gl_FragCoord.x * 0.5, 4)) + int(mod(gl_FragCoord.y * 0.5, 4)) * 4;" \
	"}"

/*
Each component of remainders must be either 0.0 or 1.0 (e.g. calculated with
mod(floor(gl_FragCoord.xy), 2)).

First line's pattern:
0.01 0.66
1.00 0.33

Second line's pattern:
0.00 0.50
0.75 0.25
*/
#define SHADER_FUNC_STIPPLE \
	"float Stipple(vec2 remainders)" \
	"{" \
		"return abs(1.0 - remainders.x * 0.67 - remainders.y * 0.99);" \
		/*"return abs(0.75 - remainders.x * 0.5 - remainders.y * 0.75);"*/ \
	"}"

// [0, 1)
// FIXME: has visible pattern
#define SHADER_FUNC_RANDOM \
	"float Random(vec2 screen, float s)" \
	"{" \
		"return fract(sin(dot(screen, vec2(12.9898, 78.233))) * 43758.5453 * s);" \
	"}"

#define SHADER_FUNC_RANDOM2 \
	"vec2 Random2(vec2 screen, float s)" \
	"{" \
		"vec2 d2 = vec2(" \
			"dot(screen, vec2(125.9898 + s * 3.5, 78.233 - s * 6.0))," \
			"dot(screen, vec2(58.0317 - s * 2.6, 934.737 + s * 1.4))" \
		");" \
		\
		"return fract(sin(d2) * 43758.5453 * s);" \
	"}"

// ~[-1, 1]
#define SHADER_FUNC_SIMPLEX_NOISE \
	"float SimplexNoise(vec2 screen, float s)" \
	"{" \
		"const float TO_SKEWED = 0.36602540378443864676372317075294;" /* (sqrt(3) - 1) / 2 */ \
		"const float TO_SCREEN = 0.21132486540518711774542560974902;" /* (3 - sqrt(3)) / 6 */ \
		"screen += 10.583;" /* Offset so low-left corner isn't 0, 0 */ /* FIXME: Make this a parameter */ \
		"vec2 skewed = screen + (screen.x + screen.y) * TO_SKEWED;" \
		"vec2 origin = floor(skewed);" \
		"vec2 dif = screen - origin + (origin.x + origin.y) * TO_SCREEN;" \
		"vec2 difFar = dif - 0.57735026918962576450914878050196;" /* dif - 1.0 + 2.0 * TO_SCREEN */ \
		"float cmp = step(dif.y, dif.x);" \
		"vec2 sideOff = vec2(cmp, 1.0 - cmp);" /* dif.x > dif.y ? vec2(1, 0) : vec2(0, 1) */ \
			/* This works despite dif being in screen space because the boundary line between
			two simplices in a grid quad has the same angle (45 deg) skewed and unskewed */ \
		"vec2 difSide = dif - sideOff + TO_SCREEN;" \
		"float sum = 0.0;" \
		\
		"vec2 grad = 1.0 - 2.0 * Random2(origin, s);" \
		"float f = max(0.0, 0.5 - dot(dif, dif));" \
		"f *= f;" /* FIXME: do a pow instead with a parameter for the power, but the sum multiplier will have to change too */ \
		"sum += f * f * dot(grad, dif);" \
		\
		"grad = 1.0 - 2.0 * Random2(origin + 1.0, s);" \
		"f = max(0.0, 0.5 - dot(difFar, difFar));" \
		"f *= f;" \
		"sum += f * f * dot(grad, difFar);" \
		\
		"grad = 1.0 - 2.0 * Random2(origin + sideOff, s);" \
		"f = max(0.0, 0.5 - dot(difSide, difSide));" \
		"f *= f;" \
		"sum += f * f * dot(grad, difSide);" \
		\
		/* Multiply by magic number to make largest sum ~1.0 (at triangle center if all gradients are facing it) */ \
		"return sum * 70.0;" \
	"}"

#define NORMAL_PACKED_TYPE "vec3"

#define SHADER_FUNC_PACK_NORMAL \
	"vec3 PackNormal(vec3 n)" \
	"{" \
		"return n * 0.5 + 0.5;" \
	"}"

#define SHADER_FUNC_UNPACK_NORMAL \
	"vec4 UnpackNormal(vec4 gFrag)" \
	"{" \
		"vec3 n = normalize(vec3(gFrag.g, gFrag.b, gFrag.a) * 2.0 - 1.0);" \
		"return vec4(n.x, n.y, n.z, 1.0);" \
	"}"

// influence [0.0, 1.0]
// packedLightSub [0.0, topLightSub / bufferMask]
#define SHADER_FUNC_PACK_LIGHT \
	"float PackLight(float influence, float packedLightSub)" \
	"{" \
		"return (floor(clamp(influence, 0.0, 1.0) * topInfluence) * influenceFactor + packedLightSub);" \
	"}"

/* FIXME:
https://forum.unity.com/threads/the-quest-for-efficient-per-texel-lighting.529948/
Linked solution treats differentials like a 2x2 matrix and inverts it; seems to avoid branching without introducing divide-by-zero artifacts
float2x2 dSTdUV = float2x2(dUVdT[1], -dUVdT[0], -dUVdS[1], dUVdS[0])*(1.0f/(dUVdS[0]*dUVdT[1]-dUVdT[0]*dUVdS[1]));
*/

/* FIXME: on high graphics setting, should do multi-buffer rendering so an anchor-delta (not
sure in what space, something that can use 8-bit components efficiently) can be saved and used
by lighting shaders to fix its position */
#define SHADER_FUNC_TEXEL_ANCHOR \
	"vec2 TexelAnchor(vec2 anchorDif, vec2 texelPerFragX, vec2 texelPerFragY)" \
	"{" \
		/* Find anchorDif in fragments by solving this system of linear equations:
		anchorDif.s = texelPerFragX.s * fragDif.x + texelPerFragY.s * fragDif.y
		anchorDif.t = texelPerFragX.t * fragDif.x + texelPerFragY.t * fragDif.y */ \
		"vec2 fragDif;" \
		\
		"if(texelPerFragY.s == 0.0) {" \
			"fragDif.x = anchorDif.s / texelPerFragX.s;" \
			"fragDif.y = (anchorDif.t - texelPerFragX.t * fragDif.x) / texelPerFragY.t;" \
				/* Assuming texelPerFragY.t != 0 since texelPerFragY.s == 0 */ \
					/* Triangles with no UV area will display undefined artifacts */ \
		"} else {" \
			"float ytOverYS = texelPerFragY.t / texelPerFragY.s;" \
			"fragDif.x = (anchorDif.t - anchorDif.s * ytOverYS) / (texelPerFragX.t - texelPerFragX.s * ytOverYS);" \
			"fragDif.y = (anchorDif.s - texelPerFragX.s * fragDif.x) / texelPerFragY.s;" \
		"}" \
		\
		"return fragDif;" \
	"}"

// https://community.khronos.org/t/mipmap-level-calculation-using-dfdx-dfdy/67480
#define SHADER_FUNC_LEVEL_OF_DETAIL \
	"float LevelOfDetail(vec2 texel)" \
	"{" \
		"vec2 dtx = dFdx(texel);" \
		"vec2 dty = dFdy(texel);" \
		"float magSq = max(dot(dtx, dtx), dot(dty, dty));" \
		"return log2(magSq) * 0.5;" \
	"}"

#define SHADER_FUNC_FILTERED_COLOR_IDX \
	"float FilteredColorIdx(vec2 tc, vec2 origTexel, float mipBias)" \
	"{" \
		"\n#if (DO_DITHER == 1)\n" \
			"int index = DitherIndex();" \
			"float ditherValue = dither[index];" \
		"\n#endif\n" \
		\
		"#if (FILTER_BRIGHTNESS == 2) || (FILTER_COLOR == 1)\n" \
			"float ditherSigned = ditherValue - 0.46875;" /* Puts into [-0.46875, 0.46875] range */ \
		"\n#endif\n" \
		\
		"#if (CUSTOM_LOD == 1)\n" \
			"#if (MIP_FADE == 1)\n" \
				"#define ADD_DITHERED_MIP_FADE_PLACEHOLDER + ditherValue * mipFade\n" \
			"#else\n" \
				"#define ADD_DITHERED_MIP_FADE_PLACEHOLDER\n" \
			"#endif\n" \
			\
			"int lod = max(0, int(LevelOfDetail(origTexel) + mipBias ADD_DITHERED_MIP_FADE_PLACEHOLDER));" \
		"\n#else\n" \
			"#define lod 0\n" \
		"#endif\n" \
		\
		"#if (GET_DIMS == 1)\n" \
			"vec2 dims = textureSize(texture, lod);" \
		"\n#endif\n" \
		\
		"#if (FILTER_COLOR == 1)\n" \
			"#define ADD_DITHERED_COLOR_PLACEHOLDER + ditherSigned / dims\n" \
		"#else\n" \
			"#define ADD_DITHERED_COLOR_PLACEHOLDER\n" \
		"#endif\n" \
		\
		/* FIXME: This doubles render time (e.g. starting scene, 0.29 ms to 0.58 ms).
		Try optimizing by storing lum values in texture alpha and letting driver filter it for us.
		For the discard color, make the lum an average of adjacent texel lums. Bright colors
		(single-color ramps) should have a lum of 1.0 */ \
		"#if (FILTER_BRIGHTNESS > 0)\n" \
			"vec2 texel = tc * dims;" \
			"vec2 fracs = fract(texel) - 0.5;" \
			"vec2 signs = (step(-fracs, vec2(0.0, 0.0)) * 2.0 - 1.0) / dims;" \
			"float m = textureLod(texture, tc, lod).r;" \
			"float e = textureLod(texture, tc + vec2(signs.x, 0.0), lod).r;" \
			"float n = textureLod(texture, tc + vec2(0.0, signs.y), lod).r;" \
			"float ne = textureLod(texture, tc + signs, lod).r;" \
			"vec4 mramp = texture1D(rampLookup, m);" /* FIXME: 1D deprecated in 130 but overloaded func doesn't exist on GTX 260... */ \
			"vec4 eramp = texture1D(rampLookup, e);" \
			"vec4 nramp = texture1D(rampLookup, n);" \
			"vec4 neramp = texture1D(rampLookup, ne);" \
			"float mlum = mramp.b > 0.0 ? mramp.a / mramp.b : 0.0;" \
			"float elum = eramp.b > 0.0 ? eramp.a / eramp.b : 0.0;" \
			"float nlum = nramp.b > 0.0 ? nramp.a / nramp.b : 0.0;" \
			"float nelum = neramp.b > 0.0 ? neramp.a / neramp.b : 0.0;" \
			"vec2 afracs = abs(fracs);" \
			"float lum = (mlum + (elum - mlum) * afracs.x) * (1.0 - afracs.y) + (nlum + (nelum - nlum) * afracs.x) * afracs.y;" \
			\
			"float d = textureLod(texture, tc ADD_DITHERED_COLOR_PLACEHOLDER, lod).r;" \
			"vec4 dramp = texture1D(rampLookup, d);" \
			"float drampStart = dramp.r + dramp.g * 0.0039215686274509803921568627451;" \
			"float drampLength = dramp.b * rampDistScale;" \
			"float drampEnd = drampStart + drampLength;" \
		"\n#endif\n" \
		\
		"#if (FILTER_BRIGHTNESS == 2)\n" \
			"#define ADD_DITHERED_BRIGHTNESS_PLACEHOLDER + ditherSigned * invNumRampTexels\n" \
		"#else\n" \
			"#define ADD_DITHERED_BRIGHTNESS_PLACEHOLDER\n" \
		"#endif\n" \
		\
		"#if (FILTER_BRIGHTNESS > 0)\n" \
			"return texture1D(ramps, clamp(drampStart + drampLength * lum ADD_DITHERED_BRIGHTNESS_PLACEHOLDER, drampStart, drampEnd)).a;" \
		"\n#elif (CUSTOM_LOD == 1)\n" \
			"return textureLod(texture, tc ADD_DITHERED_COLOR_PLACEHOLDER, lod).r;" \
		"\n#else\n" \
			/* LOD is a level below expected when doing color dithering here because it
			increases the differential; made FILTER_COLOR set CUSTOM_LOD for that reason */ \
			"return texture2D(texture, tc, mipBias).r;" \
		"\n#endif\n" \
	"}"

#define SHADER_FILTERED_COLOR_IDX_EXTRA_DEFINES \
	"\n#if (MIP_FADE == 1) || (FILTER_BRIGHTNESS > 0) || (FILTER_COLOR == 1)\n" \
		"#define DO_DITHER 1\n" \
	"#else\n" \
		"#define DO_DITHER 0\n" \
	"#endif\n" \
	\
	"\n#if (MIP_MAPPING == 1) && ((MIP_FADE == 1) || (FILTER_BRIGHTNESS > 0) || (FILTER_COLOR == 1))\n" \
		"#define CUSTOM_LOD 1\n" \
	"#else\n" \
		"#define CUSTOM_LOD 0\n" \
	"#endif\n" \
	\
	"\n#if (FILTER_BRIGHTNESS > 0) || (FILTER_COLOR == 1)\n" \
		"#define GET_DIMS 1\n" \
	"#else\n" \
		"#define GET_DIMS 0\n" \
	"#endif\n"

#define SHADER_FILTERED_COLOR_IDX_AUTO_DECLARE_DITHER \
	"\n#if (DO_DITHER == 1)\n" \
		"uniform float dither[16];" \
		SHADER_FUNC_DITHER_INDEX \
	"\n#endif\n"

#define SHADER_FILTERED_COLOR_IDX_AUTO_DECLARATIONS \
	"\n#if (MIP_FADE == 1)\n" \
		"uniform float mipFade;" \
	"\n#endif\n" \
	\
	"\n#if (FILTER_BRIGHTNESS > 0)\n" \
		"uniform float rampDistScale;" \
		"uniform sampler1D ramps;" \
		"uniform sampler1D rampLookup;" \
	"\n#endif\n" \
	\
	"\n#if (FILTER_BRIGHTNESS == 2)\n" \
		"uniform float invNumRampTexels;" \
	"\n#endif\n" \
	\
	"uniform float mipBias;" \
	\
	"\n#if (CUSTOM_LOD == 1)\n" \
		SHADER_FUNC_LEVEL_OF_DETAIL \
	"\n#endif\n"

/*
################################################################################################


	SIMPLE


################################################################################################
*/

char* rnd::vertSimpleSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"void main()"
	"{"
		"gl_Position = pos;"
	"}"
};

char* rnd::fragSimpleSource = {
	"#version 120\n"

	"void main()"
	"{"
		"gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
	"}"
};

/*
################################################################################################


	WORLD

Draws world space triangles. The color buffer contains the color index in the R component and
the world space normal vector in the G, B, and A components. The depth buffer contains the
geometry's depth.
################################################################################################
*/

// FIXME: smooth normals w/ per-texel snapping
char* rnd::vertWorldSource = {
	"#version 120\n"

	"attribute vec4 pos;" // world space
	"attribute vec2 texCoord;"
	"attribute vec3 norm;" // world space
	"attribute float subPalette;"

	"uniform mat4 worldToClip;"
	"uniform vec2 texDims;" // Only needed when CUSTOM_LOD is 1
	"uniform float subPaletteFactor;"

	"varying vec2 tc;"
	"varying vec2 origTexel;"
	"varying " NORMAL_PACKED_TYPE " packedNorm;"
	"varying float sp;"

	SHADER_FUNC_PACK_NORMAL

	"void main()"
	"{"
		"tc = texCoord;"
		"origTexel = texCoord * texDims;"
		"packedNorm = PackNormal(norm);"
		"sp = subPalette * subPaletteFactor;"
		"gl_Position = pos * worldToClip;"
	"}"
};

char* rnd::fragWorldSource = {
	"uniform sampler2D texture;"
	"uniform sampler2D subPalettes;"

	"in vec2 tc;"
	"in vec2 origTexel;"
	"in " NORMAL_PACKED_TYPE " packedNorm;"
	"in float sp;"

	SHADER_FILTERED_COLOR_IDX_EXTRA_DEFINES
	SHADER_FILTERED_COLOR_IDX_AUTO_DECLARE_DITHER
	SHADER_FILTERED_COLOR_IDX_AUTO_DECLARATIONS
	SHADER_FUNC_FILTERED_COLOR_IDX

	"void main()"
	"{"
		"gl_FragColor = vec4(" // FIXME: is gl_FragColor deprecated in 130?
			"texture2D(subPalettes, vec2(FilteredColorIdx(tc, origTexel, mipBias), sp)).r,"
			"packedNorm.x,"
			"packedNorm.y,"
			"packedNorm.z"
		");"
	"}"
};

char* rnd::vertWorldGlassSource = {
	"#version 120\n"

	"attribute vec4 pos;" // world space
	"attribute float intensity;"

	"uniform mat4 worldToClip;"

	"varying float intensityOut;"

	"void main()"
	"{"
		"intensityOut = intensity;"
		"gl_Position = pos * worldToClip;"
	"}"
};

// FIXME: snap to texels using texCoords and texture dimensions
char* rnd::fragWorldGlassSource = {
	"#version 120\n"

	"uniform float intensityFactor;"
	"uniform float intensityExponent;"

	"varying float intensityOut;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"pow(max(intensityOut, 0.0), intensityExponent) * intensityFactor,"
			"0.0,"
			"0.0,"
			"0.0"
		");"
	"}"
};

/*
################################################################################################


	MODEL

Draws model space triangles. The color buffer contains the color index in the R component and
the world space normal vector in the G, B, and A components. The depth buffer contains the
geometry's depth.
################################################################################################
*/

char* rnd::vertModelSource = {
	"#version 120\n"

	// model space
	"attribute vec4 pos0;"
	"attribute vec4 pos1;"

	"attribute vec2 texCoord;"

	// model space
	"attribute vec4 norm0;"
	"attribute vec4 norm1;"

	"uniform float lerp;"
	"uniform vec2 texShift;"
	"uniform vec2 texDims;"
	"uniform mat4 modelToClip;"
	"uniform mat4 modelToNormal;"

	"varying vec2 tc;"
	"varying vec2 origTexel;"
	"varying " NORMAL_PACKED_TYPE " packedNorm;"

	SHADER_FUNC_PACK_NORMAL

	"void main()"
	"{"
		"tc = texCoord + texShift;"
		"origTexel = tc * texDims;"
		"packedNorm = PackNormal(vec3(mix(norm0, norm1, lerp) * modelToNormal));"
		"gl_Position = mix(pos0, pos1, lerp) * modelToClip;"
	"}"
};

#if 0 // FIXME: make the transparency stuff a #define so the same shader source can be used to make two shader programs
char* rnd::fragModelSource = {
	"#version 120\n"

	"uniform float subPalette;"
	"uniform sampler2D texture;"
	"uniform sampler2D subPalettes;"

	"varying vec2 texCoordOut;"
	"varying " NORMAL_PACKED_TYPE " normOut;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"texture2D(subPalettes, vec2(texture2D(texture, texCoordOut).r, subPalette)).r,"
			"normOut.x,"
			"normOut.y,"
			"normOut.z"
		");"
	"}"
};

char* rnd::fragModelFadeSource = {
#else
char* rnd::fragModelSource = {
#endif
	"uniform float subPalette;"
	"uniform float opacity;"
	"uniform float dither[16];"
	"uniform sampler2D texture;"
	"uniform sampler2D subPalettes;"

	"in vec2 tc;"
	"in vec2 origTexel;"
	"in " NORMAL_PACKED_TYPE " packedNorm;"

	SHADER_FUNC_DITHER_INDEX
	SHADER_FUNC_TEXEL_ANCHOR

	SHADER_FILTERED_COLOR_IDX_EXTRA_DEFINES
	SHADER_FILTERED_COLOR_IDX_AUTO_DECLARATIONS
	SHADER_FUNC_FILTERED_COLOR_IDX
	
	"void main()"
	"{"
		"float color = texture2D(subPalettes, vec2(FilteredColorIdx(tc, origTexel, mipBias), subPalette)).r;"
		
		"if(dither[DitherIndex()] > opacity || color == 0.0)"
			"discard;"

		"\n#if (ANCHOR_TEXEL_NORMALS == 1)\n"
			// Keep normal constant across texel by deriving normal of texel's center
			/* UV map must not be collinear (squished) anywhere, otherwise middle of texel is an
			infinite distance away */
			"vec2 anchorDif = floor(origTexel) + 0.5 - origTexel;"
			"vec2 texelPerFragX = dFdx(origTexel);"
			"vec2 texelPerFragY = dFdy(origTexel);"
			"vec2 fragDif = TexelAnchor(anchorDif, texelPerFragX, texelPerFragY);"
		"\n#endif\n"

		"gl_FragColor = vec4("
			"color,"
			"\n#if (ANCHOR_TEXEL_NORMALS == 1)\n"
				"packedNorm + dFdx(packedNorm) * fragDif.x + dFdy(packedNorm) * fragDif.y"
			"\n#else\n"
				"packedNorm"
			"\n#endif\n"
		");"
	"}"
};

char* rnd::vertModelNoNormalSource = {
	// model space
	"attribute vec4 pos0;"
	"attribute vec4 pos1;"

	"attribute vec2 texCoord;"

	"uniform float lerp;"
	"uniform vec2 texShift;"
	"uniform mat4 modelToClip;"

	"varying vec2 tc;"

	//"\n#if (ORIG_TEXEL == 1)\n"
		"uniform vec2 texDims;"
		"varying vec2 origTexel;"
	//"\n#endif\n"

	"void main()"
	"{"
		"tc = texCoord + texShift;"

		//"\n#if (ORIG_TEXEL == 1)\n"
			"origTexel = tc * texDims;"
		//"\n#endif\n"

		"gl_Position = mix(pos0, pos1, lerp) * modelToClip;"
	"}"
};

char* rnd::fragModelAddSource = {
	"#version 120\n"

	"uniform float opacity;"
	"uniform float mipBias;"
	"uniform float additiveNormalization;"
	"uniform sampler2D texture;"
	"varying vec2 tc;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"0.0,"
			"texture2D(texture, tc, mipBias).r * opacity * additiveNormalization,"
			"0.0,"
			"0.0"
		");"
	"}"
};

char* rnd::fragModelAmbientSource = {
	"#version 120\n"

	"uniform float opacity;"
	"uniform float mipBias;"
	"uniform sampler2D texture;"
	"varying vec2 tc;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"texture2D(texture, tc, mipBias).r * opacity,"
			"0.0,"
			"0.0,"
			"0.0"
		");"
	"}"
};

char* rnd::fragModelMulSource = {
	"#version 120\n"

	"uniform float opacity;"
	"uniform float mipBias;"
	"uniform sampler2D texture;"
	"varying vec2 tc;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"(1.0 - texture2D(texture, tc, mipBias).r) * opacity,"
			"0.0,"
			"0.0,"
			"0.0"
		");"
	"}"
};

char* rnd::fragModelRecolorSource = {
	"uniform float packedSub;" // pre-packed with 0 intensity
	"uniform float subPalette;"
	"uniform float dither[16];"
	"uniform float mipBias;"
	"uniform float mipFade;"
	"uniform sampler2D texture;"
	"varying vec2 tc;"
	"varying vec2 origTexel;"

	"\n#if (MIP_MAPPING == 1) && ((MIP_FADE == 1) || (FILTER_COLOR == 1))\n"
		"#define CUSTOM_FILTER 1\n"
	"#endif\n"

	"\n#ifdef CUSTOM_FILTER\n"
		SHADER_FUNC_DITHER_INDEX
		SHADER_FUNC_LEVEL_OF_DETAIL
	"\n#endif\n"

	"void main()"
	"{"
		"\n#ifdef CUSTOM_FILTER\n"
			"int index = DitherIndex();"
			"float ditherValue = dither[index];"

			"\n#if (FILTER_COLOR == 1)\n"
				"float ditherSigned = ditherValue - 0.46875;"
			"\n#endif\n"

			"\n#if (MIP_FADE == 1)\n"
				"#define ADD_DITHERED_MIP_FADE_PLACEHOLDER + ditherValue * mipFade\n"
			"#else\n"
				"#define ADD_DITHERED_MIP_FADE_PLACEHOLDER\n"
			"#endif\n"
			
			"int lod = max(0, int(LevelOfDetail(origTexel) + mipBias ADD_DITHERED_MIP_FADE_PLACEHOLDER));"

			"\n#if (FILTER_COLOR == 1)\n"
				"vec2 dims = textureSize(texture, lod);"
				"\n#define ADD_DITHERED_COLOR_PLACEHOLDER + ditherSigned / dims\n"
			"#else\n"
				"#define ADD_DITHERED_COLOR_PLACEHOLDER\n"
			"#endif\n"

			"float red = textureLod(texture, tc ADD_DITHERED_COLOR_PLACEHOLDER, lod).r;"
		"\n#else\n"
			"float red = texture2D(texture, tc, mipBias).r;"
		"\n#endif\n"
		
		"if(red == 0.0)"
			"discard;"

		"gl_FragColor = vec4("
			"0.0,"
			"0.0,"
			"subPalette,"
			"packedSub"
		");"
	"}"
};

/*
################################################################################################


	CLOUD


################################################################################################
*/

// FIXME: polish dithered-texel lighting on clouds
// FIXME: optimize, remove experimental code

#define SHADER_FUNC_CLOUD_DENSITY \
	"float CloudDensity(vec4 pos)" \
	"{" \
		"vec3 ray = normalize(pos.xyz - modelCam);" \
		"vec3 marchVoxel = (pos * modelToVoxel).xyz;" \
		\
		/* Skip this vertex's voxel so it doesn't contribute to its own visual density */ \
			/* Note that thin parts of the cloud may disappear; meshes should try to have one
			vertex per voxel at most */ \
		/* FIXME: sign() is only officially available in glsl 130, make a substitute function */ \
		"vec3 nextVoxelDists = abs(floor(marchVoxel + sign(ray)) + 0.5 - marchVoxel);" \
			/* Per-component distance to center of next voxel in direction ray */ \
			/* Going to center to prevent filtering from grabbing original voxel */ \
		"vec3 factors = nextVoxelDists / max(abs(ray), 0.00001);" \
		"float smallFactor = min(factors.x, min(factors.y, factors.z));" \
		"marchVoxel += ray * smallFactor;" \
		\
		/* Do simple ray march to accumulate density some distance behind vertex */ \
		"vec3 marchTexel = marchVoxel * voxelInvDims;" \
		"vec3 stepTexel = ray * voxelInvDims;" /* Travel one unit in voxel-space per step */ \
		"int numSteps = 64;" /* FIXME: uniform */ \
		"float density = 0.0;" \
		/*"float barrier = 0.0;"*/ \
		\
		/* FIXME: need a way to stop marching across large gaps without making vertices flicker; negative densities act as soft barriers? */ \
		/* FIXME: make vertices close to viewer use mipmaps to grab more voxels? */ \
		"for(int i = 0; i < numSteps; i++)" \
		"{" \
			"density += texture3D(voxels, marchTexel).r * voxelScale;" \
				/* Multiplied by approximate distance travelled thru voxel */ \
			"marchTexel += stepTexel;" \
			\
			/*"density += max(0.0, texture3D(voxels, marchTexel).r - barrier) * voxelScale;"*/ \
				/* Multiplied by approximate distance travelled thru voxel */ \
			/*"marchTexel += stepTexel;"*/ \
			\
			/*"if(texture3D(voxels, marchTexel).r == 0.0) barrier += 0.1;"*/ \
			/*"if(barrier >= 1.0) break;"*/ \
		"}" \
		\
		"return density;" \
	"}"

char* rnd::vertModelCloudSource = {
	"#version 120\n"

	"attribute vec4 pos0, pos1;" // model space
	"attribute vec2 texCoord;"
	"attribute vec4 norm0, norm1;" // model space

	"uniform float lerp;"
	"uniform vec2 texShift;"
	"uniform mat4 modelToClip;"
	"uniform mat4 modelToNormal;"
	"uniform vec3 modelCam;"
	"uniform mat4 modelToVoxel;"
	"uniform vec3 voxelInvDims;"
	"uniform float voxelScale;"
	"uniform sampler3D voxels;"

	"varying vec2 texCoordOut;"
	"varying " NORMAL_PACKED_TYPE " normOut;"
	"varying float densityOut;"

	SHADER_FUNC_PACK_NORMAL
	SHADER_FUNC_CLOUD_DENSITY

	"void main()"
	"{"
		"texCoordOut = texCoord + texShift;"
		"normOut = PackNormal(vec3(mix(norm0, norm1, lerp) * modelToNormal));"
		"vec4 pos = mix(pos0, pos1, lerp);"
		"gl_Position = pos * modelToClip;"
		"densityOut = CloudDensity(pos);"
	"}"
};

char* rnd::fragModelCloudSource = {
	"#version 120\n"

	"uniform float subPalette;"
	"uniform float opacity;"
	"uniform float dither[16];"
	"uniform sampler2D texture;"
	"uniform sampler2D subPalettes;"

	"varying vec2 texCoordOut;"
	"varying " NORMAL_PACKED_TYPE " normOut;"
	"varying float densityOut;"

	"vec2 dims = vec2(128.0, 128.0);" // FIXME: uniform

	SHADER_FUNC_DITHER_INDEX // FIXME: remove if not being used
	SHADER_FUNC_HALF_DITHER_INDEX // FIXME: remove if not being used
	SHADER_FUNC_STIPPLE
	SHADER_FUNC_TEXEL_ANCHOR

	"void main()"
	"{"
		"float color = texture2D(subPalettes, vec2(texture2D(texture, texCoordOut).r, subPalette)).r;"
		"float alpha = densityOut;" // FIXME: multiply by opacity

		/* Dither alpha at half resolution to make more alpha bands and make higher-alpha clouds
		behind lower-alpha clouds smoothly fade in and out */

		// For high setting; has hard boundary at full transparency
		//*
		"if(alpha < 0.01)"
			"discard;"

		"alpha = max(0.01, alpha - dither[HalfDitherIndex()] * 0.32);"
		//*/

		//"alpha = max(0.01, alpha - dither[HalfDitherIndex()] * 0.32 * step(alpha, 0.999));"
			/* Alternative dithering that creates hard boundary at full opaqueness for max
			resolution, but it looks bad when it shows up in background clouds */

		// FIXME: enable based on quality option
		// For low/medium settings; smoothly transitions from opaque to stipple pattern
		//"alpha = alpha * 1.301 - dither[HalfDitherIndex()] * 0.32;"

		"vec2 texel = texCoordOut * dims;" // FIXME: move below discard branch if alpha is not per-texel
		"vec2 dxt = dFdx(texel);"
		"vec2 dyt = dFdy(texel);"
		"vec2 fragDif = TexelAnchor(floor(texel) + 0.5 - texel, dxt, dyt);"
		//"alpha += dFdx(alpha) * fragDif.x + dFdy(alpha) * fragDif.y;"

		// Apply alpha stipple pattern (and discard transparent texels too)
		"if(Stipple(mod(floor(gl_FragCoord.xy), 2)) > alpha || color == 0.0)"
			"discard;"

		"float mip = floor(max(0.0, 0.5 * log2(max(dot(dxt, dxt), dot(dyt, dyt))) + 0.5) + 1.5 + gl_FragCoord.z);"
																				// FIXME: add mip map bias uniform
			// +constant in max call is mip map bias
			// +1 to convert mip-level 0 to a mip-divisor of 1
			// +0.5 to round to nearest mip level
				// FIXME: input mip bias as uniform?
				// FIXME: if far-off clouds should look like they're part of a low-res painting, mip-level should be calculated differently, so it goes higher quicker
		"float d = dither[int(mod(texel.x / mip, 4)) + int(mod(texel.y / mip, 4)) * 4];"
		//"float d = dither[DitherIndex()];"

		"gl_FragColor = vec4("
			"color,"
			"floor((normOut + dFdx(normOut) * fragDif.x + dFdy(normOut) * fragDif.y) * 10.0 + d * 0.7) * 0.1" // FIXME: uniforms for imprecision and dithering factor
			//"floor(normOut * 10.0 + dither[DitherIndex()] * 0.7) * 0.1" // FIXME: less consistent in motion but may look nicer in still images
			//"floor(normOut * 8.0 + dither[DitherIndex()]) * 0.125"
			//"floor(normOut * 16.0 + dither[DitherIndex()]) * 0.0625"
			//"normOut.xyz"
		");"
	"}"
};

/*
################################################################################################


	SKY


################################################################################################
*/

char* rnd::vertSkyBoxSource = {
	"#version 120\n"

	"attribute vec4 pos;"

	"uniform mat4 toClip;"

	"varying vec3 dirOut;"

	"void main()"
	"{"
		"dirOut = pos.xyz;"

		//"vec4 posClip = pos * toClip;"
		//"gl_Position = posClip.xyww;" // Set z to w so depth is always 1.0

		"gl_Position = pos * toClip;"
		"gl_Position.z = 0.0;" // Set depth to 0.0 (farthest)
	"}"
};

char* rnd::fragSkyBoxSource = {
	"#version 120\n"

	"uniform samplerCube texture;"

	"varying vec3 dirOut;"

	"void main()"
	"{"
		"gl_FragColor = vec4("
			"textureCube(texture, dirOut).r,"
			"0, 0, 0" // Normal doesn't matter; sky is stenciled out during lighting
		");"
	"}"
};

char* rnd::fragSkySphereSource = {
	"uniform vec2 texDims;"
	"uniform float seamMipBias;"
	"uniform sampler2D texture;"

	SHADER_FILTERED_COLOR_IDX_EXTRA_DEFINES
	SHADER_FILTERED_COLOR_IDX_AUTO_DECLARE_DITHER
	SHADER_FILTERED_COLOR_IDX_AUTO_DECLARATIONS
	SHADER_FUNC_FILTERED_COLOR_IDX

	"varying vec3 dirOut;"

	"void main()"
	"{"
		//*
		"vec3 dir = normalize(dirOut);"

		"vec2 texCoord = vec2("
			"atan(-dir.y, dir.x) * 0.15915494309189533576888376337251 + 0.5,"
			"acos(dir.z) * -0.31830988618379067153776752674503 + 1.0"
			//"atan(dir.y, -dir.x) * 0.15915494309189533576888376337251,"
			//"acos(dir.z) * -0.31830988618379067153776752674503"
		");"
		//*/
		
		/*
		// https://github.com/openglsuperbible/sb6code/blob/master/bin/media/shaders/equirectangular/render.fs.glsl
		"vec3 dir = normalize(dirOut);"
		"vec2 texCoord;"
		"texCoord.y = dir.z; dir.z = 0.0;"
		"texCoord.x = normalize(dir).x * 0.5;"

		"float s = sign(dir.y) * 0.5;"

		"texCoord.s = 0.75 - s * (0.5 - texCoord.s);"
		"texCoord.t = 0.5 + 0.5 * texCoord.t;"
		//*/

		/*
		// Slightly faster but has ugly stretching at y+ and y-
		"vec3 dir = normalize(dirOut);"
		"vec2 texCoord;"
		"texCoord.y = 0.5 + 0.5 * dir.z;"
		"texCoord.x = 0.75 + sign(dir.x) * 0.5 * (0.5 - normalize(dir.xy).y * 0.5);"
		//*/

		"vec2 origTexel = texCoord * texDims;"

		// https://makc3d.wordpress.com/2017/01/19/sampling-equirectangular-textures/
		// Prevent low LOD at top and bottom
		//"float poleBias = -2.0 * log2(1.0 + dir.z * dir.z);"

		// Hide LOD seam
		"const float SEAM_FACTOR = 200.0;"
		"float seam = max(0.0, 1.0 - abs(dir.y) * SEAM_FACTOR) * clamp(1.0 - dir.x * SEAM_FACTOR, 0.0, 1.0);"

		"gl_FragColor = vec4("
			//"texture2D(texture, texCoord).r,"
			"FilteredColorIdx(texCoord, origTexel, mipBias + seam * seamMipBias),"
			"0, 0, 0" // Normal doesn't matter; sky is stenciled out during lighting
		");"
	"}"
};

char* rnd::vertSkyLightSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"void main()"
	"{"
		"gl_Position = pos;"
	"}"
};

char* rnd::fragSkyLightSource = {
	"#version 120\n"

	"uniform float intensity;"
	"uniform float subPalette;" // pre-packed

	"void main()"
	"{"
		"gl_FragColor = vec4(intensity, 0.0, 0.0, subPalette);"
	"}"
};

/*
################################################################################################


	AMBIENT LIGHT


################################################################################################
*/

char* rnd::vertAmbientSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"void main()"
	"{"
		"gl_Position = pos;"
	"}"
};

char* rnd::fragAmbientSource = {
	"#version 120\n"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"

	"uniform float ambSubPalette;"
	"uniform float ambIntensity;"
	"uniform float ambColorSmooth;"
	"uniform float ambColorPriority;"

	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		"float rand = Random(gl_FragCoord.xy, seed);"

		"gl_FragColor = vec4("
			"ambIntensity,"
			"0.0,"
			"0.0,"
			"PackLight("
				"ambIntensity * mix(1.0, rand, ambColorSmooth) * ambColorPriority,"
				"ambSubPalette"
			")"
		");"
	"}"
};

/*
################################################################################################


	SHADOW

Draws geometry depth.
################################################################################################
*/

#define VERT_SHADOW_SOURCE \
	"#version 120\n" \
	\
	"attribute vec4 pos0;" \
	"attribute vec4 pos1;" \
	VERT_SHADOW_SOURCE_DECLARE_TEX_COORD \
	"attribute vec4 norm0;" \
	"attribute vec4 norm1;" \
	"uniform float lerp;" \
	"uniform mat4 toClip;" \
	"uniform float vertexBias;" /* Vertex bias hides dark surface edges from PCF'd shadows */ \
	"uniform vec3 bulbPos;" /* Same space as pos attributes */ \
	\
	"void main()" \
	"{" \
		"vec4 pos = mix(pos0, pos1, lerp);" \
		"vec3 norm = mix(norm0.xyz, norm1.xyz, lerp);" \
		"vec3 bias = normalize(pos.xyz - bulbPos);" \
		"bias *= (1.0 - abs(dot(bias, norm))) * vertexBias;" \
		\
		/* Calculate vertex-biased and non-biased position and only use depth value of biased
		version to avoid creating holes in rasterization */ \
		/* FIXME: Dumb, find similar or better bias that's applied to depth directly */ \
		"vec4 clipBiased = (pos + vec4(bias.x, bias.y, bias.z, 0.0)) * toClip;" \
		"gl_Position = pos * toClip;" \
		/* FIXME: Can this branch be avoided? */ \
		"if(clipBiased.w != 0.0)" /* Avoid division by zero if vertex is on camera's plane */ \
			"gl_Position.z = (clipBiased.z / clipBiased.w) * gl_Position.w;" \
		\
		VERT_SHADOW_SOURCE_SET_TEX_COORD \
	"}"

#define VERT_SHADOW_SOURCE_DECLARE_TEX_COORD ""
#define VERT_SHADOW_SOURCE_SET_TEX_COORD ""

char* rnd::vertShadowSource = {
	VERT_SHADOW_SOURCE
};

char* rnd::fragShadowSource = {
	"#version 120\n"

	"void main()"
	"{"
		"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);"
	"}"
};

#undef VERT_SHADOW_SOURCE_DECLARE_TEX_COORD
#define VERT_SHADOW_SOURCE_DECLARE_TEX_COORD \
	"attribute vec2 texCoord;" \
	"varying vec2 texCoordOut;" \
	"uniform vec2 texShift;"

#undef VERT_SHADOW_SOURCE_SET_TEX_COORD
#define VERT_SHADOW_SOURCE_SET_TEX_COORD "texCoordOut = texCoord + texShift;"

char* rnd::vertShadowFadeSource = {
	VERT_SHADOW_SOURCE
};

char* rnd::fragShadowFadeSource = {
	"#version 120\n"

	"uniform float opacity;"
	"uniform float seed;"
	"uniform sampler2D texture;"
	"varying vec2 texCoordOut;"

	SHADER_FUNC_RANDOM

	"void main()"
	"{"
		"if(Random(gl_FragCoord.xy, seed) > opacity ||"
		"texture2D(texture, texCoordOut).r == 0.0)"
			"discard;"

		"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);"
	"}"
};

char* rnd::vertShadowSunSource = {
	"#version 120\n"

	"attribute vec4 pos0;"
	"attribute vec4 pos1;"
	"uniform float lerp;"
	"uniform mat4 toClip;"

	"void main()"
	"{"
		"gl_Position = mix(pos0, pos1, lerp) * toClip;"
	"}"
};

char* rnd::vertShadowSunFadeSource = {
	"#version 120\n"

	"attribute vec4 pos0;"
	"attribute vec4 pos1;"
	"attribute vec2 texCoord;"
	"uniform float lerp;"
	"uniform vec2 texShift;"
	"uniform mat4 toClip;"
	"uniform sampler2D texture;"
	"varying vec2 texCoordOut;"

	"void main()"
	"{"
		"gl_Position = mix(pos0, pos1, lerp) * toClip;"
		"texCoordOut = texCoord + texShift;"
	"}"
};

/*
################################################################################################


	SUN LIGHT

Uses the geometry buffer and light information to add sun light. The color buffer is a light
buffer. The depth buffer is unchanged.

Lights have an unmixable sub-palette index instead of a color, so each pixel must choose to keep
one light's sub-palette. This is done by packing each light fragment's color-influence and
sub-palette into one component and blending so the most influential fragment is kept. Without
the packed component, lights would have to be drawn in sub-palette order and the framebuffer
would have to be copied to a texture for every sub-palette group so sub-palette selection could
be done in the shader.

Light buffer:
r	total light intensity
g	total additive shade (for glass, not affected by lights)
b	weak sub-palette
a	best-light influence and sub-palette, packed so influence is most significant
################################################################################################
*/

char* rnd::fragSunSource = {
	"#version 120\n"
	"#extension GL_EXT_gpu_shader4 : enable\n"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"
	"uniform float depthProj;"
	"uniform float depthBias, surfaceBiases[4];"
	"uniform mat4 focalToSunClips[4];" // One per cascade
	"uniform float kernels[4];"
	"uniform float litExps[4];"
	"uniform float pureSegment;"

	"uniform vec3 sunDir;" // world space
	"uniform float sunSubPalette;"
	"uniform float sunIntensity;"
	"uniform float sunColorSmooth;"
	"uniform float sunColorPriority;"

	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"
	"uniform sampler2DShadow shadow;"

	"vec3 cascadeOffsets[4] = vec3[]("
		"vec3(0.25, 0.25, 0.0),"
		"vec3(0.75, 0.25, 0.0),"
		"vec3(0.25, 0.75, 0.0),"
		"vec3(0.75, 0.75, 0.0)"
	");"

	// PCF offsets in [-0.5, 0.5] range since shadow texture has 2x2 cascade maps
	"vec2 offsets[16] = vec2[]("
		"vec2(-0.402066112, -0.478209794),"
		"vec2(0.388180166, -0.379787594),"
		"vec2(0.210989714, 0.492950231),"
		"vec2(-0.149616987, 0.496215701),"
		"vec2(0.45739615, 0.225913271),"
		"vec2(-0.331781983, 0.378688931),"
		"vec2(0.024704732, -0.484801769),"
		"vec2(0.446989357, -0.166219056),"
		"vec2(-0.32592243, -0.216544092),"
		"vec2(0.0456709489, 0.37380597),"
		"vec2(-0.361964792, 0.0117954034),"
		"vec2(-0.0184789579, -0.281243324),"
		"vec2(0.237784967, -0.14033936),"
		"vec2(0.169850767, 0.191213727),"
		"vec2(-0.101825006, 0.157429725),"
		"vec2(0.0252235476, -0.0566881299)"
	");"

	SHADER_FUNC_UNPACK_NORMAL
	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"float depthVal = texture2D(depth, tdc).r;"
		"float biasedLinDepth = depthProj / (depthVal + depthBias);"
		"vec3 focalFrag = ray * biasedLinDepth;"

		// Try each cascade until the sun-clip-space fragment is in range [-1, 1]
		"vec3 coords = (vec4(focalFrag + norm.xyz * surfaceBiases[0], 1.0) * focalToSunClips[0]).xyz;"
		"int cascade = 0;" // FIXME: fetch array values with a constant in each branch instead of later with a dynamic index?
		"float mac = max(abs(coords.x), abs(coords.y));"

		"if(mac > 0.995)"
		"{"
			"coords = (vec4(focalFrag + norm.xyz * surfaceBiases[1], 1.0) * focalToSunClips[1]).xyz;"
			"cascade = 1;"
			"mac = max(abs(coords.x), abs(coords.y));"

			"if(mac > 0.995)"
			"{"
				"coords = (vec4(focalFrag + norm.xyz * surfaceBiases[2], 1.0) * focalToSunClips[2]).xyz;"
				"cascade = 2;"
				"mac = max(abs(coords.x), abs(coords.y));"

				"if(mac > 0.995)"
				"{"
					"coords = (vec4(focalFrag + norm.xyz * surfaceBiases[3], 1.0) * focalToSunClips[3]).xyz;"
					"cascade = 3;"
					"mac = max(abs(coords.x), abs(coords.y));"
					//"mac = 0.0;" // Prevent trying to fade to next cascade
				"}"
			"}"
		"}"

		"coords = coords * vec3(0.25, 0.25, 1.0) + cascadeOffsets[cascade];"
		"float kernel = kernels[cascade];"
		"float lit = shadow2D(shadow, vec3(coords.xy + offsets[0] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[1] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[2] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[3] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[4] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[5] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[6] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[7] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[8] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[9] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[10] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[11] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[12] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[13] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[14] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[15] * kernel, coords.z)).r;"
		"lit = pow(lit * 0.0625, litExps[cascade]);" // Higher exponent makes shadows darker + blobbier
		
		"if(mac > pureSegment)"
		"{"
			// Frag is near cascade seam, fade to next cascade
			"if(cascade >= 3)"
			"{"
				"lit = mix(lit, 1.0, min((mac - pureSegment) / (0.995 - pureSegment), 1.0));"
			"}"
			"else"
			"{"
				"cascade += 1;"
				"coords = (vec4(focalFrag + norm.xyz * surfaceBiases[cascade], 1.0) * focalToSunClips[cascade]).xyz;"
				"coords = coords * vec3(0.25, 0.25, 1.0) + cascadeOffsets[cascade];"
				"kernel = kernels[cascade];"
				"float litn = shadow2D(shadow, vec3(coords.xy + offsets[0] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[1] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[2] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[3] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[4] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[5] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[6] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[7] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[8] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[9] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[10] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[11] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[12] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[13] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[14] * kernel, coords.z)).r;"
				"litn += shadow2D(shadow, vec3(coords.xy + offsets[15] * kernel, coords.z)).r;"

				// FIXME: seeing sharp seams when pure segment is very small
				"lit = mix(lit, pow(litn * 0.0625, litExps[cascade]), (mac - pureSegment) / (0.995 - pureSegment));"
			"}"
		"}"

		"float rand = Random(gl_FragCoord.xy, seed);"
		"float alpha = dot(vec3(norm), sunDir) * sunIntensity * lit;"
		"float go = step(0.0001, alpha);"

		"gl_FragColor = vec4("
			"alpha,"
			"0.0,"
			"0.0,"
			"PackLight("
				"alpha * mix(1.0, rand, sunColorSmooth) * sunColorPriority,"
				"sunSubPalette * go"
			")"
		");"
	"}"
};

/*
################################################################################################


	BULB LIGHT

Similar to SUN LIGHT but renders a point light.
################################################################################################
*/

char* rnd::vertBulbStencilSource = {
	"#version 120\n"

	"attribute vec4 pos;" // model space
	"uniform mat4 modelToClip;"

	"void main()"
	"{"
		"gl_Position = pos * modelToClip;"
	"}"
};

char* rnd::fragBulbStencilSource = {
	"#version 120\n"

	"void main()"
	"{"
		"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);"
	"}"
};

char* rnd::vertBulbLightSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space
	
	"uniform mat4 clipToFocal;"
	"uniform float frustumRatio;"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"void main()"
	"{"
		"gl_Position = pos;"
		"tdc = (pos.xy + 1.0) * 0.5;"

		// Get focal-space position of frustum near-plane corner and convert into per-view-x ray
		"vec4 focal = pos * clipToFocal;"
		"ray = focal.xyz / focal.w * frustumRatio;"
	"}"
};

#if DRAW_POINT_SHADOWS
// FIXME: optimize this heavily, should run decently on integrated GPU
char* rnd::fragBulbLightSource = {
	"#version 120\n"
	"#extension GL_EXT_gpu_shader4 : enable\n" // FIXME: just increase version instead?

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"
	"uniform float fade;"
	"uniform float depthProj0, depthProj1;"
	"uniform float depthBias, surfaceBias;"
	"uniform float kernel;"
	"uniform vec3 bulbPos;" // focal space
	"uniform float bulbInvRadius;" // world space
	"uniform float bulbExponent;"
	"uniform float bulbSubPalette;"
	"uniform float bulbIntensity;"
	"uniform float bulbColorSmooth;"
	"uniform float bulbColorPriority;"
	"uniform float bulbDepthProj0, bulbDepthProj1;"

	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"
	"uniform samplerCubeShadow shadow;"

	"vec3 offsets[16] = vec3[]("
		"vec3(1, 1, 1),"
		"vec3(-1, 1, 1),"
		"vec3(1, -1, 1),"
		"vec3(-1, -1, 1),"
		"vec3(1, 1, -1),"
		"vec3(-1, 1, -1),"
		"vec3(1, -1, -1),"
		"vec3(-1, -1, -1),"
		"vec3(-0.958189666, -0.00576799922, -0.771477401),"
		"vec3(-0.67345196, -0.986815989, -0.0173039958),"
		"vec3(0.99572742, -0.0177922919, 0.246131778),"
		"vec3(0.0814539045, 0.130893886, -0.956602693),"
		"vec3(-0.924130976, -0.0456251726, 0.259743035),"
		"vec3(0.206030458, -0.109836116, 0.922910213),"
		"vec3(-0.00839259103, 0.772209823, 0.102511674),"
		"vec3(0.195348978, -0.490646064, -0.18137151)"
	");"

	SHADER_FUNC_UNPACK_NORMAL
	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"float depthVal = texture2D(depth, tdc).r;"

		// Depth bias stops z-fighting when view depth precision is way lower than shadow map's
		// Surface bias stops z-fighting up close (not sure of cause) and bad self-shadowing
		"float biasedLinDepth = depthProj1 / (depthVal - depthBias - depthProj0);"
		"vec3 shadowDiff = ray * biasedLinDepth + norm.xyz * surfaceBias - bulbPos;"
		"float axialDist = max(abs(shadowDiff.x), max(abs(shadowDiff.y), abs(shadowDiff.z)));"
		"float refDepth = bulbDepthProj0 + bulbDepthProj1 / axialDist;"

#if 0
		"float lit = shadowCube(shadow, vec4(shadowDiff, refDepth)).r;"
#elif 1
		"float lit = shadowCube(shadow, vec4(shadowDiff + offsets[0] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[1] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[2] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[3] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[4] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[5] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[6] * kernel, refDepth)).r;"
		"lit += shadowCube(shadow, vec4(shadowDiff + offsets[7] * kernel, refDepth)).r;"

		"if(lit == 0.0)"
		"{"
			"discard;" // Exit early if first several samples are in shadow
		"}"
		"else if(lit < 8.0)"
		"{"
			// Fetch more samples if any previous are in shadow
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[8] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[9] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[10] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[11] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[12] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[13] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[14] * kernel, refDepth)).r;"
			"lit += shadowCube(shadow, vec4(shadowDiff + offsets[15] * kernel, refDepth)).r;"
			"lit *= 0.0625;"
		"}"
		"else"
		"{"
			"lit = 1.0;"
		"}"
#else
		"float lit = 0.0;"

		"for(int i = 0; i < 16; i++)"
		"{"
			// Project offset onto frag's plane and recalculate reference depth for each sample
			// Like receiver plane depth bias but slower and doesn't need depth derivative
			// Reduces need for other biases
			// Smooths certain sharp PCF edges (seen on shadows from occluders with flat depth)
			"vec3 tempDiff = shadowDiff + offsets[i] - dot(norm.xyz, offsets[i]) * norm.xyz;"
			"float tempAxial = max(abs(tempDiff.x), max(abs(tempDiff.y), abs(tempDiff.z)));"
			"float tempDepth = bulbDepthProj0 + bulbDepthProj1 / tempAxial;"
			"lit += shadowCube(shadow, vec4(tempDiff, tempDepth)).r;"
		"}"

		"lit *= 0.0625;"
#endif

		// Get attenuated light value with unbiased frag position
		"float linDepth = depthProj1 / (depthVal - depthProj0);"
		"vec3 diff = ray * linDepth - bulbPos;"
		"float diffLen = length(diff);"
		"float normDot = dot(vec3(norm), diff / -diffLen);"
		"float attenuation = pow(max(0.0, 1.0 - diffLen * bulbInvRadius), bulbExponent);"
		"float alpha = lit * bulbIntensity * attenuation * normDot;"
		"float rand = Random(gl_FragCoord.xy, seed);"
		"float go = step(rand * fade, alpha);" // Discard random low-intensity frags to smooth low precision bands

		"gl_FragColor = vec4("
			"alpha * go,"
			"0.0,"
			"0.0,"
			"PackLight("
				"alpha * mix(1.0, rand, bulbColorSmooth) * bulbColorPriority,"
				"bulbSubPalette * go"
			")"
		");"
	"}"
};
#else

// FIXME: pretty slow even without shadows
	// is blending the problem?
		// could do light batches per draw call, blend lights in frag shader before GPU blends to framebuffer
			// this would fetch from geometry buffer less, too
	// stencil pass?
		// try disabling again, but I think last test showed the stencil pass helps a lot

char* rnd::fragBulbLightSource = {
	"#version 120\n"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"
	"uniform float fade;"
	"uniform float depthProj;"
	"uniform vec3 bulbPos;" // focal space
	"uniform float bulbInvRadius;" // world space
	"uniform float bulbExponent;"
	"uniform float bulbSubPalette;"
	"uniform float bulbIntensity;"
	"uniform float bulbColorSmooth;"
	"uniform float bulbColorPriority;"

	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"

	SHADER_FUNC_UNPACK_NORMAL
	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"float depthVal = texture2D(depth, tdc).r;"

		// Get attenuated light value with unbiased frag position
		"float linDepth = depthProj / depthVal;"
		"vec3 diff = ray * linDepth - bulbPos;"
		"float diffLen = length(diff);"
		"float normDot = dot(vec3(norm), diff / -diffLen);"
		"float attenuation = pow(max(0.0, 1.0 - diffLen * bulbInvRadius), bulbExponent);"
		"float alpha = bulbIntensity * attenuation * normDot;"
		"float rand = Random(gl_FragCoord.xy, seed);"
		"float go = step(rand * fade, alpha);" // Discard random low-intensity frags to smooth low precision bands

		"gl_FragColor = vec4("
			"alpha * go,"
			"0.0,"
			"0.0,"
			"PackLight("
				"alpha * mix(1.0, rand, bulbColorSmooth) * bulbColorPriority,"
				"bulbSubPalette * go"
			")"
		");"
	"}"
};
#endif

/*
################################################################################################


	SPOT LIGHT


################################################################################################
*/

#if DRAW_POINT_SHADOWS
char* rnd::fragSpotLightSource = {
	"#version 120\n"
	"#extension GL_EXT_gpu_shader4 : enable\n"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"
	"uniform float fade;"
	"uniform float depthProj0, depthProj1;"
	"uniform float depthBias, surfaceBias;"
	"uniform float kernel;"
	"uniform float clipClamp;"
	"uniform vec3 coordsMultiply;"
	"uniform vec3 coordsOffset;"
	"uniform mat4 spotFocalToClip;"
	"uniform vec3 spotPos;" // focal space
	"uniform vec3 spotDir;" // world space
	"uniform float spotOuter;" // cosine of cutoff angle
	"uniform float spotInvPenumbra;" // 1.0 / (cos(inner) - cos(outer))
	"uniform float spotInvRadius;" // world space
	"uniform float spotExponent;"
	"uniform float spotSubPalette;"
	"uniform float spotIntensity;"
	"uniform float spotColorSmooth;"
	"uniform float spotColorPriority;"

	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"
	"uniform sampler2DShadow shadow;"

	"vec2 offsets[16] = vec2[]("
		"vec2(-1, 1),"
		"vec2(1, -1),"
		"vec2(-1, -1),"
		"vec2(1, 1),"
		"vec2(0.95977658, 0.314676344),"
		"vec2(-0.951780736, -0.288491458),"
		"vec2(0.0390331745, -0.96417129),"
		"vec2(0.0784020498, 0.952269077),"
		"vec2(-0.434064746, 0.809991777),"
		"vec2(0.670827329, -0.591906488),"
		"vec2(0.48265022, 0.593676567),"
		"vec2(0.611255229, -0.0732139051),"
		"vec2(-0.441633344, -0.411847293),"
		"vec2(-0.523972273, 0.292214721),"
		"vec2(0.15359965, -0.448774695),"
		"vec2(0.00247199833, 0.0514236875)"
	");"

	SHADER_FUNC_UNPACK_NORMAL
	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		// Get cone strength
		"float depthVal = texture2D(depth, tdc).r;"
		"float linDepth = depthProj1 / (depthVal - depthProj0);"
		"vec3 diff = ray * linDepth - spotPos;"
		"float diffLen = length(diff);"
		"diff /= diffLen;"
		"float cone = min((dot(diff, spotDir) - spotOuter) * spotInvPenumbra, 1.0);"

		"if(cone <= 0.0)"
			"discard;"

		"cone = pow(cone, spotExponent);"

		// Shadows
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"float biasedLinDepth = depthProj1 / (depthVal - depthBias - depthProj0);"
		"vec3 focalFrag = ray * biasedLinDepth;"
		"vec4 coords = vec4(focalFrag + norm.xyz * surfaceBias, 1.0) * spotFocalToClip;"
		"coords.xyz /= coords.w;"

		// Clamp coords so it doesn't stray into other shadow maps due to PCF or surface bias
		// Looks bad, so shadow map field of view should be increased to mostly avoid this case
		"coords.xy = clamp(coords.xy, -clipClamp, clipClamp);"
		"coords.xyz = coords.xyz * coordsMultiply + coordsOffset;"

		"float lit = shadow2D(shadow, vec3(coords.xy + offsets[0] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[1] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[2] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[3] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[4] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[5] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[6] * kernel, coords.z)).r;"
		"lit += shadow2D(shadow, vec3(coords.xy + offsets[7] * kernel, coords.z)).r;"

		"if(lit == 0.0)"
		"{"
			"discard;" // Exit early if first several samples are in shadow
		"}"
		"else if(lit < 8.0)"
		"{"
			// Fetch more samples if any previous are in shadow
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[8] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[9] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[10] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[11] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[12] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[13] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[14] * kernel, coords.z)).r;"
			"lit += shadow2D(shadow, vec3(coords.xy + offsets[15] * kernel, coords.z)).r;"
			"lit *= 0.0625;"
		"}"
		"else"
		"{"
			"lit = 1.0;"
		"}"

		// Attenuation & final light value
		"float normDot = dot(vec3(norm), -diff);"
		"float attenuation = pow(max(0.0, 1.0 - diffLen * spotInvRadius), spotExponent);"
		"float alpha = cone * lit * spotIntensity * attenuation * normDot;"
		"float rand = Random(gl_FragCoord.xy, seed);"
		"float go = step(rand * fade, alpha);"

		"gl_FragColor = vec4("
			"PackLight("
				"alpha * mix(1.0, rand, spotColorSmooth) * spotColorPriority,"
				"spotSubPalette * go"
			"),"
			"0.0,"
			"0.0,"
			"alpha * go"
		");"
	"}"
};
#else
char* rnd::fragSpotLightSource = {
	"#version 120\n"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"uniform float topInfluence;"
	"uniform float influenceFactor;"
	"uniform float seed;"
	"uniform float fade;"
	"uniform float depthProj;"
	"uniform vec3 spotPos;" // focal space
	"uniform vec3 spotDir;" // world space
	"uniform float spotOuter;" // cosine of cutoff angle
	"uniform float spotInvPenumbra;" // 1.0 / (cos(inner) - cos(outer))
	"uniform float spotInvRadius;" // world space
	"uniform float spotExponent;"
	"uniform float spotSubPalette;"
	"uniform float spotIntensity;"
	"uniform float spotColorSmooth;"
	"uniform float spotColorPriority;"

	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"

	SHADER_FUNC_UNPACK_NORMAL
	SHADER_FUNC_RANDOM
	SHADER_FUNC_PACK_LIGHT

	"void main()"
	"{"
		// Get cone strength
		"float depthVal = texture2D(depth, tdc).r;"
		"float linDepth = depthProj / depthVal;"
		"vec3 diff = ray * linDepth - spotPos;"
		"float diffLen = length(diff);"
		"diff /= diffLen;"
		"float cone = min((dot(diff, spotDir) - spotOuter) * spotInvPenumbra, 1.0);"

		"if(cone <= 0.0)"
			"discard;"

		"cone = pow(cone, spotExponent);"

		// Attenuation & final light value
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"float normDot = dot(vec3(norm), -diff);"
		"float attenuation = pow(max(0.0, 1.0 - diffLen * spotInvRadius), spotExponent);"
		"float alpha = cone * spotIntensity * attenuation * normDot;"
		"float rand = Random(gl_FragCoord.xy, seed);"
		"float go = step(rand * fade, alpha);"

		"gl_FragColor = vec4("
			"alpha * go,"
			"0.0,"
			"0.0,"
			"PackLight("
				"alpha * mix(1.0, rand, spotColorSmooth) * spotColorPriority,"
				"spotSubPalette * go"
			")"
		");"
	"}"
};
#endif

/*
################################################################################################


	COMPOSITION

Combines geometry and light buffers. The color buffer contains the scene's final RGB output.
################################################################################################
*/

#define SHADER_FUNC_UNPACK_LIGHT_SUB_PALETTE \
	/* Note that this fails on highest light-sub-palette if numColorBands is 1 */ \
	"float UnpackLightSubPalette(float c)" \
	"{" \
		/*
			c = influence * influenceFactor + subPalette * packLightSubFactor
			c / influenceFactor = influence + subPalette * packLightSubFactor / influenceFactor
			floor(c / influenceFactor) = influence
			fract(c / influenceFactor) = subPalette * packLightSubFactor / influenceFactor
		*/ \
		/* unpackFactor solves for subPalette and puts into palette's sub-palette range */ \
		/* Bias here helps with rounding error */ \
		"return fract((c + 0.0000001) * invInfluenceFactor) * unpackFactor;" \
	"}"

#define SHADER_FUNC_GET_RAMP_LUMINANCE \
	"float GetRampLuminance(vec2 tdc)" \
	"{" \
		"vec4 geometryFrag = texture2D(geometry, tdc);" \
		"vec4 lightFrag = texture2D(light, tdc);" \
		"float colorSubCoord = UnpackLightSubPalette(lightFrag.a);" \
		"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, colorSubCoord)).r;" \
		/*"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, mix(lightFrag.b + 0.00001, colorSubCoord, step(0.003, colorSubCoord)))).r;"*/ \
		"vec4 rampInfo = texture1D(rampLookup, fullBrightCoord);" \
		"float scale = pow(texture1D(lightCurve, lightFrag.r * exposure + midCurveSegment).r * maxIntensity, gamma);" \
		"return rampInfo.a * scale;" \
	"}"

char* rnd::vertCompSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"uniform mat4 clipToFocal;"
	"uniform float frustumRatio;"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	"void main()"
	"{"
		"tdc = (pos.xy + 1.0) * 0.5;"
		"gl_Position = pos;"

		// Same as in bulb vert shader
		"vec4 focal = pos * clipToFocal;"
		"ray = focal.xyz / focal.w * frustumRatio;"
	"}"
};

char* rnd::fragCompSource = {
	"#version 120\n"

	"#define OUTPUT_NORMALS 0\n"
	"#define OUTPUT_PACKED_NORMALS 0\n"
	"#define OUTPUT_FULLBRIGHT 0\n"
	"#define OUTPUT_LIGHT 0\n"
	"#define OUTPUT_CUBE 0\n"
	"#define OUTPUT_DEPTH 0\n"
	"#define OUTPUT_FOG_TEST 0\n"

	"#define PI 3.1415926535897932384626433832795\n"

	"uniform float depthProj;"
	"uniform float invInfluenceFactor;"
	"uniform float unpackFactor;"
	"uniform float maxIntensity;"
	"uniform float rampDistScale;" // Converts 8-bit ramp distance to texture coordinate delta
	"uniform float exposure;"
	"uniform float gamma;"
	"uniform float midCurveSegment;"
	"uniform float ditherRandom;"
	"uniform float ditherGrain;"
	"uniform float ditherFactor;"
	"uniform float grainScale;"
	"uniform float pattern[16];"
	"uniform float seed;"
	"uniform float rampAdd;"
	"uniform float additiveDenormalization;"
	"uniform vec3 sunPos, fogCenterFocal;" // sun must be normalized
	"uniform float fogRadius, fogThinRadius, fogThinExponent;"
	"uniform float fogGeoSubStartDist, fogGeoSubHalfDist, fogGeoSubFactor, fogGeoSubCoord;"
	"uniform float fogFadeStartDist, fogFadeHalfDist, fogFadeAmount, fogFadeRampPos, fogFadeRampPosSun;"
	"uniform float fogAddStartDist, fogAddHalfDist, fogAddAmount, fogAddAmountSun;"
	"uniform float fogSunRadius, fogSunExponent;"

	"\n#if (OUTPUT_CUBE == 1)\n"
	"uniform samplerCube geometry;"
	"\n#else\n"
	"uniform sampler2D geometry;"
	"uniform sampler2D depth;"
	"uniform sampler2D light;"
	"uniform sampler2D subPalettes;"
	"uniform sampler1D ramps;"
	"uniform sampler1D rampLookup;" // FIXME: Combine ramps and rampLookup into one texture? Their dimensions are likely to be similar
	"uniform sampler1D lightCurve;"
	"\n#endif\n"

	"varying vec2 tdc;"
	"varying vec3 ray;"

	SHADER_FUNC_UNPACK_LIGHT_SUB_PALETTE
	SHADER_FUNC_RANDOM
	SHADER_FUNC_RANDOM2
	SHADER_FUNC_SIMPLEX_NOISE

	// For debugging cube maps
	"\n#if (OUTPUT_CUBE == 1)\n"
	"vec3 LensDirection(vec2 fragCoord, float hFOV, float aspect, int dir)"
	"{"
		"float vFOV = 2.0 * atan(tan(hFOV * 0.5) / aspect);"
		"float hori = tan(hFOV * 0.5);"
		"float vert = tan(vFOV * 0.5);"

		"if(dir == 0){"
			"return vec3("
				"1.0,"
				"-hori * (tdc.x * 2.0 - 1.0),"
				"vert * (tdc.y * 2.0 - 1.0)"
			");"
		"}else if(dir == 1){"
			"return vec3("
				"-1.0,"
				"hori * (tdc.x * 2.0 - 1.0),"
				"vert * (tdc.y * 2.0 - 1.0)"
			");"
		"}else if(dir == 2){"
			"return vec3("
				"hori * (tdc.x * 2.0 - 1.0),"
				"1.0,"
				"vert * (tdc.y * 2.0 - 1.0)"
			");"
		"}else if(dir == 3){"
			"return vec3("
				"-hori * (tdc.x * 2.0 - 1.0),"
				"-1.0,"
				"vert * (tdc.y * 2.0 - 1.0)"
			");"
		"}else if(dir == 4){"
			"return vec3("
				"-vert * (tdc.y * 2.0 - 1.0),"
				"-hori * (tdc.x * 2.0 - 1.0),"
				"1.0"
			");"
		"}else{"
			"return vec3("
				"vert * (tdc.y * 2.0 - 1.0),"
				"-hori * (tdc.x * 2.0 - 1.0),"
				"-1.0"
			");"
		"}"
	"}"
	"\n#endif\n"

	"\n#if (OUTPUT_NORMALS == 1)\n"
	SHADER_FUNC_UNPACK_NORMAL
	"\n#endif\n"

	SHADER_FUNC_DITHER_INDEX

	"float fogDither[64] = float[]("
		" 0.0/64.0, 32.0/64.0,  8.0/64.0, 40.0/64.0,	 2.0/64.0, 34.0/64.0, 10.0/64.0, 42.0/64.0,"
		"48.0/64.0, 16.0/64.0, 56.0/64.0, 24.0/64.0,	50.0/64.0, 18.0/64.0, 58.0/64.0, 26.0/64.0,"
		"12.0/64.0, 44.0/64.0,  4.0/64.0, 36.0/64.0,	14.0/64.0, 46.0/64.0,  6.0/64.0, 38.0/64.0,"
		"60.0/64.0, 28.0/64.0, 52.0/64.0, 20.0/64.0,	62.0/64.0, 30.0/64.0, 54.0/64.0, 22.0/64.0,"

		" 3.0/64.0, 35.0/64.0, 11.0/64.0, 43.0/64.0,	 1.0/64.0, 33.0/64.0,  9.0/64.0, 41.0/64.0,"
		"51.0/64.0, 19.0/64.0, 59.0/64.0, 27.0/64.0,	49.0/64.0, 17.0/64.0, 57.0/64.0, 25.0/64.0,"
		"15.0/64.0, 47.0/64.0,  7.0/64.0, 39.0/64.0,	13.0/64.0, 45.0/64.0,  5.0/64.0, 37.0/64.0,"
		"63.0/64.0, 31.0/64.0, 55.0/64.0, 23.0/64.0,	61.0/64.0, 29.0/64.0, 53.0/64.0, 21.0/64.0 "
	");"

	"void main()"
	"{"
		"\n#if (OUTPUT_CUBE == 0)\n"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"float fullBrightCoord = geometryFrag.r;"
		"float depthVal = texture2D(depth, tdc).r;"
		//"float focalDist = (1.0 - step(depthVal, 0.0)) * length(ray * (depthProj / depthVal));"
		"float focalDist = length(ray * (depthProj / depthVal));"

		"vec3 rayNorm = normalize(ray);"
		"float fogTotalRadius = fogRadius + fogThinRadius;" // FIXME: extra uniform? should fogThinRadius be subtractive?
		"float fogCenterRayDot = dot(rayNorm, fogCenterFocal);"
		"float fogProjDist = length(rayNorm * fogCenterRayDot - fogCenterFocal);"
		"float fogAccum = 0.0;"

		"if(fogProjDist < fogTotalRadius)"
		"{"
			"float halfSphereIntersect = sqrt(fogTotalRadius * fogTotalRadius - fogProjDist * fogProjDist);"
			"float fogStart = max(0.0, fogCenterRayDot - halfSphereIntersect);"
			"float fogEnd = min(focalDist, fogCenterRayDot + halfSphereIntersect);"
			"float fogDist = max(0.0, fogEnd - fogStart);"

#if 1
			"float stepDist = fogDist * 0.2;"
#if 1 // FIXME: which one's faster?
			"vec3 rayStep = rayNorm * stepDist;"
			"vec3 march = fogCenterFocal - rayNorm * (fogStart + fogDist * 0.1);"
			"fogAccum += stepDist * pow(min(1.0 - ((length(march) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"march -= rayStep;"
			"fogAccum += stepDist * pow(min(1.0 - ((length(march) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"march -= rayStep;"
			"fogAccum += stepDist * pow(min(1.0 - ((length(march) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"march -= rayStep;"
			"fogAccum += stepDist * pow(min(1.0 - ((length(march) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"march -= rayStep;"
			"fogAccum += stepDist * pow(min(1.0 - ((length(march) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
#else
			"fogAccum += stepDist * pow(min(1.0 - ((length(fogCenterFocal - rayNorm * (fogStart + fogDist * 0.1)) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"fogAccum += stepDist * pow(min(1.0 - ((length(fogCenterFocal - rayNorm * (fogStart + fogDist * 0.3)) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"fogAccum += stepDist * pow(min(1.0 - ((length(fogCenterFocal - rayNorm * (fogStart + fogDist * 0.5)) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"fogAccum += stepDist * pow(min(1.0 - ((length(fogCenterFocal - rayNorm * (fogStart + fogDist * 0.7)) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
			"fogAccum += stepDist * pow(min(1.0 - ((length(fogCenterFocal - rayNorm * (fogStart + fogDist * 0.9)) - fogRadius) / fogThinRadius), 1.0), fogThinExponent);"
#endif
#else
			"fogAccum = fogDist;" // Whole atmosphere has a density of 1
#endif
		"}"

#if 1 // This seems to optimize more than the #else branch
		// FIXME: Only tested on an RTX 3060 TI; check on GTX 260 (where it really matters) if the branching is helpful at all
		//"if(fogGeoSubCoord > 0.0 && fogDist > 0.0)"
		"if(fogGeoSubCoord > 0.0)"
		"{"
			"float fogGeoSubDist = max(0.0, fogAccum - fogGeoSubStartDist);"
			"float fogGeoSubEffect = min(fogGeoSubDist / (fogGeoSubDist + fogGeoSubHalfDist) * fogGeoSubFactor, 1.0);"

			"fullBrightCoord = mix("
				"texture2D(subPalettes, vec2(geometryFrag.r, fogGeoSubCoord)).r,"
				"geometryFrag.r,"
				"step(fogGeoSubEffect - fogDither[int(mod(gl_FragCoord.x, 8)) + int(mod(gl_FragCoord.y, 8)) * 8], 0.0)"
			");"
		"}"
#else
		"float fogGeoSubDist = max(0.0, fogAccum - fogGeoSubStartDist);"
		"float fogGeoSubEffect = min(fogGeoSubDist / (fogGeoSubDist + fogGeoSubHalfDist) * fogGeoSubFactor, 1.0);"

		"if(fogGeoSubCoord > 0.0 && focalDist > 0.0 && fogGeoSubEffect - fogDither[int(mod(gl_FragCoord.x, 8)) + int(mod(gl_FragCoord.y, 8)) * 8] > 0.0)"
			"fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, fogGeoSubCoord)).r;"
#endif

		"vec4 lightFrag = texture2D(light, tdc);"
		"float colorSubCoord = UnpackLightSubPalette(lightFrag.a);"
		//"fullBrightCoord = texture2D(subPalettes, vec2(fullBrightCoord, colorSubCoord)).r;"

		"fullBrightCoord = texture2D(subPalettes, vec2(fullBrightCoord, mix(lightFrag.b + 0.00001, colorSubCoord, step(0.003, colorSubCoord)))).r;"
			// Use weak-glass sub-palette if light-sub-palette is 0
			// FIXME: should the bias on lightFrag.b be a uniform based on the number of sub palettes?

		// FIXME: do fogLightSub-palette right here?

		"vec4 rampInfo = texture1D(rampLookup, fullBrightCoord);"

		// FIXME: make this actually 16 bits, bits are being wasted since r = 1, g = 0 is equal to 1.0
		/* FIXME: Use texelFetch if 130 is available to avoid multiply by rampDistScale?
			Red and green values will have to be fractions of 255, not the ramp texture's length */
		"float rampStart = rampInfo.r + rampInfo.g * 0.0039215686274509803921568627451;" // ~16-bit value, r + g / 255.0
		"float rampLength = rampInfo.b * rampDistScale;" // FIXME: can avoid an extra operation by doing this multiply each time it's needed (muladd) since rampDistScale is already involved in a multiplication in rampColorCoord's calculation
		"float rampEnd = rampStart + rampLength;"

		// Curve intensity to mimic film grains reacting to light non-linearly or other effects
		// Then apply gamma correction so light fall-off looks right on user's monitor
		/* Each ramp must be approximately linear in perceived brightness for this to work (i.e.
		grayscale ramp's sRGB values should go up linearly) */

		// FIXME: rescale input intensity so curve texture (and exposure) can go beyond maxIntensity?
		// FIXME: change lightCurve to a multiplier so if it's not 16-bit it doesn't filter the light buffer's precision?
		"float scale = pow(texture1D(lightCurve, lightFrag.r * exposure + midCurveSegment).r * maxIntensity, gamma);"

#if 0
		/* FIXME: can do this instead of modifying ramps to make bright ramps work but also let
		them be lit up by lights, not just additive glass; brightCoords is numBrightColors / 255.0 */
		"if(fullBrightCoord < brightCoords)"
			"scale = max(1.0, scale);"
#endif

		/* The curve operations above are not entirely correct because they only affect light
		intensity, they don't account for how much the diffuse color reflects received light.
		Therefore, a 100% diffuse at 50% light may look different from a 50% diffuse at 100%
		light. Additive light is also not affected.
		
		Some flawed solutions:
		. Have curves affect the final RGB values
			. Breaks the what-you-see-is-what-you-get aspect of the palette (important to stay
			consistent here)
			. Textures have to be gamma corrected
			. Real-time lighting and gamma-corrected textures look rough and grungy in the low
			end because there aren't enough shades to the user's eye
			. The curve no longer emulates film's characteristic curve and instead acts like
			pushing/pulling during film development: the pixels/grains are set, now it's just
			post-processing (can already achieve this effect by changing the palette)
		. Embed the curves into the palette
			. Palette is still affected but is now visible without launching the game
			. Introduces all the other negatives
		. Apply curves to a normalized rampColorCoord (0-1 from start to end of ramp) and then
		undo the normalization before getting the final RGB color
			. Preserves palette
			. Introduces all the other negatives
		. Change meaning of intensity and have light pass include diffuse color in calculations
			. Complicated and expensive */

		// FIXME: Compile different shaders for different dither types; use preprocessing to create the source strings

		/* FIXME: Update cloud/alpha composition with new dithering code
			Random-dithering option
			rnd_ramp_add option
		*/

		/* FIXME: Distant recolored glass recolors fog because light sub-palette is not affected
		by fog (so distant lights look cool). But, if this needs to be fixed, then a
		light-affecting sub-palette fog needs to be added in that uses the depth buffer WITH the
		recolored glass drawn in. Every other fog effect would still use the depth buffer
		without glass, so this would be expensive. Alternatively, dither-fade away recolored
		glass in accordance with fog, or just according to distance from the camera. */

		/* FIXME: Putting additive fog and fading fog together with separate curves looks really
		good but can create an unrealistic effect where the sun-lit horizon is also lit by the
		additive fog and becomes brighter than the sky. Either recreate a similar effect but
		only in the mix() function (tried with fogMergeFinal but it's not right), or multiply
		fogFadeEffect so it reaches 1.0 at some point and fully fogs the edge of the horizon */

		"float fogFadeDist = max(0.0, fogAccum - fogFadeStartDist);"
		"float fogFadeEffect = min(fogFadeDist / (fogFadeDist + fogFadeHalfDist), 1.0);"
		"float fogAddDist = max(0.0, fogAccum - fogAddStartDist);"
		"float fogAddEffect = min(fogAddDist / (fogAddDist + fogAddHalfDist), 1.0);"

		//"float fogMergeFinal = (fogFadeEffect * fogFadeAmount + fogAddEffect * fogAddAmount) / (fogFadeAmount + fogAddAmount);" // FIXME TEMP

		"float fogSun = pow(max(0.0, 1.0 - acos(clamp(dot(rayNorm, sunPos), -1.0, 1.0)) / fogSunRadius), fogSunExponent);"
		"float exposedRampLength = rampLength * exposure;"

		"float rampColorCoord = clamp("
			"rampStart + "
			"mix("
				"rampInfo.a * rampDistScale * scale,"
				"(fogFadeRampPos + fogFadeRampPosSun * fogSun) * exposedRampLength,"
				"fogFadeEffect * fogFadeAmount"
				/*"(fogFadeRampPos + fogAddAmount + (fogFadeRampPosSun + fogAddAmountSun) * fogSun) * rampLength,"
				"fogMergeFinal"*/
			") + "
			"lightFrag.g * additiveDenormalization * exposedRampLength + "
			"fogAddEffect * (fogAddAmount + fogAddAmountSun * fogSun) * exposedRampLength + "
			"rampAdd + "
			"mix("
				"mix("
					"pattern[DitherIndex()],"
					"Random(gl_FragCoord.xy, seed) - 0.5,"
					"ditherRandom"
				"),"
				"SimplexNoise(gl_FragCoord.xy * grainScale, seed),"
				"ditherGrain"
			") * ditherFactor,"
			//"rampStart + lightFrag.b * rampLength,"
			"rampStart,"
			"rampEnd"
		");"

		"vec4 colorFinal = texture1D(ramps, rampColorCoord);"
		"\n#endif\n"

		"\n#if (OUTPUT_NORMALS == 1)\n"
		"vec4 norm = UnpackNormal(geometryFrag);"
		"gl_FragColor = vec4(norm.x * 0.5 + 0.5, norm.y * 0.5 + 0.5, norm.z * 0.5 + 0.5, 1.0);"
		"\n#elif (OUTPUT_PACKED_NORMALS == 1)\n"
		"gl_FragColor = vec4(geometryFrag.g, geometryFrag.b, geometryFrag.a, 1.0);"
		"\n#elif (OUTPUT_FULLBRIGHT == 1)\n"
		"gl_FragColor = texture1D(ramps, rampStart + rampInfo.a * rampDistScale);"
		"\n#elif (OUTPUT_LIGHT == 1)\n"
		"gl_FragColor = vec4(lightFrag.r, lightFrag.r, lightFrag.r, 1.0);"
		"\n#elif (OUTPUT_CUBE == 1)\n"
		"float minVal = 0.993;"
		"float maxVal = 1.0;"
		"gl_FragColor = (textureCube(geometry, LensDirection(gl_FragCoord.xy, PI * 0.7, 1280.0 / 720.0, 1)) - minVal) / (maxVal - minVal);"
		"\n#elif (OUTPUT_DEPTH == 1)\n"
		"float minVal = 0.0;"
		"float maxVal = 0.001;"
		"gl_FragColor = vec4((depthVal - minVal) / (maxVal - minVal));"
		"\n#elif (OUTPUT_FOG_TEST == 1)\n"
		"gl_FragColor = vec4(fogAccum / (fogTotalRadius * 2.0));"
		"\n#else\n"
		"gl_FragColor = colorFinal;"
		"\n#endif\n"

		"gl_FragColor.a = dot(gl_FragColor.rgb, vec3(0.299, 0.587, 0.114));" // FIXME TEMP: embed luma straight into ramp output
	"}"
};

// FIXME: cloud composition shaders are outdated, missing features from regular composition shader; do general alpha mixing shader
char* rnd::vertCloudCompSource = {
	"#version 120\n"

	"attribute vec4 pos0, pos1;" // model space
	"attribute vec4 norm0, norm1;" // model space // FIXME: remove if not needed

	"uniform float lerp;"
	"uniform mat4 modelToClip;"
	"uniform mat4 modelToNormal;"
	"uniform vec3 modelCam;"
	"uniform mat4 modelToVoxel;"
	"uniform vec3 voxelInvDims;"
	"uniform float voxelScale;"
	"uniform sampler3D voxels;"

	"varying float densityOut;"

	SHADER_FUNC_CLOUD_DENSITY

	"void main()"
	"{"
		"vec4 pos = mix(pos0, pos1, lerp);"
		"gl_Position = pos * modelToClip;"
		"densityOut = CloudDensity(pos);" // FIXME: multiply by opacity
	"}"
};

// FIXME: cloud composition quality user option
char* rnd::fragCloudCompSourceMedium = {
	"#version 120\n"

	// FIXME: opacity
	"uniform vec2 vidInv;"
	"uniform float invInfluenceFactor;"
	"uniform float unpackFactor;"
	"uniform float maxIntensity;"
	"uniform float rampDistScale;"
	"uniform float exposure;"
	"uniform float gamma;"
	"uniform float midCurveSegment;"
	"uniform float ditherRandom;"
	"uniform float ditherFactor;"
	"uniform float grainScale;"
	"uniform float pattern[16];"
	"uniform float seed;"

	"uniform sampler2D geometry;"
	"uniform sampler2D light;"
	"uniform sampler2D subPalettes;"
	"uniform sampler1D ramps;"
	"uniform sampler1D rampLookup;"
	"uniform sampler1D lightCurve;"

	"varying float densityOut;"

	SHADER_FUNC_UNPACK_LIGHT_SUB_PALETTE
	SHADER_FUNC_RANDOM2
	SHADER_FUNC_SIMPLEX_NOISE
	SHADER_FUNC_DITHER_INDEX
	SHADER_FUNC_HALF_DITHER_INDEX
	SHADER_FUNC_GET_RAMP_LUMINANCE

	"void main()"
	"{"
		// Get alpha
		"float alpha = densityOut * 1.301 - pattern[HalfDitherIndex()] * 0.32;"

		"if(alpha >= 1.0 || alpha <= 0.01)"
			"discard;" // Let regular composition take care of this frag

		// Calculate primary frag
		"vec2 tdc = gl_FragCoord.xy * vidInv;"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 lightFrag = texture2D(light, tdc);"
		"float colorSubCoord = UnpackLightSubPalette(lightFrag.a);"
		//"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, colorSubCoord)).r;"
		"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, mix(lightFrag.b + 0.00001, colorSubCoord, step(0.003, colorSubCoord)))).r;"

		"vec4 rampInfo = texture1D(rampLookup, fullBrightCoord);"

		"float rampStart = rampInfo.r + rampInfo.g * 0.0039215686274509803921568627451;"
		"float rampLength = rampInfo.b * rampDistScale;"
		"float rampEnd = rampStart + rampLength;"

		"float scale = pow(texture1D(lightCurve, lightFrag.r * exposure + midCurveSegment).r * maxIntensity, gamma);"
		"float mainLum = rampInfo.a * scale;"

		// Partially blend with other frags
		"vec2 other = tdc + vidInv * (1.0 - mod(floor(gl_FragCoord.xy), 2) * 2.0);"

		"float finalLum = mainLum * 0.5 + ("
			"GetRampLuminance(vec2(other.x, tdc.y)) +"
			"GetRampLuminance(vec2(tdc.x, other.y)) +"
			"GetRampLuminance(vec2(other.x, other.y))"
		") * 0.16666666666666666666666666666667;"

		// FIXME: try uniform branch to select between no dithering, pattern dithering, and grain dithering, and see if it's faster
		// FIXME: additive stuff (lightFrag.g, lightFrag.b) in the background will be dithered, need to blend that too
		"float rampColorCoord = clamp("
			"rampStart + finalLum * rampDistScale + lightFrag.g * rampLength - mix(pattern[DitherIndex()], SimplexNoise(gl_FragCoord.xy * grainScale, seed), ditherRandom) * ditherFactor,"
			"mix(rampStart, rampEnd, lightFrag.b),"
			"rampEnd"
		");"

		"gl_FragColor = texture1D(ramps, rampColorCoord);"
		"gl_FragColor.a = dot(gl_FragColor.rgb, vec3(0.299, 0.587, 0.114));" // FIXME TEMP: embed luma straight into ramp output
	"}"
};

char* rnd::fragCloudCompSourceHigh = {
	"#version 120\n"

	// FIXME: opacity
	"uniform vec2 vidInv;"
	"uniform float invInfluenceFactor;"
	"uniform float unpackFactor;"
	"uniform float maxIntensity;"
	"uniform float rampDistScale;"
	"uniform float exposure;"
	"uniform float gamma;"
	"uniform float midCurveSegment;"
	"uniform float ditherRandom;"
	"uniform float ditherFactor;"
	"uniform float grainScale;"
	"uniform float pattern[16];"
	"uniform float seed;"

	"uniform sampler2D geometry;"
	"uniform sampler2D light;"
	"uniform sampler2D subPalettes;"
	"uniform sampler1D ramps;"
	"uniform sampler1D rampLookup;"
	"uniform sampler1D lightCurve;"

	"varying float densityOut;"

	SHADER_FUNC_UNPACK_LIGHT_SUB_PALETTE
	SHADER_FUNC_RANDOM2
	SHADER_FUNC_SIMPLEX_NOISE
	SHADER_FUNC_DITHER_INDEX
	SHADER_FUNC_HALF_DITHER_INDEX
	SHADER_FUNC_GET_RAMP_LUMINANCE
	SHADER_FUNC_STIPPLE

	"void main()"
	"{"
		// FIXME: Prevent blending with frags in front of the cloud; fetch depth and calculate expected using delta
			// FIXME: To get the depth buffer texture, blit default buffer's depth back into geometry-depth buffer

		// FIXME: Make background clouds smooth too by writing alpha into geometry buffer
			/* Either in another cloud prepass after lighting is done (overwrite a normal
			component) or with multi rendering into another buffer entirely (have to fetch twice
			here, though) */
			/* Cloud composition could then be a branch in the composition shader, but it would
			have to do 4 fetches for every screen frag to figure out whether to branch. A big
			benefit is the ability to do true transparency uniformly on any object, not just
			clouds. The clouds could also be drawn again but without calculating alpha or
			writing depth, just setting a stencil bit to avoid recomposition; that would get rid
			of the foreground-background separation, so the final blend might not convey depth
			order (higher alpha clouds in the back may look like they're in front). Another
			option is to draw the composition clouds as they are now, just with the background
			alpha values being used as weights in the background blend. */

		// FIXME: At [1.0, 1.3) alpha, some frags still get their ramp from the background, which means there's a slight transparency effect in the color

		// FIXME: Sparkling at 1.0 alpha received from vertex shader caused by variance in triangles and imprecise interpolation (making the 1.0 jitter)
		
		"float realAlpha = densityOut;" // Wanted visual alpha value
		"float alpha = max(0.01, realAlpha - pattern[HalfDitherIndex()] * 0.32);" // Stipple alpha value

		"if(alpha >= 1.0 || realAlpha < 0.01)"
			"discard;" // Let regular composition take care of this frag

		// Calculate primary frag
		"vec2 tdc = gl_FragCoord.xy * vidInv;"
		"vec4 geometryFrag = texture2D(geometry, tdc);"
		"vec4 lightFrag = texture2D(light, tdc);"
		"float colorSubCoord = UnpackLightSubPalette(lightFrag.a);"
		//"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, colorSubCoord)).r;"
		"float fullBrightCoord = texture2D(subPalettes, vec2(geometryFrag.r, mix(lightFrag.b + 0.00001, colorSubCoord, step(0.003, colorSubCoord)))).r;"

		"vec4 rampInfo = texture1D(rampLookup, fullBrightCoord);"

		"float rampStart = rampInfo.r + rampInfo.g * 0.0039215686274509803921568627451;"
		"float rampLength = rampInfo.b * rampDistScale;"
		"float rampEnd = rampStart + rampLength;"

		"float scale = pow(texture1D(lightCurve, lightFrag.r * exposure + midCurveSegment).r * maxIntensity, gamma);"
		"float mainLum = rampInfo.a * scale;"

		// Calculate other frags
		"vec2 rem = mod(floor(gl_FragCoord.xy), 2);"
		"vec2 otherDir = 1.0 - rem * 2.0;"
		"vec2 otherX = vec2(tdc.x + vidInv.x * otherDir.x, tdc.y);"
		"vec2 otherY = vec2(tdc.x, tdc.y + vidInv.y * otherDir.y);"
		"vec2 otherDiag = vec2(otherX.x, otherY.y);"

		// otherBlend is the blended background if mainLum is part of the foreground, and vice versa
		"float mainGround = 1.0 - step(alpha, Stipple(rem)) * 2.0;" // 1 = foreground, -1 = background
		"vec2 otherBlend = vec2(0.0, 0.0);" // Second component is a weight accumulation
		"vec2 mainBlend = vec2(mainLum, 1.0);"

		// Only add luminance to otherLum if it comes from the opposite ground of mainLum; add to mainBlend otherwise
		// If main frag is in background, mainGround flips sign of alpha and stipple value so less-or-equal check becomes greater-than
		"vec2 alphaDelta = vec2(otherDir.x * dFdx(alpha), otherDir.y * dFdy(alpha));" // Used to get alpha of nearby frags to prevent seams

		"vec2 tempLum = vec2(GetRampLuminance(otherX), 1.0);"
		"float oppositeGround = step((alpha + alphaDelta.x) * mainGround, Stipple(vec2(1.0 - rem.x, rem.y)) * mainGround);"
		"otherBlend += tempLum * oppositeGround;"
		"mainBlend += tempLum * (1.0 - oppositeGround) * 0.33333333333333333333333333333333;"

		"tempLum.x = GetRampLuminance(otherY);"
		"oppositeGround = step((alpha + alphaDelta.y) * mainGround, Stipple(vec2(rem.x, 1.0 - rem.y)) * mainGround);"
		"otherBlend += tempLum * oppositeGround;"
		"mainBlend += tempLum * (1.0 - oppositeGround) * 0.33333333333333333333333333333333;"

		"tempLum.x = GetRampLuminance(otherDiag);"
		"oppositeGround = step((alpha + alphaDelta.x + alphaDelta.y) * mainGround, Stipple(vec2(1.0 - rem.x, 1.0 - rem.y)) * mainGround);"
		"otherBlend += tempLum * oppositeGround;"
		"mainBlend += tempLum * (1.0 - oppositeGround) * 0.33333333333333333333333333333333;"

		/* Clamp here prevents division by zero; mainAlpha should be high in that case, so
		otherLum's effect will be negligible */
		"otherBlend.x /= max(1.0, otherBlend.y);"

		/* If this frag is part of the background, use the blended background. This reduces
		background resolution but makes background cloud dither-sparkles a little less
		glaring */
		"mainLum = mix(mainBlend.x / mainBlend.y, mainLum, max(0.0, mainGround));"

		"float mainAlpha = fract(clamp(realAlpha, 0.0, 0.999) * mainGround);" // mainGround flips alpha if this is a background frag
		"float finalLum = mix(otherBlend.x, mainLum, mainAlpha);"

		// FIXME: try uniform branch to select between no dithering, pattern dithering, and grain dithering, and see if it's faster
		// FIXME: additive stuff (lightFrag.g) in the background will be dithered, need to blend that too
		"float rampColorCoord = clamp("
			"rampStart + finalLum * rampDistScale + lightFrag.g * rampLength - mix(pattern[DitherIndex()], SimplexNoise(gl_FragCoord.xy * grainScale, seed), ditherRandom) * ditherFactor,"
			"mix(rampStart, rampEnd, lightFrag.b),"
			"rampEnd"
		");"

		"gl_FragColor = texture1D(ramps, rampColorCoord);"
		"gl_FragColor.a = dot(gl_FragColor.rgb, vec3(0.299, 0.587, 0.114));" // FIXME TEMP: embed luma straight into ramp output
	"}"
};

/*
################################################################################################


	IMAGE

Draws clip space elements. This pass draws the UI onto integer scaled quads. The color buffer
contains the final RGB output of the UI and anything that was drawn before. The depth buffer
contains the geometry's depth.
################################################################################################
*/

char* rnd::vertImageSource = {
	"#version 120\n"

	"attribute vec3 pos;"
	"attribute vec2 texCoord;"
	"attribute float swap;"

	"varying vec2 texCoordOut;"
	"varying float swapOut;"

	"void main()"
	"{"
		"texCoordOut = texCoord;"
		"swapOut = swap + 0.001;" // FIXME: check bias
		"gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);"
	"}"
};

char* rnd::fragImageSource = {
	"#version 120\n"

	"varying vec2 texCoordOut;"
	"varying float swapOut;"

	"uniform sampler2D image;"
	"uniform sampler1D palette;"
	"uniform sampler2D subPalettes;"

	"void main()"
	"{"
		"vec3 imageIndex = vec3(texture2D(image, texCoordOut));"
		"if(imageIndex.r == 0) discard;"
		"gl_FragColor = texture1D(palette, texture2D(subPalettes, vec2(imageIndex.r, swapOut)).r);" // draw 256 color image
		//"gl_FragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 0.0);"
		//"gl_FragColor = vec4(imageIndex.r, imageIndex.g, imageIndex.b, 0.0);" // draw RGB image
	"}"
};

/*
################################################################################################


	LINE

Debug lines
################################################################################################
*/

char* rnd::vertLineSource = {
	"#version 120\n"

	"attribute vec4 pos;" // world space
	"attribute float color;"

	"uniform sampler1D palette;"
	"uniform mat4 worldToClip;"

	"void main()"
	"{"
		"gl_Position = pos * worldToClip;"
		"gl_FrontColor = texture1D(palette, color);"
	"}"
};

char* rnd::vertLine2DSource = {
	"#version 120\n"

	"attribute vec4 pos;" // user space
	"attribute float color;"

	"uniform sampler1D palette;"
	"uniform vec2 invVid;"

	"void main()"
	"{"
		"vec2 clip = pos.xy * invVid * 2.0 - 1.0;"
		"gl_Position = vec4(clip.x, clip.y, pos.z, pos.w);"
		"gl_FrontColor = texture1D(palette, color);"
	"}"
};

char* rnd::fragLineSource = {
	"#version 120\n"

	"void main()"
	"{"
		"gl_FragColor = gl_Color;"
	"}"
};

/*
################################################################################################


	RESTORE


################################################################################################
*/

char* rnd::vertRestoreSource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"void main()"
	"{"
		"gl_Position = pos;"
	"}"
};

char* rnd::fragRestoreSource = {
	"#version 120\n"

	"uniform vec2 videoDimensions;" // FIXME: just convert pos to [0, 1] range
	"uniform sampler2D tex;"

	"void main()"
	"{"
		"gl_FragDepth = texture2D(tex, vec2(gl_FragCoord) / videoDimensions).r;"
	"}"
};

/*
################################################################################################


	FXAA


################################################################################################
*/

char* rnd::vertFXAASource = {
	"#version 130\n"

	"in vec4 pos;" // clip space

	"noperspective out vec2 tdc;"
	//"noperspective in vec4 fxaaConsolePosPos;"
	
	"void main()"
	"{"
		"gl_Position = pos;"
		"tdc = pos.xy * 0.5 + 0.5;"
	"}"
};

char* rnd::fragFXAAPrependSource = {
	"#version 130\n"
	"#define FXAA_PC 1\n"
	"#define FXAA_GLSL_130 1\n"
	"#define FXAA_DISCARD 1\n"

	/* FXAA shader requires this extension is enabled if these macros are set because then it
	automatically defines FXAA_GATHER4_ALPHA. Apparently these extension macros are defined when
	the extension is supported, not necessarily enabled... */
	"#ifdef GL_ARB_gpu_shader5\n"
		"#extension GL_ARB_gpu_shader5 : enable\n"
	"#else\n"
		"#ifdef GL_NV_gpu_shader5\n"
			"#extension GL_NV_gpu_shader5 : enable\n"
		"#endif\n"
	"#endif\n"

	//"#define FXAA_QUALITY__PRESET 39\n" // FIXME: make this an option, recompile shader each time it's changed
											// Note fxaa_grain overrides this because it needs precise searching on the first few pixels
};

char* rnd::fragFXAAAppendSource = {
	"\n"
	// FXAA function inputs; commented out inputs are not used for FXAA Quality PC mode
	"noperspective in vec2 tdc;"
	//"noperspective in vec4 fxaaConsolePosPos;"
	"uniform sampler2D tex;" // RGBLuma color buffer, sRGB
	//"uniform sampler2D fxaaConsole360TexExpBiasNegOne;" // Pass in tex
	//"uniform sampler2D fxaaConsole360TexExpBiasNegTwo;" // Pass in tex
	"uniform vec2 fxaaQualityRcpFrame;" // 1/vidWidth, 1/vidHeight
	//"uniform vec4 fxaaConsoleRcpFrameOpt;"
	//"uniform vec4 fxaaConsoleRcpFrameOpt2;"
	//"uniform vec4 fxaaConsole360RcpFrameOpt2;"
	"uniform float fxaaQualitySubpix;" // [0, 1], steps of 0.25, default should be 0.75
	"uniform float fxaaQualityEdgeThreshold;" // Default 0.166
	"uniform float fxaaQualityEdgeThresholdMin;" // Default 0.0833
	//"uniform float fxaaConsoleEdgeSharpness;" // Default 8.0
	//"uniform float fxaaConsoleEdgeThreshold;" // Default 0.125
	//"uniform float fxaaConsoleEdgeThresholdMin;" // Default 0.05
	//"uniform vec4 fxaaConsole360ConstDir;"

	"out vec4 fragColor;"

	"void main()"
	"{"
		"fragColor = FxaaPixelShader("
			"tdc,"
			"vec4(0),"
			"tex,"
			"tex,"
			"tex,"
			"fxaaQualityRcpFrame,"
			"vec4(0),"
			"vec4(0),"
			"vec4(0),"
			"fxaaQualitySubpix,"
			"fxaaQualityEdgeThreshold,"
			"fxaaQualityEdgeThresholdMin,"
			"8.0,"
			"0.125,"
			"0.05,"
			"vec4(0)"
		");"
	"}"
};

// FIXME: revert to this if version 130 is unavailable?
#if 0
char* rnd::vertFXAASource = {
	"#version 120\n"

	"attribute vec4 pos;" // clip space

	"varying vec2 tdc;" // FIXME: should be noperspective; only in version 130?
	//"noperspective varying vec4 fxaaConsolePosPos;"
	
	"void main()"
	"{"
		"gl_Position = pos;"
		"tdc = pos.xy * 0.5 + 0.5;"
	"}"
};

char* rnd::fragFXAAPrependSource = {
	"#version 120\n"
	"#define FXAA_PC 1\n"
	"#define FXAA_GLSL_120 1\n"
	"#define FXAA_DISCARD 1\n"
	"#extension GL_EXT_gpu_shader4: enable\n"
		// FIXME: try to enable all extensions fxaa_3_11.h can use for for FXAA_FAST_PIXEL_OFFSET and FXAA_GATHER4_ALPHA?
};

char* rnd::fragFXAAAppendSource = {
	"\n"
	// FXAA function inputs; commented out inputs are not used for FXAA Quality PC mode
	"varying vec2 tdc;"
	//"noperspective varying vec4 fxaaConsolePosPos;"
	"uniform sampler2D tex;" // RGBLuma color buffer, sRGB
	//"uniform sampler2D fxaaConsole360TexExpBiasNegOne;" // Pass in tex
	//"uniform sampler2D fxaaConsole360TexExpBiasNegTwo;" // Pass in tex
	"uniform vec2 fxaaQualityRcpFrame;" // 1/vidWidth, 1/vidHeight
	//"uniform vec4 fxaaConsoleRcpFrameOpt;"
	//"uniform vec4 fxaaConsoleRcpFrameOpt2;"
	//"uniform vec4 fxaaConsole360RcpFrameOpt2;"
	"uniform float fxaaQualitySubpix;" // [0, 1], steps of 0.25, default should be 0.75
	"uniform float fxaaQualityEdgeThreshold;" // Default 0.166
	"uniform float fxaaQualityEdgeThresholdMin;" // Default 0.0833
	//"uniform float fxaaConsoleEdgeSharpness;" // Default 8.0
	//"uniform float fxaaConsoleEdgeThreshold;" // Default 0.125
	//"uniform float fxaaConsoleEdgeThresholdMin;" // Default 0.05
	//"uniform vec4 fxaaConsole360ConstDir;"

	"void main()"
	"{"
		"gl_FragColor = FxaaPixelShader("
			"tdc,"
			"vec4(0),"
			"tex,"
			"tex,"
			"tex,"
			"fxaaQualityRcpFrame,"
			"vec4(0),"
			"vec4(0),"
			"vec4(0),"
			"fxaaQualitySubpix,"
			"fxaaQualityEdgeThreshold,"
			"fxaaQualityEdgeThresholdMin,"
			"8.0,"
			"0.125,"
			"0.05,"
			"vec4(0)"
		");"
	"}"
};
#endif

char* rnd::fragFXAAPrependBlurSource = {
	"#define BLUR_EDGES 1\n"
};

char* rnd::fragFXAAPrependWarpSource = {
	"#define WARP_EDGES 1\n"
};

char* rnd::fragGrainSource = {
	"\n"

	"uniform float seed;"
	"uniform float grainScale;"
	"uniform float warpStraight;"
	"uniform float warpDiagonal;"

	// FXAA function inputs; commented out inputs are not used for FXAA Quality PC mode
	"noperspective in vec2 tdc;"
	//"noperspective in vec4 fxaaConsolePosPos;"
	"uniform sampler2D tex;" // RGBLuma color buffer, sRGB
	//"uniform sampler2D fxaaConsole360TexExpBiasNegOne;" // Pass in tex
	//"uniform sampler2D fxaaConsole360TexExpBiasNegTwo;" // Pass in tex
	"uniform vec2 fxaaQualityRcpFrame;" // 1/vidWidth, 1/vidHeight
	//"uniform vec4 fxaaConsoleRcpFrameOpt;"
	//"uniform vec4 fxaaConsoleRcpFrameOpt2;"
	//"uniform vec4 fxaaConsole360RcpFrameOpt2;"
	"uniform float fxaaQualitySubpix;" // [0, 1], steps of 0.25, default should be 0.75
	"uniform float fxaaQualityEdgeThreshold;" // Default 0.166
	"uniform float fxaaQualityEdgeThresholdMin;" // Default 0.0833
	//"uniform float fxaaConsoleEdgeSharpness;" // Default 8.0
	//"uniform float fxaaConsoleEdgeThreshold;" // Default 0.125
	//"uniform float fxaaConsoleEdgeThresholdMin;" // Default 0.05
	//"uniform vec4 fxaaConsole360ConstDir;"

	"out vec4 fragColor;"

	"void main()"
	"{"
		// FIXME: remove unused parameters
		"fragColor = FxaaPixelShader("
			"tdc,"
			"vec4(0),"
			"tex,"
			"tex,"
			"tex,"
			"fxaaQualityRcpFrame,"
			"vec4(0),"
			"vec4(0),"
			"vec4(0),"
			"fxaaQualitySubpix,"
			"fxaaQualityEdgeThreshold,"
			"fxaaQualityEdgeThresholdMin,"
			"8.0,"
			"0.125,"
			"0.05,"
			"vec4(0),"
			"seed,"
			"gl_FragCoord.xy * grainScale,"
			"warpStraight,"
			"warpDiagonal"
		");"
	"}"
};