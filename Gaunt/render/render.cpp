// render.cpp -- Rendition system
// Martynas Ceicys

#include <stdint.h>
#include <stdio.h>
#include <math.h> // ceil floor

#include "render.h"
#include "render_private.h"
#include "render_lua.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../hit/hit.h"
#include "../script/script.h"
#include "../vector/vec_lua.h"
#include "../wrap/wrap.h"

namespace rnd
{
	// GENERAL
	render_extensions extensions;
	GLfloat gWorldToView[16], gViewToClip[16], gWorldToClip[16], gClipToFocal[16];
	GLfloat gOverlayWTC[2][16], gOverlayCTF[2][16];
	GLfloat gSkyVTC[16], gSkyWTC[16];
	GLuint nearScreenVertexBuffer, farScreenVertexBuffer;
	GLuint texGeometryBuffer, texDepthBuffer, texExtraDepthBuffer, texLightBuffer,
		texShadowBuffer, texFlatShadowBuffer, texCompositionBuffer, texCurve;
	GLuint fboGeometryBuffer = 0, fboLightBuffer = 0, fboShadowBuffer = 0,
		fboFlatShadowBuffer = 0, fboCompositionBuffer = 0;
	GLsizei allocShadowRes, allocCascadeRes;
	GLint stencilBits;
	GLint skyBit, overlayStartBit, cloudBit;
	GLuint skyMask, overlayMask, overlayRelitMask, cloudMask;
	simple_mesh *lightSphere, *lightHemi;
	GLfloat shaderRandomSeed, shaderRandomSeed2;
	Timer timers[NUM_TIMERS];
	bool doTiming;

	const char* TIMER_NAMES[NUM_TIMERS] = {
		"overlay",
		"world",
		"model",
		"cloud",
		"sky",
		"sky light",
		"ambient",
		"sun shadow",
		"sun light",
		"spot shadow",
		"spot light",
		"bulb shadow",
		"bulb light",
		"world glass",
		"glass",
		"composition",
		"anti-aliasing",
		"image",
		"misc"
	};

	void	UpdateMatrices();
	void	UpdatePerspectiveMatrices(const com::Vec3& pos, const com::Qua& ori, float vfov,
			const com::Vec3& skewOrigin, const com::Vec2& zSkew, GLfloat (&wtv)[16],
			GLfloat (&vtc)[16], GLfloat (&wtc)[16], GLfloat (&ctf)[16]);
	void	UpdateShadowBuffer();
	void	UpdateNoise();
	void	EnsureShaderPrograms();
}

/*
################################################################################################


	IMAGE REGISTER


################################################################################################
*/

/*--------------------------------------
	rnd::RegisteredTexture
--------------------------------------*/
rnd::Texture* rnd::RegisteredTexture(const img_reg& reg)
{
	return reg.tex;
}

/*--------------------------------------
	rnd::RegisterImage

Registers image to a texture.
--------------------------------------*/
rnd::img_reg* rnd::RegisterImage(gui::iImage& img, Texture& tex)
{
	TextureGL& t = (TextureGL&)tex;
	img_reg* reg = new img_reg;
	reg->img = &img;
	reg->tex = &tex;
	reg->tex->AddLock();
	COM_LINK(t.lastImg, reg);
	t.numImgs++;
	return reg;
}

/*--------------------------------------
	rnd::UnregisterImage

Removes image from the texture list and sets regInOut to 0.
--------------------------------------*/
void rnd::UnregisterImage(img_reg*& regInOut)
{
	TextureGL* tex = (TextureGL*)regInOut->tex;
	tex->RemoveLock();
	COM_UNLINK(tex->lastImg, regInOut);
	tex->numImgs--;
	delete regInOut;
	regInOut = 0;
}

/*
################################################################################################


	SPACE


################################################################################################
*/

/*--------------------------------------
	rnd::LensDirection

Return world-space direction pointing away from the user at the given user-space position.
--------------------------------------*/
com::Vec3 rnd::LensDirection(const scn::Camera& cam, const com::Vec2& user)
{
	float height = (float)wrp::VideoHeight();
	float width = (float)wrp::VideoWidth();
	float vFOV = cam.FinalFOV();
	float hFOV = com::HorizontalFOV(vFOV, width / height);
	float hori = tan(hFOV * 0.5f);
	float vert = tan(vFOV * 0.5f);

	com::Vec3 lens = com::Vec3(
		1.0f,
		-hori * (user.x / width * 2.0f - 1.0f),
		vert * (user.y / height * 2.0f - 1.0f)
	).Normalized();

	return com::VecRot(lens, cam.FinalOri());
}

/*--------------------------------------
	rnd::VecUserToWorldZPlane
--------------------------------------*/
com::Vec3 rnd::VecUserToWorldZPlane(const scn::Camera& cam, const com::Vec2& user, float z)
{
	com::Vec3 lens = LensDirection(cam, user);

	if(lens.z == 0.0f)
		return com::Vec3(0.0f, 0.0f, z);

	com::Vec3 camPos = cam.FinalPos();
	float dist = (z - camPos.z) / lens.z;
	return lens * dist + camPos;
}

/*--------------------------------------
	rnd::VecWorldToUser

Returned z is linear depth [-inf, inf] mapped to [-1, 1] by the equation:
	depth / (abs(depth) + halfDist)

halfDist controls what depth becomes 0.5.
--------------------------------------*/
com::Vec3 rnd::VecWorldToUser(unsigned width, unsigned height, const scn::Camera& cam,
	const com::Vec3& world, float* linDepth, float halfDist)
{
	if(!width || !height)
	{
		if(linDepth)
			*linDepth = 0.0f;

		return 0.0f;
	}

	GLfloat worldToView[16];
	WorldToView(cam.FinalPos(), cam.FinalOri(), worldToView);
	GLfloat viewToClip[16];
	ViewToClipPers((GLfloat)width / height, cam.fov * 0.5f, nearClip.Float(), viewToClip);
	com::Vec3 view = com::Multiply4x4(world, worldToView);

	if(linDepth)
		*linDepth = view.x;

	com::Vec3 ndc = com::Multiply4x4Homogeneous(view, viewToClip);

	return com::Vec3(
		(ndc.x * 0.5f + 0.5f) * (float)width,
		(ndc.y * 0.5f + 0.5f) * (float)height,
		(view.x == 0.0f) ? 0.0f : (view.x / (fabs(view.x) + halfDist))
	);
}

/*
################################################################################################


	SPACE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::LensDirection

IN	camC, v2User
OUT	v3Dir
--------------------------------------*/
int rnd::LensDirection(lua_State* l)
{
	vec::LuaPushVec(l, LensDirection(*scn::Camera::CheckLuaTo(1), vec::LuaToVec(l, 2, 3)));
	return 3;
}

/*--------------------------------------
LUA	rnd::VecUserToWorldZPlane

IN	camC, v2User, nZ
OUT	v3World
--------------------------------------*/
int rnd::VecUserToWorldZPlane(lua_State* l)
{
	vec::LuaPushVec(l, VecUserToWorldZPlane(*scn::Camera::CheckLuaTo(1), vec::LuaToVec(l, 2, 3),
		lua_tonumber(l, 4)));
	return 3;
}

/*--------------------------------------
LUA	rnd::VecWorldToUser

IN	[v2Size], camC, v3World, [nHalfDist = 1000.0]
OUT	v3User, nLinDepth
--------------------------------------*/
int rnd::VecWorldToUser(lua_State* l)
{
	unsigned width = luaL_optinteger(l, 1, wrp::VideoWidth());
	unsigned height = luaL_optinteger(l, 2, wrp::VideoHeight());
	float linDepth;

	vec::LuaPushVec(l, VecWorldToUser(width, height, *scn::Camera::CheckLuaTo(3),
		vec::LuaToVec(l, 4, 5, 6), &linDepth, luaL_optnumber(l, 7, 1000.0f)));

	lua_pushnumber(l, linDepth);
	return 4;
}

/*
################################################################################################


	SHADOW


################################################################################################
*/

/*--------------------------------------
	rnd::CalculateCascadeDistances
--------------------------------------*/
void rnd::CalculateCascadeDistances(float logFraction)
{
	float dists[3];

	for(size_t i = 0; i < 3; i++)
	{
		float f = (i + 1) * 0.25f;
		float linDist = nearClip.Float() + (farClip.Float() - nearClip.Float()) * f;
		float logDist = nearClip.Float() * pow(farClip.Float() / nearClip.Float(), f);
		dists[i] = COM_LERP(linDist, logDist, logFraction);
	}

	cascadeDist0.SetValue(dists[0]);
	cascadeDist1.SetValue(dists[1]);
	cascadeDist2.SetValue(dists[2]);
	cascadeDist3.SetValue(farClip.Float());
}

/*
################################################################################################


	SHADOW LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::CalculateCascadeDistances

IN	nLogFraction
--------------------------------------*/
int rnd::CalculateCascadeDistances(lua_State* l)
{
	CalculateCascadeDistances(luaL_checknumber(l, 1));
	return 0;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	rnd::UpdateMatrices

Calculates frame-constant client-side matrices. Uniforms still need to be set.
--------------------------------------*/
void rnd::UpdateMatrices()
{
	GLfloat temp[16], temp2[16];
	const scn::Camera& cam = *scn::ActiveCamera();
	com::Qua camOri = cam.FinalOri();
	float camFOV = cam.FinalFOV();
	com::Vec3 camSkewOrg = cam.FinalSkewOrigin();
	com::Vec2 camZSkew = cam.FinalZSkew();

	UpdatePerspectiveMatrices(cam.FinalPos(), camOri, camFOV, camSkewOrg, camZSkew,
		gWorldToView, gViewToClip, gWorldToClip, gClipToFocal);

	for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
	{
		if(const scn::Camera* oc = scn::Overlays()[i].cam)
		{
			UpdatePerspectiveMatrices(oc->FinalPos(), oc->FinalOri(), oc->FinalFOV(),
				oc->FinalSkewOrigin(), oc->FinalZSkew(), temp, temp2, gOverlayWTC[i],
				gOverlayCTF[i]);
		}
		else
		{
			com::Identity4x4(gOverlayWTC[i]);
			com::Identity4x4(gOverlayCTF[i]);
		}
	}

	const scn::Overlay& skyOvr = scn::SkyOverlay();
	com::Vec3 skyPos;
	com::Qua skyOri;

	if(skyOvr.cam)
	{
		const scn::Camera& skyCam = *skyOvr.cam;
		UpdatePerspectiveMatrices(skyCam.FinalPos(), skyCam.FinalOri(), skyCam.FinalFOV(),
			skyCam.FinalSkewOrigin(), skyCam.FinalZSkew(), temp, gSkyVTC, gSkyWTC, temp2);
	}
	else
	{
		UpdatePerspectiveMatrices(0.0f, camOri, camFOV, camSkewOrg, camZSkew, temp, gSkyVTC,
			gSkyWTC, temp2);
	}
}

/*--------------------------------------
	rnd::UpdatePerspectiveMatrices
--------------------------------------*/
void rnd::UpdatePerspectiveMatrices(const com::Vec3& camPos, const com::Qua& camOri, float vfov,
	const com::Vec3& skewOrigin, const com::Vec2& zSkew, GLfloat (&wtv)[16], GLfloat (&vtc)[16],
	GLfloat (&wtc)[16], GLfloat (&ctf)[16])
{
	float vHalfFOV = vfov * 0.5f;
	GLfloat clipNear = nearClip.Float();
	//GLfloat clipFar = farClip.Float();
	bool skew = zSkew != 0.0f;

	/*
		WORLD TO CLIP
	*/

	GLfloat aspect = (GLfloat)wrp::VideoWidth() / (GLfloat)wrp::VideoHeight();

	if(skew)
		WorldToViewSkew(camPos, camOri, skewOrigin, zSkew, wtv);
	else
		WorldToView(camPos, camOri, wtv);

	ViewToClipPers(aspect, vHalfFOV, clipNear, vtc);
	com::Multiply4x4(wtv, vtc, wtc);

	/*
		CLIP TO VIEW
	*/

	// [-1, 1] clip depth with finite far plane
	/*
	GLfloat clipToView[16] = {};
	GLfloat tanVHFOV = tan(vHalfFOV);
	clipToView[3] = 1.0f;
	clipToView[4] = -aspect * tanVHFOV;
	clipToView[9] = tanVHFOV;
	clipToView[14] = 1.0f / vtc[11];
	clipToView[15] = -(clipFar + clipNear) / (-2.0f * clipFar * clipNear);
	*/

	// [1, 0] clip depth with infinite far plane
	GLfloat clipToView[16] = {};
	GLfloat tanVHFOV = tan(vHalfFOV);
	clipToView[3] = 1.0f;
	clipToView[4] = -aspect * tanVHFOV;
	clipToView[9] = tanVHFOV;
	clipToView[14] = 1.0f / vtc[11];

#if 0
	// FIXME TEMP
	GLfloat view[4] = {4.0f, 3.1f, 5.2f, 1.0f};
	GLfloat clip[4];
	com::Multiply4x4Vec4(view, vtc, clip);
	GLfloat back[4];
	com::Multiply4x4Vec4(clip, clipToView, back);
	com::Vec3 viewv(4.0f, 3.1f, 5.2f);
	com::Vec3 clipv = com::Multiply4x4Homogeneous(viewv, vtc);
	com::Vec3 backv = com::Multiply4x4Homogeneous(clipv, clipToView);
	int a = 0;
#endif

	/*
		CLIP TO FOCAL
	*/

	GLfloat viewToFocal[16];

	if(skew)
	{
		/* FIXME: for this to work, light shaders need to do clipToFocal multiply each frag
		instead of calculating a ray in the vertex shader; preliminary test didn't show any
		performance hit, but if it's a problem a separate shader can be compiled for the skewed
		case */
		GLfloat rot[16];
		com::MatQua(camOri, rot);

		GLfloat unskew[16] = {
			1.0f,	0.0f,	-zSkew.x,	zSkew.x * (skewOrigin.z - camPos.z),
			0.0f,	1.0f,	-zSkew.y,	zSkew.y * (skewOrigin.z - camPos.z),
			0.0f,	0.0f,	1.0f,		0.0f,
			0.0f,	0.0f,	0.0f,		1.0f
		};

		com::Multiply4x4(rot, unskew, viewToFocal);
	}
	else
		com::MatQua(camOri, viewToFocal);

	com::Multiply4x4(clipToView, viewToFocal, ctf);
}

/*--------------------------------------
	rnd::WorldToView
--------------------------------------*/
void rnd::WorldToView(const com::Vec3& pos, const com::Qua& ori, GLfloat (&mOut)[16])
{
	com::MatQua(ori.Conjugate(), mOut);

	for(size_t r = 0; r < 3; r++)
	{
		size_t index = r * 4 + 3;
		mOut[index] = 0.0f;

		for(size_t c = 0; c < 3; c++)
			mOut[index] += -pos[c] * mOut[r * 4 + c];
	}
}

/*--------------------------------------
	rnd::WorldToViewSkew
--------------------------------------*/
void rnd::WorldToViewSkew(const com::Vec3& pos, const com::Qua& ori,
	const com::Vec3& skewOrigin, const com::Vec2& zSkew, GLfloat (&mOut)[16])
{
	GLfloat matMoveSkew[16] = {
		1.0f,	0.0f,	zSkew.x,	-pos.x - zSkew.x * skewOrigin.z,
		0.0f,	1.0f,	zSkew.y,	-pos.y - zSkew.y * skewOrigin.z,
		0.0f,	0.0f,	1.0f,		-pos.z,
		0.0f,	0.0f,	0.0f,		1.0f
	};

	GLfloat matRot[16];
	com::MatQua(ori.Conjugate(), matRot);
	com::Multiply4x4(matMoveSkew, matRot, mOut);
}

/*--------------------------------------
	rnd::ViewToWorld
--------------------------------------*/
void rnd::ViewToWorld(const com::Vec3& pos, const com::Qua& ori, GLfloat (&mOut)[16])
{
	com::MatQua(ori, mOut);

	for(size_t i = 0; i < 3; i++)
		mOut[i * 4 + 3] = pos[i];
}

/*--------------------------------------
	rnd::ViewToClipPers
--------------------------------------*/
void rnd::ViewToClipPers(GLfloat aspect, GLfloat vHalfFOV, GLfloat clipNear,
	GLfloat (&mOut)[16])
{
	GLfloat tanVHFOV = tan(vHalfFOV);
	//GLfloat subFN = clipFar - clipNear;

	// Perspective, view X to screen right, Y to up, Z to backward
	/*GLfloat viewToClip[16] = {
		1.0f / (aspect * tanVHFOV),	0.0f,				0.0f,							0.0f,
		0.0f,						1.0f / tanVHFOV,	0.0f,							0.0f,
		0.0f,						0.0f,				-(clipFar + clipNear) / subFN,	(-2.0f * clipFar * clipNear) / subFN,
		0.0f,						0.0f,				-1.0f,							0.0f
	};*/

	// Perspective, view X to screen forward, Y to left, Z to up
	/*
	mOut[0] = 0.0f;							mOut[1] = -1.0f / (aspect * tanVHFOV);	mOut[2] = 0.0f;				mOut[3] = 0.0f;
	mOut[4] = 0.0f;							mOut[5] = 0.0f;							mOut[6] = 1.0f / tanVHFOV;	mOut[7] = 0.0f;
	mOut[8] = (clipFar + clipNear) / subFN;	mOut[9] = 0.0f;							mOut[10] = 0.0f;			mOut[11] = (-2.0f * clipFar * clipNear) / subFN;
	mOut[12] = 1.0f;						mOut[13] = 0.0f;						mOut[14] = 0.0f;			mOut[15] = 0.0f;
	*/

		/* To put clip space z into [0, 1] range instead of [-1, 1]:
			Change mOut[8] to clipFar / subFN
			Change mOut[11] to -(clipFar * clipNear) / subFN */

	// Reverse depth [1, 0] with infinite far plane (approaches 0 forever)
	mOut[0] = 0.0f;		mOut[1] = -1.0f / (aspect * tanVHFOV);	mOut[2] = 0.0f;				mOut[3] = 0.0f;
	mOut[4] = 0.0f;		mOut[5] = 0.0f;							mOut[6] = 1.0f / tanVHFOV;	mOut[7] = 0.0f;
	mOut[8] = 0.0f;		mOut[9] = 0.0f;							mOut[10] = 0.0f;			mOut[11] = clipNear;
	mOut[12] = 1.0f;	mOut[13] = 0.0f;						mOut[14] = 0.0f;			mOut[15] = 0.0f;
}

/*--------------------------------------
	rnd::WorldToClipPers
--------------------------------------*/
void rnd::WorldToClipPers(GLfloat aspect, GLfloat vfov, GLfloat clipNear, const com::Vec3& pos,
	const com::Qua& ori, GLfloat (&mOut)[16])
{
	GLfloat worldToView[16];
	WorldToView(pos, ori, worldToView);
	GLfloat viewToClip[16];
	ViewToClipPers(aspect, vfov * 0.5f, clipNear, viewToClip);
	com::Multiply4x4(worldToView, viewToClip, mOut);
}

/*--------------------------------------
	rnd::ViewToClipPersLimited

FIXME: Reverse depth here so it's [1, 0]; haven't got around to it because a clip space with far
	plane clipping isn't needed right now (but would be if bulb shadows were enabled)
--------------------------------------*/
void rnd::ViewToClipPersLimited(GLfloat aspect, GLfloat vHalfFOV, GLfloat clipNear,
	GLfloat clipFar, GLfloat (&mOut)[16])
{
	GLfloat tanVHFOV = tan(vHalfFOV);
	GLfloat subFN = clipFar - clipNear;

	// Perspective, view X to screen forward, Y to left, Z to up
	// [0, 1] range
	mOut[0] = 0.0f;				mOut[1] = -1.0f / (aspect * tanVHFOV);	mOut[2] = 0.0f;				mOut[3] = 0.0f;
	mOut[4] = 0.0f;				mOut[5] = 0.0f;							mOut[6] = 1.0f / tanVHFOV;	mOut[7] = 0.0f;
	mOut[8] = clipFar / subFN;	mOut[9] = 0.0f;							mOut[10] = 0.0f;			mOut[11] = -(clipFar * clipNear) / subFN;
	mOut[12] = 1.0f;			mOut[13] = 0.0f;						mOut[14] = 0.0f;			mOut[15] = 0.0f;
}

/*--------------------------------------
	rnd::ViewToClipOrth
--------------------------------------*/
void rnd::ViewToClipOrth(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
	GLfloat clipNear, GLfloat clipFar, GLfloat (&mOut)[16])
{
	GLfloat rsl = right - left;
	GLfloat tsb = top - bottom;
	GLfloat fsn = clipFar - clipNear;

	// Orthographic, view X to screen right, Y to up, Z to backward
	/*mOut[0] = 2.0f / rsl;				mOut[1] = 0.0f;						mOut[2] = 0.0f;							mOut[3] = (right + left) / -rsl;
	mOut[4] = 0.0f;						mOut[5] = 2.0f / tsb;				mOut[6] = 0.0f;							mOut[7] = (top + bottom) / -tsb;
	mOut[8] = 0.0f;						mOut[9] = 0.0f;						mOut[10] = -2.0f / fsn;					mOut[11] = (clipFar + clipNear) / -fsn;
	mOut[12] = 0.0f;					mOut[13] = 0.0f;					mOut[14] = 0.0f;						mOut[15] = 1.0f;*/

	// Orthographic, view X to screen forward, Y to left, Z to up
	/*mOut[0] = 0.0f;		mOut[1] = -2.0f / rsl;	mOut[2] = 0.0f;			mOut[3] = (right + left) / -rsl;
	mOut[4] = 0.0f;			mOut[5] = 0.0f;			mOut[6] = 2.0f / tsb;	mOut[7] = (top + bottom) / -tsb;
	mOut[8] = 2.0f / fsn;	mOut[9] = 0.0f;			mOut[10] = 0.0f;		mOut[11] = (clipFar + clipNear) / -fsn;
	mOut[12] = 0.0f;		mOut[13] = 0.0f;		mOut[14] = 0.0f;		mOut[15] = 1.0f;*/

	// Reverse depth [1, 0]
	mOut[0] = 0.0f;			mOut[1] = -2.0f / rsl;	mOut[2] = 0.0f;			mOut[3] = (right + left) / -rsl;
	mOut[4] = 0.0f;			mOut[5] = 0.0f;			mOut[6] = 2.0f / tsb;	mOut[7] = (top + bottom) / -tsb;
	mOut[8] = -1.0f / fsn;	mOut[9] = 0.0f;			mOut[10] = 0.0f;		mOut[11] = 1.0f + clipNear / fsn;
	mOut[12] = 0.0f;		mOut[13] = 0.0f;		mOut[14] = 0.0f;		mOut[15] = 1.0f;
}

/*--------------------------------------
	rnd::WorldToSunView
--------------------------------------*/
void rnd::WorldToSunView(GLfloat pitch, GLfloat yaw, const com::Vec3& pos, GLfloat (&mOut)[16])
{
	/*
	standard rotation matrices:

	y:
	cy	0	sy
	0	1	0
	-sy	0	cy

	z:
	cz	-sz	0
	sz	cz	0
	0	0	1

	transposed:

	y:
	cy	0	-sy
	0	1	0
	sy	0	cy

	z:
	cz	sz	0
	-sz	cz	0
	0	0	1

	zy (column-dot-row):
	czcy	szcy	-sy
	-sz		cz		0
	czsy	szsy	cy
	*/

	GLfloat sy = sin(pitch), cy = cos(pitch);
	GLfloat sz = sin(yaw), cz = cos(yaw);
	mOut[0] = cz * cy;
	mOut[1] = sz * cy;
	mOut[2] = -sy;
	mOut[3] = pos.x * mOut[0] + pos.y * mOut[1] + pos.z * mOut[2];
	mOut[4] = -sz;
	mOut[5] = cz;
	mOut[6] = 0.0f;
	mOut[7] = pos.x * mOut[4] + pos.y * mOut[5];
	mOut[8] = cz * sy;
	mOut[9] = sz * sy;
	mOut[10] = cy;
	mOut[11] = pos.x * mOut[8] + pos.y * mOut[9] + pos.z * mOut[10];
	mOut[12] = 0.0f;
	mOut[13] = 0.0f;
	mOut[14] = 0.0f;
	mOut[15] = 1.0f;
}

/*--------------------------------------
	rnd::InitProgram

If vertSrcs are given, creates a shader and sets vert to its name. This is done with fragSrcs
and frag, too.

Creates a program using vert and frag and sets prog to its name.

If attributes is not 0, binds attribute indicated by identifier to the location indicated by
index. The last element should be a sentinel with all values set to 0.

A similar thing is done with the uniforms array; name is set to the location of uniform
indicated by identifier. If name is 0 but identifier is not, the uniform is skipped.

Returns false if OpenGL failed to create or link shaders. The inability to find a uniform is
reported but is not considered a failure. Errors are written to the log and tagged with title.
--------------------------------------*/
bool rnd::InitProgram(const char* title, GLuint& prog, GLuint& vert,
	const char* const* vertSrcs, size_t numVertSrcs, GLuint& frag, const char* const* fragSrcs,
	size_t numFragSrcs, const prog_attribute* attributes, prog_uniform* uniforms)
{
	if(vertSrcs && numVertSrcs > 0 &&
	!CreateShader(title, GL_VERTEX_SHADER, vertSrcs, numVertSrcs, vert))
		return false;

	if(fragSrcs && numFragSrcs > 0 &&
	!CreateShader(title, GL_FRAGMENT_SHADER, fragSrcs, numFragSrcs, frag))
		return false;

	CreateProgram(vert, frag, prog);

	for(const prog_attribute* a = attributes; a->identifier; a++)
		glBindAttribLocation(prog, a->index, a->identifier);

	if(!LinkProgram(prog))
		return false;

	if(uniforms)
	{
		for(prog_uniform* u = uniforms; u->identifier; u++)
		{
			if(!u->name)
				continue;

			if((*u->name = glGetUniformLocation(prog, u->identifier)) == -1)
			{
				if(!u->silent)
					con::LogF("No uniform '%s' in shader '%s'", u->identifier, title);
			}
		}
	}

	return true;
}

/*--------------------------------------
	rnd::CreateShader

Creates and compiles a shader with error log output to the console. type is either
GL_VERTEX_SHADER or GL_FRAGMENT_SHADER. name is an uninitialized GLuint.
--------------------------------------*/
bool rnd::CreateShader(const char* title, GLenum type, const char* const* sources,
	size_t numSources, GLuint& name)
{
	if(type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)
		return false;

	name = glCreateShader(type);
	glShaderSource(name, numSources, sources, 0);
	glCompileShader(name);

	GLint status;
	glGetShaderiv(name, GL_COMPILE_STATUS, &status);

	if(status == GL_FALSE)
	{
		con::LogF("'%s' %s shader compile failure", title,
			type == GL_VERTEX_SHADER ? "vertex" : "fragment");

		GLint logLength;
		glGetShaderiv(name, GL_INFO_LOG_LENGTH, &logLength);

		if(!logLength)
		{
			glDeleteShader(name);
			name = 0;
			return false;
		}

		char* log = new char[logLength];
		glGetShaderInfoLog(name, logLength, 0, log);

		con::LogF(log);
		delete[] log;

		glDeleteShader(name);
		name = 0;
		return false;
	}

	return true;
}

/*--------------------------------------
	rnd::CreateProgram

Creates a program and attaches shaders.
--------------------------------------*/
void rnd::CreateProgram(GLuint vertName, GLuint fragName, GLuint& name)
{
	name = glCreateProgram();
	glAttachShader(name, vertName);
	glAttachShader(name, fragName);
}

/*--------------------------------------
	rnd::LinkProgram

Links a program with error log output to the console.
Separate from Rnd_CreateProgram() because attribute
locations must be bound in between.

FIXME: call glDeleteShaders so they're deleted when the program is deleted (but make sure to
	convey the shaders have been "consumed" once this is called)
--------------------------------------*/
bool rnd::LinkProgram(GLuint name)
{
	glLinkProgram(name);

	GLint status;
	glGetProgramiv(name, GL_LINK_STATUS, &status);

	if(status == GL_FALSE)
	{
		con::LogF("Progam Link Failure");

		GLint logLength;
		glGetProgramiv(name, GL_INFO_LOG_LENGTH, &logLength);
		if(!logLength)
		{
			glDeleteProgram(name);
			return false;
		}

		char* log = new char[logLength];
		glGetProgramInfoLog(name, logLength, 0, log);

		con::LogF(log);
		delete[] log;

		glDeleteProgram(name);
		return false;
	}

	return true;
}

/*--------------------------------------
	rnd::CopyFrameBuffer

Binds texture to RND_VARIABLE_TEXTURE_UNIT and copies the frame buffer to it. Whether the color
buffer or depth buffer is copied depends on the texture's settings.
--------------------------------------*/
void rnd::CopyFrameBuffer(GLuint texture)
{
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, wrp::VideoWidth(), wrp::VideoHeight());
}

/*--------------------------------------
	rnd::MakeDitherArray
--------------------------------------*/
void rnd::MakeDitherArray(GLfloat (&dither)[16], GLfloat divisorFactor)
{
	GLfloat divisor = divisorFactor * 16.0f;

	dither[0] = 0.0f;			dither[1] = 8.0f/divisor;	dither[2] = 2.0f/divisor;	dither[3] = 10.0f/divisor;
	dither[4] = 12.0f/divisor;	dither[5] = 4.0f/divisor;	dither[6] = 14.0f/divisor;	dither[7] = 6.0f/divisor;
	dither[8] = 3.0f/divisor;	dither[9] = 11.0f/divisor;	dither[10] = 1.0f/divisor;	dither[11] = 9.0f/divisor;
	dither[12] = 15.0f/divisor;	dither[13] = 7.0f/divisor;	dither[14] = 13.0f/divisor;	dither[15] = 5.0f/divisor;
}

/*--------------------------------------
	rnd::MakeSignedDitherArray

[-0.46875, 0.46875]
--------------------------------------*/
void rnd::MakeSignedDitherArray(GLfloat (&dither)[16], GLfloat divisorFactor)
{
	GLfloat divisor = divisorFactor * 16.0f;

	dither[0] = -7.5f/divisor;	dither[1] = 0.5f/divisor;	dither[2] = -5.5f/divisor;	dither[3] = 2.5f/divisor;
	dither[4] = 4.5f/divisor;	dither[5] = -3.5f/divisor;	dither[6] = 6.5f/divisor;	dither[7] = -1.5f/divisor;
	dither[8] = -4.5f/divisor;	dither[9] = 3.5f/divisor;	dither[10] = -6.5f/divisor;	dither[11] = 1.5f/divisor;
	dither[12] = 7.5f/divisor;	dither[13] = -0.5f/divisor;	dither[14] = 5.5f/divisor;	dither[15] = -2.5f/divisor;
}

/*--------------------------------------
	rnd::MakeBilinearDitherArray

FIXME: remove
--------------------------------------*/
void rnd::MakeBilinearDitherArray(GLfloat (&dither)[32])
{
	const GLfloat DIV = 16.0f;

	// FIXME: placeholder equal to regular dither pattern; remove this function if this pattern stays for dithered bilinear
	// x
	dither[0] = 0.0f;		dither[2] = 8.0f/DIV;	dither[4] = 2.0f/DIV;	dither[6] = 10.0f/DIV;
	dither[8] = 12.0f/DIV;	dither[10] = 4.0f/DIV;	dither[12] = 14.0f/DIV;	dither[14] = 6.0f/DIV;
	dither[16] = 3.0f/DIV;	dither[18] = 11.0f/DIV;	dither[20] = 1.0f/DIV;	dither[22] = 9.0f/DIV;
	dither[24] = 15.0f/DIV;	dither[26] = 7.0f/DIV;	dither[28] = 13.0f/DIV;	dither[30] = 5.0f/DIV;

	// y
	dither[1] = 0.0f;		dither[3] = 8.0f/DIV;	dither[5] = 2.0f/DIV;	dither[7] = 10.0f/DIV;
	dither[9] = 12.0f/DIV;	dither[11] = 4.0f/DIV;	dither[13] = 14.0f/DIV;	dither[15] = 6.0f/DIV;
	dither[17] = 3.0f/DIV;	dither[19] = 11.0f/DIV;	dither[21] = 1.0f/DIV;	dither[23] = 9.0f/DIV;
	dither[25] = 15.0f/DIV;	dither[27] = 7.0f/DIV;	dither[29] = 13.0f/DIV;	dither[31] = 5.0f/DIV;

#if 0
	// Unreal style

	/*
	dither[0] = -0.25f;	dither[2] = -0.5f;	dither[4] = 0.25f;	dither[6] = 0.5f;
	dither[8] = -0.75f;	dither[10] = 0.0f;	dither[12] = 0.75f;	dither[14] = 0.0f;
	dither[16] = -0.25f;	dither[18] = -0.5f;	dither[20] = 0.25f;	dither[22] = 0.5f;
	dither[24] = -0.75f;	dither[26] = 0.0f;	dither[28] = 0.75f;	dither[30] = 0.0f;
	*/

	/*
	dither[1] = 0.0f;	dither[3] = 0.75f;	dither[5] = 0.0f;	dither[7] = 0.75f;
	dither[9] = 0.5f;	dither[11] = 0.25f;	dither[13] = 0.5f;	dither[15] = 0.25f;
	dither[17] = 0.0f;	dither[19] = -0.75f;	dither[21] = 0.0f;	dither[23] = -0.75f;
	dither[25] = -0.5f;	dither[27] = -0.25f;	dither[29] = -0.5f;	dither[31] = -0.25f;
	*/

	dither[0] = -2.0f/5.0f;	dither[2] = -3.0f/5.0f;	dither[4] = 2.0f/5.0f;	dither[6] = 3.0f/5.0f;
	dither[8] = -4.0f/5.0f;	dither[10] = -1.0f/5.0f;	dither[12] = 4.0f/5.0f;	dither[14] = 1.0f/5.0f;
	dither[16] = -2.0f/5.0f;	dither[18] = -3.0f/5.0f;	dither[20] = 2.0f/5.0f;	dither[22] = 3.0f/5.0f;
	dither[24] = -4.0f/5.0f;	dither[26] = -1.0f/5.0f;	dither[28] = 4.0f/5.0f;	dither[30] = 1.0f/5.0f;

	dither[1] = 2.0f/5.0f;	dither[3] = 3.0f/5.0f;	dither[5] = 2.0f/5.0f;	dither[7] = 3.0f/5.0f;
	dither[9] = 4.0f/5.0f;	dither[11] = 1.0f/5.0f;	dither[13] = 4.0f/5.0f;	dither[15] = 1.0f/5.0f;
	dither[17] = -2.0f/5.0f;	dither[19] = -3.0f/5.0f;	dither[21] = -2.0f/5.0f;	dither[23] = -3.0f/5.0f;
	dither[25] = -4.0f/5.0f;	dither[27] = -1.0f/5.0f;	dither[29] = -4.0f/5.0f;	dither[31] = -1.0f/5.0f;

	for(size_t i = 0; i < 32; i++)
		dither[i] *= 0.5f;
#endif

#if 0
	const GLfloat DIV = 16.0f;

	// x
	dither[0] = 0.0f;			dither[2] = -8.0f/DIV;	dither[4] = -2.0f/DIV;	dither[6] = 10.0f/DIV;
	dither[8] = -12.0f/DIV;	dither[10] = 4.0f/DIV;	dither[12] = 14.0f/DIV;	dither[14] = -6.0f/DIV;
	dither[16] = -3.0f/DIV;	dither[18] = 11.0f/DIV;	dither[20] = 1.0f/DIV;	dither[22] = -9.0f/DIV;
	dither[24] = 15.0f/DIV;	dither[26] = -7.0f/DIV;	dither[28] = -13.0f/DIV;	dither[30] = 5.0f/DIV;

	// y
	dither[1] = 0.0f;			dither[3] = -8.0f/DIV;	dither[5] = -2.0f/DIV;	dither[7] = 10.0f/DIV;
	dither[9] = -12.0f/DIV;	dither[11] = 4.0f/DIV;	dither[13] = 14.0f/DIV;	dither[15] = -6.0f/DIV;
	dither[17] = 3.0f/DIV;	dither[19] = -11.0f/DIV;	dither[21] = -1.0f/DIV;	dither[23] = 9.0f/DIV;
	dither[25] = -15.0f/DIV;	dither[27] = 7.0f/DIV;	dither[29] = 13.0f/DIV;	dither[31] = -5.0f/DIV;

	/*
	// y
	dither[1] = abs(dither[30]);	dither[3] = abs(dither[28]);	dither[5] = abs(dither[26]);	dither[7] = abs(dither[24]);
	dither[9] = -abs(dither[22]);	dither[11] = -abs(dither[20]);	dither[13] = -abs(dither[18]);	dither[15] = -abs(dither[16]);
	dither[17] = abs(dither[14]);	dither[19] = abs(dither[12]);	dither[21] = abs(dither[10]);	dither[23] = abs(dither[8]);
	dither[25] = -abs(dither[6]);	dither[27] = -abs(dither[4]);	dither[29] = -abs(dither[2]);	dither[31] = -abs(dither[0]);
	*/

	/*
	// [-0.5, 0.5]
	dither[0] = -8.0f/DIV;	dither[2] = 0.0f;		dither[4] = -6.0f/DIV;	dither[6] = 2.0f/DIV;
	dither[8] = 4.0f/DIV;	dither[10] = -4.0f/DIV;	dither[12] = 6.0f/DIV;	dither[14] = -2.0f/DIV;
	dither[16] = -5.0f/DIV;	dither[18] = 3.0f/DIV;	dither[20] = -7.0f/DIV;	dither[22] = 1.0f/DIV;
	dither[24] = 7.0f/DIV;	dither[26] = -1.0f/DIV;	dither[28] = 5.0f/DIV;	dither[30] = -3.0f/DIV;
	*/

	/*
	// y
	dither[1] = dither[30];	dither[3] = dither[28];		dither[5] = dither[26];	dither[7] = dither[24];
	dither[9] = dither[22];	dither[11] = dither[20];	dither[13] = dither[18];	dither[15] = dither[16];
	dither[17] = dither[14];	dither[19] = dither[12];	dither[21] = dither[10];	dither[23] = dither[8];
	dither[25] = dither[6];	dither[27] = dither[4];	dither[29] = dither[2];	dither[31] = dither[0];
	*/

	for(size_t i = 0; i < 32; i++)
		dither[i] *= 0.5f;
#endif
}

/*--------------------------------------
	rnd::GetErrorString
--------------------------------------*/
const char* rnd::GetErrorString(GLenum err)
{
	switch(err)
	{
	case GL_NO_ERROR:
		return "GL_NO_ERROR";
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	case GL_STACK_OVERFLOW:
		return "GL_STACK_OVERFLOW";
	case GL_STACK_UNDERFLOW:
		return "GL_STACK_UNDERFLOW";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_CONTEXT_LOST:
		return "GL_CONTEXT_LOST";
	case GL_TABLE_TOO_LARGE:
		return "GL_TABLE_TOO_LARGE";
	default:
		return "Unknown error";
	}
}

/*--------------------------------------
	rnd::GetFramebufferStatusString
--------------------------------------*/
const char* rnd::GetFramebufferStatusString(GLenum stat)
{
	switch(stat)
	{
	case GL_FRAMEBUFFER_COMPLETE:
		return "GL_FRAMEBUFFER_COMPLETE";
	case GL_FRAMEBUFFER_UNDEFINED:
		return "GL_FRAMEBUFFER_UNDEFINED";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
	case GL_FRAMEBUFFER_UNSUPPORTED:
		return "GL_FRAMEBUFFER_UNSUPPORTED";
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
	case 0:
		return "glCheckFramebufferStatus error";
	default:
		return "Unknown framebuffer status";
	}
}

/*--------------------------------------
	rnd::StencilLimit
--------------------------------------*/
GLint rnd::StencilLimit()
{
	return 1 << COM_MIN(stencilBits, sizeof(GLint) * CHAR_BIT - 1);
}

/*--------------------------------------
	rnd::UpdateShadowBuffer
--------------------------------------*/
void rnd::UpdateShadowBuffer()
{
	GLsizei actual = shadowRes.Float();
	GLsizei actualCascade = sunShadowRes.Float();

	if(!usingFBOs.Bool())
	{
		unsigned vidLimit = com::HighestSetBit(COM_MIN(wrp::VideoWidth(), wrp::VideoHeight()));
		actualCascade = COM_MIN(actualCascade, (GLsizei)vidLimit);
		actual = COM_MIN(actual, (GLsizei)vidLimit);
	}

	actual = COM_MIN(actual, actualCascade * 2); // Fit spot-light shadow maps in sun's buffer

	if(allocShadowRes == actual && allocCascadeRes == actualCascade)
		return;

	allocShadowRes = actual;
	allocCascadeRes = actualCascade;
	con::LogF("Updated shadow resolution: %d, sun %d", (int)allocShadowRes,
		(int)allocCascadeRes);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texShadowBuffer);

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
		glTexImage2D(TARGETS[i], 0, GL_DEPTH_COMPONENT, allocShadowRes, allocShadowRes, 0,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glBindTexture(GL_TEXTURE_2D, texFlatShadowBuffer);
	GLsizei allocAtlasRes = allocCascadeRes * 2; // 4 cascades
		// FIXME: it shouldn't be a square, cascades should be packed sequentially so adding more is easy
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, allocAtlasRes, allocAtlasRes, 0,
		GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);

	if(usingFBOs.Bool())
	{
		if(fboFlatShadowBuffer)
			glDeleteFramebuffers(1, &fboFlatShadowBuffer);

		glGenFramebuffers(1, &fboFlatShadowBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboFlatShadowBuffer);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
			texFlatShadowBuffer, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

/*--------------------------------------
	rnd::RandomSeed
--------------------------------------*/
GLfloat rnd::RandomSeed()
{
	return 1.0f + (rand() % 100000) * 1e-05f;
}

/*--------------------------------------
	rnd::UpdateNoise
--------------------------------------*/
void rnd::UpdateNoise()
{
	static float noiseAccumTime = 0.0f;
	static unsigned oldTime = 0;
	unsigned time = wrp::Time();
	noiseAccumTime += time - oldTime;
	oldTime = time;

	if(noiseAccumTime > noiseTime.Float())
	{
		shaderRandomSeed = RandomSeed();
		shaderRandomSeed2 = RandomSeed();
		noiseAccumTime -= noiseTime.Float();

		if(noiseAccumTime > noiseTime.Float())
			noiseAccumTime = 0.0f;
	}
}

/*--------------------------------------
	rnd::EnsureShaderPrograms
--------------------------------------*/
void rnd::EnsureShaderPrograms()
{
	EnsureWorldProgram();
	EnsureModelProgram();
	EnsureSkySphereProgram();
	EnsureGlassRecolorProgram();
	EnsureAntiAliasingProgram();
}

/*--------------------------------------
	rnd::GenerateScreenQuad
--------------------------------------*/
void rnd::GenerateScreenQuad(GLuint& bufferOut, float z)
{
	glGenBuffers(1, &bufferOut);
	glBindBuffer(GL_ARRAY_BUFFER, bufferOut);

	screen_vertex verts[4] = {
		{-1.0f, -1.0f, z},
		{1.0f, -1.0f, z},
		{1.0f, 1.0f, z},
		{-1.0f, 1.0f, z}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertex) * 4, verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*--------------------------------------
	rnd::GenerateScreenTri
--------------------------------------*/
void rnd::GenerateScreenTri(GLuint& bufferOut, float z)
{
	glGenBuffers(1, &bufferOut);
	glBindBuffer(GL_ARRAY_BUFFER, bufferOut);

	screen_vertex verts[3] = {
		{-1.0f, -1.0f, z},
		{3.0f, -1.0f, z},
		{-1.0f, 3.0f, z}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertex) * 3, verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*--------------------------------------
	rnd::NormalizedIntensity
--------------------------------------*/
float rnd::NormalizedIntensity(float intensity)
{
	return intensity / CurrentMaxIntensity();
}

/*--------------------------------------
	rnd::CurrentMaxIntensity
--------------------------------------*/
float rnd::CurrentMaxIntensity()
{
	return light16.Bool() ? maxIntensity.Float() : maxIntensityLow.Float();
}

/*--------------------------------------
	rnd::RampVariance
--------------------------------------*/
float rnd::RampVariance()
{
	return com::Max(0.0f, -rampAdd.Float()) + ditherAmount.Float();
}

/*--------------------------------------
	rnd::UpdateViewport

Calls glViewport and (re)creates frame textures and FBOs. Sets usingFBOs and light16.
GL_TEXTURE0 is used and the texture is unbound at the end.
--------------------------------------*/
void rnd::UpdateViewport()
{
	GLsizei width = wrp::VideoWidth(), height = wrp::VideoHeight();
	glViewport(0, 0, width, height);
	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// FIXME: this and its FBO don't seem to be used, remove
	glBindTexture(GL_TEXTURE_2D, texCompositionBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	bool tryingFBOs = extensions.fbo && !forceNoFBOs.Bool();

	highDepthBuffer.ForceValue(false);
	light16.ForceValue(false);
	GLint internalFormat = (forceLowLightBuffer.Bool() || !tryingFBOs) ? GL_RGBA : GL_RGBA16;
	glBindTexture(GL_TEXTURE_2D, texLightBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_FLOAT, 0);

	if(tryingFBOs)
	{
		usingFBOs.ForceValue(true);

		if(extensions.depthBufferFloat && tryingFBOs && !forceLowDepthBuffer.Bool())
		{
			highDepthBuffer.ForceValue(true);

			glBindTexture(GL_TEXTURE_2D, texDepthBuffer);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, width, height, 0,
				GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0);

			glBindTexture(GL_TEXTURE_2D, texExtraDepthBuffer);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0,
				GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, texDepthBuffer);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0,
				GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);

			glBindTexture(GL_TEXTURE_2D, texExtraDepthBuffer);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
				GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		// Geometry FBO
		if(fboGeometryBuffer)
			glDeleteFramebuffers(1, &fboGeometryBuffer);

		glGenFramebuffers(1, &fboGeometryBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, fboGeometryBuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			texGeometryBuffer, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
			texDepthBuffer, 0);

		GLenum geoStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		// Light FBO
		if(fboLightBuffer)
			glDeleteFramebuffers(1, &fboLightBuffer);

		glGenFramebuffers(1, &fboLightBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, fboLightBuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			texLightBuffer, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
			texDepthBuffer, 0); // Tied to same depth-stencil texture image

		GLenum lightStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		
		// Composition FBO
		if(fboCompositionBuffer)
			glDeleteFramebuffers(1, &fboCompositionBuffer);

		glGenFramebuffers(1, &fboCompositionBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, fboCompositionBuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
			texCompositionBuffer, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
			texDepthBuffer, 0);

		GLenum compStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if(geoStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			con::AlertF("Incomplete geometry buffer fbo: %s (%u)",
				GetFramebufferStatusString(geoStatus), (unsigned)geoStatus);

			usingFBOs.ForceValue(false); // Revert to rendering without FBOs
		}
		else if(lightStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			con::AlertF("Incomplete light buffer fbo: %s (%u)",
				GetFramebufferStatusString(lightStatus), (unsigned)lightStatus);

			usingFBOs.ForceValue(false);
		}
		else if(compStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			con::AlertF("Incomplete composition buffer fbo: %s (%u)",
				GetFramebufferStatusString(compStatus), (unsigned)compStatus);

			usingFBOs.ForceValue(false);
		}
		else
		{
			if(highDepthBuffer.Bool())
			{
				// Check if depth buffer is 32-bit floating point
				GLfloat testValues[4] = {FLT_MIN};

				for(size_t i = 1; i < 4; i++)
					testValues[i] = testValues[i - 1] + testValues[i - 1] * FLT_EPSILON;

				glBindFramebuffer(GL_FRAMEBUFFER, fboGeometryBuffer);
				glBindTexture(GL_TEXTURE_2D, texDepthBuffer);

				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
					testValues);

				GLfloat pixels[4] = {0.0f};
				glReadPixels(0, 0, 4, 1, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);

				for(size_t i = 0; i < sizeof(testValues) / sizeof(GLfloat) &&
				highDepthBuffer.Bool(); i++)
					highDepthBuffer.ForceValue(pixels[i] == testValues[i]);

				if(!highDepthBuffer.Bool())
					con::LogF("Could not get 32-bit floating point depth buffer");
			}

			if(internalFormat == GL_RGBA16)
			{
				// Check if light buffer is 16-bit
				const GLshort TEST_VALUES[4] = {
					32767, 32766, 32765, 32764
				};

				glBindFramebuffer(GL_FRAMEBUFFER, fboLightBuffer);
				glBindTexture(GL_TEXTURE_2D, texLightBuffer);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_SHORT, TEST_VALUES);
				GLshort pixel[4] = {0};
				glReadPixels(0, 0, 1, 1, GL_RGBA, GL_SHORT, pixel);
				light16.ForceValue(true);

				for(size_t i = 0; i < sizeof(TEST_VALUES) / sizeof(GLshort) && light16.Bool();
				i++)
					light16.ForceValue(pixel[i] == TEST_VALUES[i]);

				if(!light16.Bool())
					con::LogF("Could not get 16-bit light buffer");
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else
		usingFBOs.ForceValue(false);

	if(!usingFBOs.Bool())
	{
		// FIXME: deallocate if not usingFBOs?
		glBindTexture(GL_TEXTURE_2D, texDepthBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT,
			GL_FLOAT, 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	// Reset palette since light16 may have changed
	if(CurrentPalette())
		SetPalette(*CurrentPalette(), true);

	// Recalc curve texture since light16 may have changed
	CalculateCurveTexture();
}

/*--------------------------------------
	rnd::Frame

Queues up commands to draw game state. Called in main loop. SwapBuffers must be called after.
fraction [0, 1] is used to lerp between the old and new tick state.
--------------------------------------*/
void rnd::Frame()
{
	if(!CurrentPalette())
		WRP_FATAL("Rendering frame without a palette");

	EnsureShaderPrograms();
	glClearDepth(0.0);
	CheckCurveUpdates();

	if(curveColor.Integer() >= 0)
		DrawCurve(curveColor.Integer());

	doTiming = extensions.timer && timeRender.Bool();

	if(doTiming)
	{
		for(size_t i = 0; i < NUM_TIMERS; i++)
			timers[i].Reset();
	}

	if(scn::ActiveCamera())
	{
		timers[TIMER_MISC].Start();

		if(usingFBOs.Bool())
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboGeometryBuffer);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		UpdateNoise();
		UpdateShadowBuffer();
		UpdateMatrices();
		FloodSunVisible();
		unsigned zoneVisCode;
		FloodVisible(zoneVisCode);
		bool drawSun = TrimVisibleSunBranches(zoneVisCode);

		timers[TIMER_MISC].Stop();

		// Draw geometry & depth buffers
		OverlayPass(); /* FIXME: overlays become black if model prog is reset and ModelPass
					   doesn't draw anything (no visible entities). I think it's because
					   frame-constant uniforms aren't set; need to store state and check whether
					   they've been updated at the beginning of Frame */
		WorldPass();
		ModelPass();

		if(drawSun)
			SkyPass();

		timers[TIMER_MISC].Start();

		// FIXME
		// FIXME
		// FIXME
		// FIXME
		// FIXME
		// FIXME: glass and bulbs are broken without FBOs
		// FIXME
		// FIXME
		// FIXME
		// FIXME
		// FIXME

		if(usingFBOs.Bool())
		{
			// Save depth into default buffer so cloud composition can test against it later
				// FIXME: Disabled for now; cloud composition needs to become generalized alpha drawing
			// FIXME: If not using FBOs, restore depth before cloud composition
			/*glBindFramebuffer(GL_READ_FRAMEBUFFER, fboGeometryBuffer);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			glBlitFramebuffer(0, 0, wrp::VideoWidth(), wrp::VideoHeight(), 0, 0,
				wrp::VideoWidth(), wrp::VideoHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);*/

			/* FIXME: Alternate version for floating point depth buffer since it can't be blit'd
			to default uint buffer. Precision loss may cause z-fighting on clouds, so this isn't
			a good solution */
			/*
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
			glBindTexture(GL_TEXTURE_2D, texDepthBuffer);
			RestoreDepthBuffer();
			*/

			// Save depth buffer for composition now so recolor glass doesn't block fog
			/* FIXME: Make an option to sort colored glass entities CPU side instead of
			allocating another depth buffer? Would probably only save VRAM, not processing */
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fboGeometryBuffer);
			CopyFrameBuffer(texExtraDepthBuffer); // FIXME: make an FBO and blit instead?
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

			// Now drawing to light buffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer);
		}
		else
		{
			CopyFrameBuffer(texGeometryBuffer);
			CopyFrameBuffer(texDepthBuffer);
		}

		// Draw light buffer
		glClear(GL_COLOR_BUFFER_BIT);

		timers[TIMER_MISC].Stop();

		if(drawSun)
			SkyLightPass();

		LightPass(drawSun);

		WorldGlassPass(); // FIXME: both of these bind and unbind the light framebuffer, just do that here?
		GlassPass();

		timers[TIMER_MISC].Start();

		if(!usingFBOs.Bool())
			CopyFrameBuffer(texLightBuffer);

		timers[TIMER_MISC].Stop();

		CompositionPass();
	}
	else
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	// FIXME: Option to do anti-aliasing before or after drawing HUD
	AntiAliasingPass();
	ImagePass();
	LinePass();
	AdvanceLines();

	if(doTiming)
	{
		GLuint total = 0;

		for(size_t i = 0; i < NUM_TIMERS; i++)
		{
			GLuint result = timers[i].Accumulated();
			total += result;
			con::LogF("%s: %g ms", TIMER_NAMES[i], result * 1e-06f);
		}

		con::LogF("frame: %g ms\n", total * 1e-06f);
	}

	if(checkErrors.Bool())
	{
		while(GLenum err = glGetError())
			con::AlertF("GL error: %s (%u)", GetErrorString(err), (unsigned)err);
	}
}

/*--------------------------------------
	rnd::PreTick
--------------------------------------*/
void rnd::PreTick()
{
}

/*--------------------------------------
	rnd::DeleteUnused
--------------------------------------*/
void rnd::DeleteUnused()
{
	for(com::linker<Palette>* it = Palette::List().f, *next; it; it = next)
	{
		next = it->next;

		if(!it->o->Used())
			delete (PaletteGL*)it->o;
	}

	for(com::linker<Texture>* it = Texture::List().f, *next; it; it = next)
	{
		next = it->next;

		if(!it->o->Used())
			delete (TextureGL*)it->o;
	}

	for(com::linker<Mesh>* it = Mesh::List().f, *next; it; it = next)
	{
		next = it->next;

		if(!it->o->Used())
			delete (MeshGL*)it->o;
	}
}

/*--------------------------------------
	rnd::Init
--------------------------------------*/
void rnd::Init()
{
	// OpenGL
	const char* version = (const char*)glGetString(GL_VERSION);
	con::LogF("OpenGL %s", version);

	GLint numFragTextureUnits, numCombinedTextureUnits, numVertexAttribs;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numFragTextureUnits);
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numCombinedTextureUnits);
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribs);
	con::LogF("Fragment shader texture units: " COM_IMAX_PRNT, (intmax_t)numFragTextureUnits);
	con::LogF("Combined texture units: " COM_IMAX_PRNT, (intmax_t)numCombinedTextureUnits);
	con::LogF("Vertex attributes: " COM_IMAX_PRNT, (intmax_t)numVertexAttribs);

	if(numFragTextureUnits < RND_MIN_NUM_FRAG_TEXTURE_UNITS)
		WRP_FATAL("Not enough fragment shader texture units");

	if(numCombinedTextureUnits < RND_MIN_NUM_COMBINED_TEXTURE_UNITS)
		WRP_FATAL("Not enough combined texture units");

	if(numVertexAttribs < RND_MIN_NUM_VERTEX_ATTRIBS)
		WRP_FATAL("Not enough vertex attributes");

	glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
	con::LogF("Stencil bit planes: " COM_IMAX_PRNT, (intmax_t)stencilBits);

	// FIXME: this checks the primary framebuffer's stencil bits, but that's irrelevant if we're only using FBOs
	// FIXME: vars like skyBit and overlayStartBit are based on stencilBits which might not equal the FBO's stencil bits
	if(stencilBits < (GLint)RND_MIN_STENCIL_BITS)
		WRP_FATAL("Not enough stencil bit planes");

	// FIXME: use EXT fbos if ARB or core are unavailable
	extensions.fbo = HaveGLCore(3, 0) || HaveGLExtension("GL_ARB_framebuffer_object");
	extensions.depthBufferFloat = HaveGLExtension("GL_ARB_depth_buffer_float");
	extensions.clipControl = HaveGLCore(4, 5) || HaveGLExtension("GL_ARB_clip_control");
	extensions.timer = HaveGLCore(3, 3) || HaveGLExtension("GL_ARB_timer_query");

	skyBit = stencilBits - 1;
	skyMask = 1 << skyBit;
	overlayStartBit = stencilBits - 3;
	overlayMask = 0;

	for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
		overlayMask |= 1 << (overlayStartBit - i);

	cloudBit = overlayStartBit - scn::NUM_OVERLAYS;
	cloudMask = 1 << cloudBit;

	if(extensions.clipControl)
	{
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		preciseClipRange.ForceValue(true);
	}

	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // FIXME: color option
	glClearStencil(0);

	GenerateScreenTri(nearScreenVertexBuffer, 1.0f);
	GenerateScreenTri(farScreenVertexBuffer, preciseClipRange.Bool() ? 0.0f : -1.0f);

	SetShaderDefines();

	glActiveTexture(GL_TEXTURE0);

	glGenTextures(1, &texGeometryBuffer);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &texDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, texDepthBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &texExtraDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, texExtraDepthBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &texLightBuffer);
	glBindTexture(GL_TEXTURE_2D, texLightBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// FIXME: disable when bulb shadows are disabled
	glGenTextures(1, &texShadowBuffer);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texShadowBuffer);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);

	glGenTextures(1, &texFlatShadowBuffer);
	glBindTexture(GL_TEXTURE_2D, texFlatShadowBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);

	glGenTextures(1, &texCompositionBuffer);
	glBindTexture(GL_TEXTURE_2D, texCompositionBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glActiveTexture(RND_CURVE_TEXTURE_UNIT);
	glGenTextures(1, &texCurve);
	glBindTexture(GL_TEXTURE_1D, texCurve);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE0);

	if(extensions.fbo)
		glGenFramebuffers(1, &fboShadowBuffer);

	if(extensions.timer)
	{
		for(size_t i = 0; i < NUM_TIMERS; i++)
			timers[i].Init();
	}

	lineWidth.SetValue(lineWidth.Float());
	ClearCurvePoints();

	if(!InitWorldPass())
		WRP_FATAL("World pass initialization failed");

	if(!InitModelPass())
		WRP_FATAL("Model pass initialization failed");

	if(!InitSkyPass())
		WRP_FATAL("Sky pass initialization failed");

	if(!InitSkyLightPass())
		WRP_FATAL("Sky light pass initialization failed");

	if(!InitLightPass())
		WRP_FATAL("Light pass initialization failed");

	if(!InitWorldGlassPass())
		WRP_FATAL("World glass pass initialization failed");

	if(!InitGlassPass())
		WRP_FATAL("Glass pass initialization failed");

	if(!InitCompositionPass())
		WRP_FATAL("Composition pass initialization failed");

	if(!InitImagePass())
		WRP_FATAL("Image pass initialization failed");

	if(!InitRestorePass())
		WRP_FATAL("Restore pass initialization failed");

	if(!InitLinePass())
		con::LogF("Line pass initialization failed");

	// Lua
	luaL_Reg regs[] =
	{
		{"FindPalette", FindPalette},
		{"EnsurePalette", EnsurePalette},
		{"CurrentPalette", CurrentPalette},
		{"SetPalette", SetPalette},
		{"FindTexture", FindTexture},
		{"EnsureTexture", EnsureTexture},
		{"FindMesh", FindMesh},
		{"EnsureMesh", EnsureMesh},
		{"SetCurvePoint", SetCurvePoint},
		{"CurvePoint", CurvePoint},
		{"NumCurvePoints", NumCurvePoints},
		{"FindCurvePoint", FindCurvePoint},
		{"DeleteCurvePoint", DeleteCurvePoint},
		{"ClearCurvePoints", ClearCurvePoints},
		{"SetSkyFace", SetSkyFace},
		{"SkySphereTexture", SkySphereTexture},
		{"SetSkySphereTexture", SetSkySphereTexture},
		{"DrawLine", DrawLine},
		{"Draw2DLine", Draw2DLine},
		{"DrawSphereLines", DrawSphereLines},
		{"LensDirection", LensDirection},
		{"VecUserToWorldZPlane", VecUserToWorldZPlane},
		{"VecWorldToUser", VecWorldToUser},
		{0, 0}
	};

	scr::constant constants[] =
	{
		{"MESH_VOXELS", Mesh::VOXELS},
		{0, 0}
	};

	luaL_Reg palRegs[] =
	{
		{"FileName", PalFileName},
		{"NumSubPalettes", PalNumSubPalettes},
		{"__gc", PalDelete},
		{0, 0}
	};

	luaL_Reg texRegs[] =
	{
		{"FileName", TexFileName},
		{"ShortName", TexShortName},
		{"NumFrames", TexNumFrames},
		{"FrameDims", TexFrameDims},
		{"Mip", TexMip},
		{"SetMip", TexSetMip},
		{"__gc", TexDelete},
		{0, 0}
	};

	luaL_Reg mshRegs[] =
	{
		{"FileName", MshFileName},
		{"Flags", MshFlags},
		{"NumFrames", MshNumFrames},
		{"FrameRate", MshFrameRate},
		{"Socket", MshSocket},
		{"NumSockets", MshNumSockets},
		{"Animation", MshAnimation},
		{"NumAnimations", MshNumAnimations},
		{"Bounds", MshBounds},
		{"Radius", MshRadius},
		{"__gc", MshDelete},
		{0, 0}
	};

	luaL_Reg socRegs[] =
	{
		{"Mesh", Socket::CLuaMesh},
		{"Name", Socket::CLuaName},
		{"NumPlaces", Socket::CLuaNumValues},
		{"Place", SocPlace},
		{"PlaceEntity", SocPlaceEntity},
		{"PlaceAlign", SocPlaceAlign},
		{0, 0}
	};

	luaL_Reg aniRegs[] =
	{
		{"Mesh", Animation::CLuaMesh},
		{"Name", Animation::CLuaName},
		{"Frames", AniFrames},
		{"End", AniEnd},
		{"NumFrames", AniNumFrames},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		palRegs,
		texRegs,
		mshRegs,
		socRegs,
		aniRegs,
		0
	};

	const char* prefixes[] = {
		"Pal",
		"Tex",
		"Msh",
		"Soc",
		"Ani",
		0
	};

	scr::RegisterLibrary(scr::state, "grnd", regs, constants, 0, metas, prefixes);
	Palette::RegisterMetatable(palRegs, 0);
	Texture::RegisterMetatable(texRegs, 0);
	Mesh::RegisterMetatable(mshRegs, 0);
	Socket::RegisterMetatable(socRegs, 0);
	Animation::RegisterMetatable(aniRegs, 0);

	// Load light sphere meshes
	if(!(lightSphere = CreateSimpleMesh("light_sphere.msh")))
		WRP_FATAL("Could not load light_sphere.msh");

	if(!(lightHemi = CreateSimpleMesh("light_hemi.msh")))
		WRP_FATAL("Could not load light_hemi.msh");

	// Commands
	lua_pushcfunction(scr::state, CalculateCascadeDistances); con::CreateCommand("calc_cascade_dists");

	while(GLenum err = glGetError())
		con::AlertF("Initialization GL error: %s (%u)", GetErrorString(err), (unsigned)err);
}