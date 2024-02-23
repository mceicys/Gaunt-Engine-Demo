// render_private.h -- Stuff shared between render source files
// Martynas Ceicys

#ifndef RENDER_PRIVATE_H
#define RENDER_PRIVATE_H

#include <math.h>

#include "render.h"
#include "../../GauntCommon/math.h"
#include "../../GauntCommon/pair.h"
#include "../../GauntCommon/vec.h"
#include "../gui/gui.h"
#include "../hit/hull.h"
#include "../scene/scene.h"
#include "../script/script.h"
#include "../wrap/glengine.h"

#define RND_VARIABLE_TEXTURE_UNIT		GL_TEXTURE0 // Used for everything except palettes
#define RND_VARIABLE_TEXTURE_NUM		0
#define RND_VARIABLE_2_TEXTURE_UNIT		GL_TEXTURE1
#define RND_VARIABLE_2_TEXTURE_NUM		1
#define RND_VARIABLE_3_TEXTURE_UNIT		GL_TEXTURE2
#define RND_VARIABLE_3_TEXTURE_NUM		2
#define RND_PALETTE_TEXTURE_UNIT		GL_TEXTURE3
#define RND_PALETTE_TEXTURE_NUM			3
#define RND_SUB_PALETTES_TEXTURE_UNIT	GL_TEXTURE4
#define RND_SUB_PALETTES_TEXTURE_NUM	4
#define RND_RAMP_TEXTURE_UNIT			GL_TEXTURE5
#define RND_RAMP_TEXTURE_NUM			5
#define RND_RAMP_LOOKUP_TEXTURE_UNIT	GL_TEXTURE6
#define RND_RAMP_LOOKUP_TEXTURE_NUM		6
#define RND_CURVE_TEXTURE_UNIT			GL_TEXTURE7
#define RND_CURVE_TEXTURE_NUM			7

// FIXME: recheck these before release
#define RND_MIN_NUM_FRAG_TEXTURE_UNITS		7
#define RND_MIN_NUM_COMBINED_TEXTURE_UNITS	8
#define RND_MIN_NUM_VERTEX_ATTRIBS			8

#define RND_MIN_STENCIL_BITS (3 + scn::NUM_OVERLAYS)
	// bit 0:		bulb stencil, temporary stuff
	// bit max:		sky
	// bit max - 1:	overlay 0
	// bit max - 2:	overlay 1
	// bit max - 3: clouds // FIXME: remove?

#define RND_SQRT_2 1.4142135623730950488016887242097f
#define RND_SQRT_HALF 0.70710678118654752440084436210485f

#define RND_MAX_SPOT_ANGLE 1.5f

#define RND_MODEL_PASS_ATTRIB_POS0		0
#define RND_MODEL_PASS_ATTRIB_POS1		1
#define RND_MODEL_PASS_ATTRIB_TEXCOORD	2 // FIXME: this should come after norm0 and norm1
#define RND_MODEL_PASS_ATTRIB_NORM0		3
#define RND_MODEL_PASS_ATTRIB_NORM1		4

#define RND_LIGHT_PASS_ATTRIB_POS0		0
#define RND_LIGHT_PASS_ATTRIB_POS1		1
#define RND_LIGHT_PASS_ATTRIB_TEXCOORD	2
#define RND_LIGHT_PASS_ATTRIB_NORM0		3
#define RND_LIGHT_PASS_ATTRIB_NORM1		4

#define RND_SUB_PAL_BIAS 0.1f

namespace rnd
{

// Image vertex
// FIXME: rename, not "g"
struct vertex_g
{
	GLfloat	pos[3];
	GLfloat	texCoord[2];
	GLfloat	subPalette;
};

struct vertex_mesh_tex
{
	GLfloat	texCoord[2];
};

struct vertex_mesh_place
{
	GLfloat	pos[3];
	GLfloat	normal[3];

	operator com::Vec3() const {return com::Vec3(pos[0], pos[1], pos[2]);}
};

struct vertex_world
{
	GLfloat	pos[3];
	GLfloat	texCoord[2];
	GLfloat	normal[3];
	GLfloat	subPalette, ambient, darkness;

	operator com::Vec3() const {return com::Vec3(pos[0], pos[1], pos[2]);}
};

struct vertex_simple
{
	GLfloat	pos[3];

	operator com::Vec3() const {return com::Vec3(pos[0], pos[1], pos[2]);}
};

struct img_reg
{
	gui::iImage*	img;
	Texture*		tex;
	vertex_g		verts[4];

	img_reg*		prev;
	img_reg*		next;
};

/*======================================
	rnd::PaletteGL
======================================*/
class PaletteGL : public Palette
{
public:
	GLuint	texPalette, texSubPalettes, texRamps, texRampLookup;
	GLfloat	subCoordScale, rampDistScale, invNumRampTexels;

			PaletteGL(const char* fileName, uint32_t numSubPalettes,
			unsigned char numFirstBrights, uint32_t maxRampSpan, const GLubyte* palette,
			const GLubyte* subPalettes, const GLubyte* ramps, uint16_t numRampTexels,
			const GLubyte* rampLookup);
			~PaletteGL();

private:
			PaletteGL();
			PaletteGL(const PaletteGL&);
};

/*======================================
	rnd::TextureGL
======================================*/
class TextureGL : public Texture
{
public:
	GLuint		texName;
	img_reg*	lastImg;
	size_t		numImgs;

				TextureGL(const char* fileName, const uint32_t (&dims)[2], uint32_t numMipmaps,
					uint32_t numFrames, frame* frames, GLubyte* image);
				~TextureGL();

	GLint		MipFilter() const;
	void		ResetMipFilter();
	GLint		MinFilter(bool linear) const;

private:
				TextureGL();
				TextureGL(const TextureGL&);
};

/*======================================
	rnd::MeshGL
======================================*/
class MeshGL : public Mesh
{
public:
	GLint		numFrameVerts;
	GLsizei		numFrameIndices;
	size_t		texDataSize; // Number of initial bytes in vertex buffer storing tex coords
	GLuint		vBufName;
	GLuint		iBufName;
	GLenum		indexType;
	GLfloat		voxelInvDims[3], voxelOrigin[3], voxelScale;
	GLuint		texVoxels;

			MeshGL(const char* fileName, uint32_t numFrames, uint32_t frameRate,
			Socket* sockets, uint32_t numSockets, Animation* animations, uint32_t numAnimations,
			const com::Vec3& boxMin, const com::Vec3& boxMax, float radius, GLint numFrameVerts,
			GLsizei numFrameIndices, GLenum indexType, const void* vp,
			const vertex_mesh_tex* vt, const void* indices, const GLfloat* voxels,
			GLfloat voxelScale, const int32_t* voxelMin, const uint32_t* voxelDims);
			~MeshGL();

private:
			MeshGL();
			MeshGL(const MeshGL&);
};

// Engine-side mesh with one frame and vertices without tex-coords and normals
struct simple_mesh
{
	GLint	numVerts;
	GLsizei	numIndices;
	GLuint	vBufName;
	GLuint	iBufName;
	GLenum	indexType;
};

// For drawing a textured zone
struct zone_reg
{
	struct draw_batch
	{
		GLint		firstIndex;
		GLint		byteOffset;
		GLsizei		numElements;
		Texture*	tex;
	};

	draw_batch*		drawBatches;
	uint32_t		numDrawBatches;
	GLsizei			totalElements;
};

/*======================================
	rnd::Timer
======================================*/
class Timer
{
public:
					Timer();
					~Timer();
	void			Init();
	void			Start();
	void			Stop();
	void			Reset();
	GLuint			Accumulated();

private:
	GLuint			query;
	GLuint			accum;
	bool			pending;
	static Timer*	timing;

	void			UpdateAccumulated();
};

// render.cpp
struct render_extensions
{
	bool fbo, depthBufferFloat, clipControl, timer;
};

extern render_extensions extensions;
extern GLfloat gWorldToView[16], gViewToClip[16], gWorldToClip[16], gClipToFocal[16];
extern GLfloat gOverlayWTC[2][16], gOverlayCTF[2][16];
extern GLfloat gSkyVTC[16], gSkyWTC[16];
typedef GLfloat screen_vertex[3];
extern GLuint nearScreenVertexBuffer, farScreenVertexBuffer;
extern GLuint texGeometryBuffer, texDepthBuffer, texExtraDepthBuffer, texLightBuffer,
	texShadowBuffer, texFlatShadowBuffer, texCompositionBuffer, texCurve;
extern GLuint fboGeometryBuffer, fboLightBuffer, fboShadowBuffer, fboFlatShadowBuffer,
	fboCompositionBuffer;
extern GLsizei allocShadowRes, allocCascadeRes;
extern GLint stencilBits;
extern GLint skyBit, overlayStartBit, cloudBit;
extern GLuint skyMask, overlayMask, overlayRelitMask, cloudMask;
extern simple_mesh *lightSphere, *lightHemi;
extern GLfloat shaderRandomSeed, shaderRandomSeed2; // FIXME: remove 2?

enum TIMER_QUERIES
{
	TIMER_OVERLAY_PASS, // Doesn't include glass overlays, those go in TIMER_GLASS_PASS
	TIMER_WORLD_PASS,
	TIMER_MODEL_PASS,
	TIMER_CLOUD_PASS,
	TIMER_SKY_PASS,
	TIMER_SKY_LIGHT_PASS,
	TIMER_AMBIENT_PASS,
	TIMER_SUN_SHADOW_PASS,
	TIMER_SUN_LIGHT_PASS,
	TIMER_SPOT_SHADOW_PASS,
	TIMER_SPOT_LIGHT_PASS,
	TIMER_BULB_SHADOW_PASS,
	TIMER_BULB_LIGHT_PASS,
	TIMER_WORLD_GLASS_PASS,
	TIMER_GLASS_PASS,
	TIMER_COMPOSITION_PASS,
	TIMER_ANTI_ALIASING_PASS,
	TIMER_IMAGE_PASS,
	TIMER_MISC,
	NUM_TIMERS
};

extern Timer timers[NUM_TIMERS];
extern bool doTiming;

struct prog_uniform
{
	GLint*	name;
	GLchar*	identifier;
	bool	silent; // Don't log a warning if identifier is not found in shader
		// FIXME: silence all or none based on a #define instead?
};

struct prog_attribute
{
	GLuint	index;
	GLchar*	identifier;
};

void		WorldToView(const com::Vec3& pos, const com::Qua& ori, GLfloat (&mOut)[16]);
void		WorldToViewSkew(const com::Vec3& pos, const com::Qua& ori,
			const com::Vec3& skewOrigin, const com::Vec2& zSkew, GLfloat (&mOut)[16]);
void		ViewToWorld(const com::Vec3& pos, const com::Qua& ori, GLfloat (&mOut)[16]);
void		ViewToClipPers(GLfloat aspect, GLfloat vHalfFOV, GLfloat clipNear,
			GLfloat (&mOut)[16]);
void		WorldToClipPers(GLfloat aspect, GLfloat vfov, GLfloat clipNear,
			const com::Vec3& pos, const com::Qua& ori, GLfloat (&mOut)[16]);
void		ViewToClipPersLimited(GLfloat aspect, GLfloat vHalfFOV, GLfloat clipNear,
			GLfloat clipFar, GLfloat (&mOut)[16]);
void		ViewToClipOrth(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
			GLfloat clipNear, GLfloat clipFar, GLfloat (&mOut)[16]);
void		WorldToSunView(GLfloat pitch, GLfloat yaw, const com::Vec3& pos,
			GLfloat (&mOut)[16]);
bool		InitProgram(const char* title, GLuint& progOut, GLuint& vertIO,
			const char* const* vertSrcs, size_t numVertSrcs, GLuint& fragIO,
			const char* const* fragSrcs, size_t numFragSrcs, const prog_attribute* attributes,
			prog_uniform* uniforms);
bool		CreateShader(const char* title, GLenum type, const char* const* sources, size_t num,
			GLuint& nameOut);
void		CreateProgram(GLuint vertName, GLuint fragName, GLuint& name);
bool		LinkProgram(GLuint name);
void		CopyFrameBuffer(GLuint texture);
void		MakeDitherArray(GLfloat (&ditherOut)[16], GLfloat divisorFactor = 1.0f);
void		MakeSignedDitherArray(GLfloat (&ditherOut)[16], GLfloat divisorFactor = 1.0f);
void		MakeBilinearDitherArray(GLfloat (&ditherOut)[32]);
const char*	GetErrorString(GLenum err);
const char*	GetFramebufferStatusString(GLenum stat);
GLint		StencilLimit();
GLfloat		RandomSeed();
void		GenerateScreenQuad(GLuint& bufferOut, float z);
void		GenerateScreenTri(GLuint& bufferOut, float z);
float		NormalizedIntensity(float intensity);
float		CurrentMaxIntensity();
float		RampVariance();

// render_palette.cpp
extern uint32_t	topInfluence;
extern GLfloat	influenceFactor, unpackFactor;

float		NormalizeSubPalette(int subPalette);
float		PackedLightSubPalette(int subPalette);
PaletteGL*	CurrentPaletteGL();

// render_texture.cpp
size_t		NumTextures();
const char*	LoadTextureFile(const char* filePath, GLubyte*& imageOut, uint32_t (&dimsOut)[2],
			uint32_t& numMipmapsOut, uint32_t& numFramesOut, Texture::frame*& framesOut,
			bool flip = true);
void		FreeTextureImage(GLubyte* image);
void		FreeTextureFrames(Texture::frame* frames);

// render_mesh.cpp
simple_mesh* CreateSimpleMesh(const char* filePath);

// render_curve.cpp
void CheckCurveUpdates();
void CalculateCurveTexture();
void DrawCurve(int color);

// render_vis.cpp
extern com::Arr<const scn::Zone*> visZones;
extern com::Arr<const scn::Entity*> visEnts, visCloudEnts, visGlassEnts;
extern com::Arr<const scn::Bulb*> visBulbs, visSpots;
extern size_t numVisZones, numVisEnts, numVisCloudEnts, numVisGlassEnts, numVisBulbs,
	numVisSpots;
extern hit::Frustum camHull;
extern com::Vec3 prevSunDir;

void	FloodVisible(unsigned& zoneVisCodeOut);
void	FloodSunVisible();
bool	TrimVisibleSunBranches(unsigned zoneVisCode);
void	CascadeDrawLists(const com::Vec3& pos, float pitch, float yaw, const com::Vec3& fMin,
		const com::Vec3& fMax, com::Arr<const scn::Zone*>& litZonesOut, size_t& numLitZonesOut,
		com::Arr<const scn::Entity*>& litEntsOut, size_t& numLitEntsOut);
void	BulbDrawList(const scn::Bulb& bulb, float radius, const com::Vec3& pos,
		com::Arr<const scn::Entity*>& litEntsOut, size_t& numLitEntsOut, uint16_t& overlaysOut);
void	BulbLitFlags(const scn::Bulb& bulb, float radius, const com::Vec3& pos,
		uint16_t& overlaysOut);
float	PortalGap(const scn::PortalSet& set, const com::Vec3& pos, bool fwd);
float	PortalDot(const scn::PortalSet& set, const com::Vec3& dir, bool fwd);

// render_world.cpp
extern GLuint		worldVertexBuffer;
extern GLuint		worldElementBuffer;
extern zone_reg*	zoneRegs;
extern uint32_t		numZoneRegs;

void		WorldPass();
bool		InitWorldPass();
bool		ReinitWorldProgram();
void		DeleteWorldProgram();
void		EnsureWorldProgram();
void		WorldDrawZone(const scn::Zone* z);

template	<typename pass>
void		WorldDrawZoneTextured(pass& p, const scn::Zone* z);

#include "render_world_template.h"

// render_world_glass_pass.cpp
void	WorldGlassPass();
bool	InitWorldGlassPass();

// render_model_pass.cpp

/*======================================
	rnd::RegularModelProg
======================================*/
class RegularModelProg
{
public:
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniTexShift, uniModelToClip, uniModelToNormal, uniTexDims, uniSubPalette,
			uniOpacity, uniDither, uniMipBias, uniMipFade, uniRampDistScale,
			uniInvNumRampTexels;
	GLint	samTexture, samSubPalettes, samRamps, samRampLookup;

	void	UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2], float lerp,
			const GLfloat (&wtc)[16], const com::Qua* normOri);
	void	UndoStateForEntity(const scn::Entity&) {}
};

extern RegularModelProg modelProg;

void		ModelUploadMTC(GLint uniMTC, const scn::Entity& ent,
			const GLfloat (&wtc)[16]);
void		ModelUploadMTCAndMTN(GLint uniMTC, GLint uniMTN, const scn::Entity& ent,
			const GLfloat (&wtc)[16], const com::Qua* normOri);
void		ModelUploadLerp(GLint uniLerp, float lerp);
void		ModelUploadTexShift(GLint uniTexShift, const scn::Entity& ent);
void		ModelUploadTexDims(GLint uniTexDims, const scn::Entity& ent);
void		ModelUploadSubPalette(GLint uniSubPalette, const scn::Entity& ent);
void		ModelUploadOpacity(GLint uniOpacity, const scn::Entity& ent);
void		CloudSetVoxelState(GLint uniModelCam, GLint uniModelToVoxel,
			GLint uniVoxelInvDims, GLint uniVoxelScale, const scn::Entity& ent);
void		ModelVertexAttribOffset(const scn::Entity& ent, const MeshGL& msh,
			size_t (&offsetsOut)[2], float& lerpOut);
void		ModelSetPlaceVertexAttribPointers(const Mesh& msh, const size_t (&offsets)[2]);
void		ModelSetTexcoordVertexAttribPointer(const Mesh& msh);
void		ModelDrawElements(const MeshGL& msh);
void		ModelToWorld(const com::Vec3& pos, const com::Qua& ori, float scale,
			GLfloat (&mtwOut)[16], GLfloat (*mtnOut)[16] = 0, const com::Qua* normOri = 0);
void		ModelToWorld(const scn::Entity& ent, GLfloat (&mOut)[16]);
com::Vec3	ModelCamPos(const scn::Entity& ent, const scn::Camera& cam);
void		ModelState();
void		ModelCleanup();
void		ModelPass();
bool		FilterNoGlass(const scn::Entity& ent);
void		OverlayPass();
bool		InitModelPass();
bool		ReinitModelProgram();
void		DeleteModelProgram();
void		EnsureModelProgram();

#include "render_model_pass_template.h"

// render_sky.cpp
void		SkyPass();
bool		InitSkyPass();
bool		ReinitSkySphereProgram();
void		DeleteSkySphereProgram();
void		EnsureSkySphereProgram();
void		SkyLightPass();
bool		InitSkyLightPass();

// render_light_pass.cpp
struct shadow_pass_type
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniLerp, uniToClip, uniVertexBias, uniBulbPos;
};

struct shadow_fade_pass_type
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniTexShift, uniLerp, uniToClip, uniVertexBias, uniBulbPos, uniOpacity, uniSeed;
	GLint	samTexture;
};

extern com::Arr<const scn::Zone*> litZones; // Kept allocated for shadow map functions
extern com::Arr<const scn::Entity*>	litEnts;
extern shadow_pass_type shadowPass;
extern shadow_fade_pass_type shadowFadePass;

void		LightPass(bool drawSun);
bool		InitLightPass();
void		DrawShadowMapFace(const scn::Bulb& bulb, const com::Vec3& pos, const com::Qua& ori,
			const GLfloat (&bulbViewToClip)[16], const hit::Frustum& frustum,
			const scn::Entity** litEnts, size_t numLitEnts, bool& fadeProgIO,
			const Mesh*& curMshIO, const Texture*& curTexIO);
int			CmpShadowEntities(const void* a, const void* b);
void		DrawBulbStencil(const simple_mesh* mesh, float radius, const com::Vec3& pos,
			const com::Qua& ori);

/*--------------------------------------
	rnd::SetSharedShadowUniforms
--------------------------------------*/
template <typename pass>
void SetSharedShadowUniforms(const pass& p, const com::Vec3& bulbPos, const scn::Entity& ent,
	const MeshGL& msh, const GLfloat (&bulbWTC)[16], GLfloat (&bulbMTC)[16],
	size_t (&offsets)[2])
{
	GLfloat bulbMTW[16];
	com::Vec3 entPos = ent.FinalPos();
	com::Qua entOri = ent.FinalOri();
	ModelToWorld(entPos, entOri, ent.FinalScale(), bulbMTW);
	com::Multiply4x4(bulbMTW, bulbWTC, bulbMTC);
	glUniformMatrix4fv(p.uniToClip, 1, GL_FALSE, bulbMTC);

	float lerp;
	ModelVertexAttribOffset(ent, msh, offsets, lerp);
	glUniform1f(p.uniLerp, lerp);

	com::Vec3 bulbModelPos = com::VecRotInv(bulbPos - entPos, entOri);
	glUniform3f(p.uniBulbPos, bulbModelPos.x, bulbModelPos.y, bulbModelPos.z);
}

// render_light_spot_pass.cpp
void		SpotPass();
bool		InitSpotLightPass();

// render_glass_pass.cpp
void		GlassPass();
bool		InitGlassPass();
bool		ReinitGlassRecolorProgram();
void		DeleteGlassRecolorProgram();
void		EnsureGlassRecolorProgram();

// render_composition_pass.cpp
void		CompositionPass();
bool		InitCompositionPass();

// render_image_pass.cpp
void		ImagePass();
bool		InitImagePass();

// render_line_pass.cpp
void		AdvanceLines();
void		LinePass();
bool		InitLinePass();

// render_restore_pass.cpp
void		RestoreDepthBuffer();
bool		InitRestorePass();

// render_anti_aliasing_pass.cpp
void		AntiAliasingPass();
bool		ReinitAntiAliasingProgram();
void		DeleteAntiAliasingProgram();
void		EnsureAntiAliasingProgram();

// render_shader.cpp
extern char *vertSimpleSource, *fragSimpleSource,
	*vertWorldSource, *fragWorldSource,
	*vertWorldGlassSource, *fragWorldGlassSource,
	*vertModelSource, *fragModelSource,
	*fragModelFadeSource,
	*vertModelNoNormalSource,
	*fragModelAddSource,
	*fragModelAmbientSource,
	*fragModelMulSource,
	*fragModelRecolorSource,
	*vertModelCloudSource, *fragModelCloudSource,
	*vertSkyBoxSource, *fragSkyBoxSource,
	*fragSkySphereSource,
	*vertSkyLightSource, *fragSkyLightSource,
	*vertAmbientSource, *fragAmbientSource,
	*vertShadowSource, *fragShadowSource,
	*vertShadowFadeSource, *fragShadowFadeSource,
	*vertShadowSunSource,
	*vertShadowSunFadeSource,
	*fragSunSource,
	*vertBulbStencilSource, *fragBulbStencilSource,
	*vertBulbLightSource, *fragBulbLightSource,
	*fragSpotLightSource,
	*vertCompSource, *fragCompSource,
	*vertCloudCompSource,
	*fragCloudCompSourceMedium,
	*fragCloudCompSourceHigh,
	*vertImageSource, *fragImageSource,
	*vertLineSource, *vertLine2DSource, *fragLineSource,
	*vertRestoreSource, *fragRestoreSource,

	*vertFXAASource, *fragFXAAPrependSource, *fragFXAAAppendSource, *fragFXAAPrependBlurSource,
	*fragFXAAPrependWarpSource, *vertGrainSource, *fragGrainSource;

extern const char
	*shaderDefineMipMapping,
	*shaderDefineMipFade,
	*shaderDefineFilterBrightness,
	*shaderDefineFilterColor,
	*shaderDefineAnchorTexelNormals,
	*shaderDefineFXAAGather4;

void SetShaderDefines();

}

#endif