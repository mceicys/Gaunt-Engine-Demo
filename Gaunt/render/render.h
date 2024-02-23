// render.h -- Rendition system
// Martynas Ceicys

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include "mesh.h"
#include "palette.h"
#include "regs.h"
#include "texture.h"
#include "../console/option.h"
#include "../../GauntCommon/json.h"
#include "../../GauntCommon/math.h"
#include "../resource/resource.h"
#include "../scene/scene.h"
#include "../gui/gui.h"

namespace rnd
{

/*
################################################################################################
	ENTITY/IMAGE REGISTER
################################################################################################
*/

// FIXME: remove image registrations
Texture*	RegisteredTexture(const img_reg& reg);
img_reg*	RegisterImage(gui::iImage& img, Texture& tex);
void		UnregisterImage(img_reg*& regInOut);

/*
################################################################################################
	PALETTE
################################################################################################
*/

Palette*	FindPalette(const char* fileName);
Palette*	EnsurePalette(const char* fileName);
Palette*	CurrentPalette();
void		SetPalette(Palette& p, bool forceReset = false);

/*
################################################################################################
	TEXTURE
################################################################################################
*/

Texture* FindTexture(const char* fileName);
Texture* EnsureTexture(const char* fileName);

/*
################################################################################################
	MESH
################################################################################################
*/

Mesh* FindMesh(const char* fileName);
Mesh* EnsureMesh(const char* fileName);

/*
################################################################################################
	ANIMATION
################################################################################################
*/

float	AdvanceAnimation(const Animation& ani, float frame, float advance, bool loop);
void	AdvanceTrack(float time, const Animation* ani, float& frameIO, float speed, bool loop,
		float& transIO, size_t& aIO, size_t& bIO, float& fIO);
void	AbsFrame(const Animation* ani, float frame, size_t& aOut, size_t& bOut, float& fOut);

/*
################################################################################################
	WORLD
################################################################################################
*/

void RegisterWorld(const scn::TriBatch* batches, uint32_t numBatches, scn::Zone* zones,
	scn::ZoneExt* zoneExts, uint32_t numZones);

/*
################################################################################################
	SKY
################################################################################################
*/

bool			SetSkyFace(int face, const char* fileName);
const Texture*	SkySphereTexture();
void			SetSkySphereTexture(const Texture* tex);

/*
################################################################################################
	LINE
################################################################################################
*/

void DrawLine(const com::Vec3& a, const com::Vec3& b, int color, float time = 0.0f);
void Draw2DLine(const com::Vec2& a, const com::Vec2& b, int color, float time = 0.0f);
void DrawSphereLines(const com::Vec3& pos, const com::Qua ori, float radius, int color,
	float time = 0.0f);

/*
################################################################################################
	SPACE
################################################################################################
*/

com::Vec3	LensDirection(const scn::Camera& cam, const com::Vec2& user);
com::Vec3	VecUserToWorldZPlane(const scn::Camera& cam, const com::Vec2& user, float z);
com::Vec3	VecWorldToUser(unsigned width, unsigned height, const scn::Camera& cam,
			const com::Vec3& world, float* linDepthOut, float halfDist = 1000.0f);

/*
################################################################################################
	SHADOW
################################################################################################
*/

void CalculateCascadeDistances(float logFraction);

/*
################################################################################################
	CURVE
################################################################################################
*/

void		SetCurvePoint(float x, float y);
com::Vec2	CurvePoint(size_t i);
size_t		NumCurvePoints();
size_t		FindCurvePoint(float x);
void		DeleteCurvePoint(size_t i);
void		ClearCurvePoints();

/*
################################################################################################
	OPTIONS
################################################################################################
*/

extern con::Option
	checkErrors,
	mipMapping,
	mipBias,
	mipFade,
	filterTextureBrightness,
	filterTextureColor,
	anchorTexelNormals,
	skySphereSeamMipBias,
	usingFBOs,
	forceNoFBOs,
	nearClip,
	farClip,
	preciseClipRange,
	highDepthBuffer,
	forceLowDepthBuffer,
	shadowRes,
	sunShadowRes,
	kernelSize,
	kernelSizeSpot,
	kernelSize0,
	kernelSize1,
	kernelSize2,
	kernelSize3,
	cascadeDist0,
	cascadeDist1,
	cascadeDist2,
	cascadeDist3,
	cascadePurity,
	cascadeExp0,
	cascadeExp1,
	cascadeExp2,
	cascadeExp3,
	polyBias,
	polyBiasSpot,
	polyBias0,
	polyBias1,
	polyBias2,
	polyBias3,
	polySlopeBias,
	polySlopeBiasSpot,
	polySlopeBias0,
	polySlopeBias1,
	polySlopeBias2,
	polySlopeBias3,
	depthBias,
	surfaceBias,
	surfaceBiasSpot,
	surfaceBias0,
	surfaceBias1,
	surfaceBias2,
	surfaceBias3,
	vertexBias,
	vertexBiasSpot,
	spotShadowAddedAngle,
	tighterSpotCull,
	light16,
	forceLowLightBuffer,
	maxIntensity,
	maxIntensityLow,
	numColorBands,
	numColorBandsLow,
	lightFade,
	lightFadeLow,
	exposure,
	exposureRandom,
	lightGamma,
	lightCurve,
	numCurveSegments,
	worldAmbientFactor,
	worldAmbientExponent,
	ditherRandom,
	ditherGrain,
	ditherGrainSize,
	ditherAmount,
	rgbBlend,
	rampAdd,
	noiseTime,
	portalColor,
	sunPortalColor,
	entityColor,
	sunEntityColor,
	bulbColor,
	cameraColor,
	curveColor,
	leafColor,
	solidLeafColor,
	lineWidth,
	timeRender,
	lockCullCam,
	antiAliasing,
	fxaaQualitySubpix,
	fxaaQualityEdgeThreshold,
	fxaaQualityEdgeThresholdMin,
	forceNoGather4,
	warpGrainSize,
	warpAmount,
	warpStraight,
	warpDiagonal;

// FIXME: cmds should be able to interpret constants
enum
{
	FILTER_TEXTURE_BRIGHTNESS_OFF = 0,
	FILTER_TEXTURE_BRIGHTNESS_ON,
	FILTER_TEXTURE_BRIGHTNESS_DITHER
};

enum
{
	ANTI_ALIASING_NONE = 0,
	ANTI_ALIASING_FXAA,
	ANTI_ALIASING_RIGOROUS_FXAA,
	ANTI_ALIASING_SOFT_FXAA,
	ANTI_ALIASING_WARPED_FXAA, // FIXME: rename to "filmic" or "grain?"
	NUM_ANTI_ALIASING_OPTIONS
};

/*
################################################################################################
	GENERAL
################################################################################################
*/

void	UpdateViewport();
void	Frame();
void	PreTick();
void	DeleteUnused();
void	Init();

}

#endif