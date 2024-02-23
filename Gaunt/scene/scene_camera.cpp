// scene_camera.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../vector/vec_lua.h"
#include "../quaternion//qua_lua.h"

namespace scn
{
	res::Ptr<Camera> activeCam;
}

/*
################################################################################################


	CAMERA


################################################################################################
*/

const char* const scn::Camera::METATABLE = "metaCamera";

/*--------------------------------------
	scn::Camera::Camera
--------------------------------------*/
scn::Camera::Camera(const com::Vec3& p, const com::Qua& o, float fov) : pos(p), oldPos(p),
	ori(o), oldOri(o), fov(fov), oldFOV(fov), flags(LERP_POS | LERP_ORI | LERP_FOV)
{
	EnsureLink();
}

/*--------------------------------------
	scn::Camera::FinalPos
--------------------------------------*/
com::Vec3 scn::Camera::FinalPos() const
{
	if(flags & LERP_POS)
		return COM_LERP(oldPos, pos, wrp::Fraction());
	else
		return pos;
}

/*--------------------------------------
	scn::Camera::FinalOri
--------------------------------------*/
com::Qua scn::Camera::FinalOri() const
{
	if(flags & LERP_ORI && oldOri != ori)
		return com::Slerp(oldOri, ori, wrp::Fraction()).Normalized();
	else
		return ori;
}

/*--------------------------------------
	scn::Camera::FinalFOV
--------------------------------------*/
float scn::Camera::FinalFOV() const
{
	if(flags & LERP_FOV)
		return COM_LERP(oldFOV, fov, wrp::Fraction());
	else
		return fov;
}

/*--------------------------------------
	scn::Camera::FinalSkewOrigin
--------------------------------------*/
com::Vec3 scn::Camera::FinalSkewOrigin() const
{
	return skewOrigin;
}

/*--------------------------------------
	scn::Camera::FinalZSkew
--------------------------------------*/
com::Vec2 scn::Camera::FinalZSkew() const
{
	return zSkew;
}

/*--------------------------------------
	scn::Camera::SaveOld
--------------------------------------*/
void scn::Camera::SaveOld()
{
	oldPos = pos;
	oldOri = ori;
	oldFOV = fov;
}

/*--------------------------------------
	scn::ActiveCamera
--------------------------------------*/
scn::Camera* scn::ActiveCamera()
{
	return activeCam;
}

/*--------------------------------------
	scn::SetActiveCamera
--------------------------------------*/
void scn::SetActiveCamera(scn::Camera* cam)
{
	activeCam.Set(cam);
}

/*
################################################################################################


	CAMERA LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::CreateCamera (Camera)

IN	v3Pos, q4Ori, nFOV
OUT	cam
--------------------------------------*/
int scn::CreateCamera(lua_State* l)
{
	com::Vec3 pos = vec::LuaToVec(l, 1, 2, 3);
	com::Qua ori = qua::LuaToQua(l, 4, 5, 6, 7);
	float fov = lua_tonumber(l, 8);
	Camera* c = new scn::Camera(pos, ori, fov);
	c->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::ActiveCamera

OUT	camActive
--------------------------------------*/
int scn::ActiveCamera(lua_State* l)
{
	if(Camera* active = ActiveCamera())
	{
		active->LuaPush();
		return 1;
	}
	
	return 0;
}

/*--------------------------------------
LUA	scn::SetActiveCamera

IN	camActive
--------------------------------------*/
int scn::SetActiveCamera(lua_State* l)
{
	SetActiveCamera(Camera::OptionalLuaTo(1));
	return 0;
}

/*--------------------------------------
LUA	scn::CamPos (Pos)

IN	cam
OUT	xCamPos, yCamPos, zCamPos
--------------------------------------*/
int scn::CamPos(lua_State* l)
{
	vec::LuaPushVec(l, Camera::CheckLuaTo(1)->pos);
	return 3;
}

/*--------------------------------------
LUA	scn::CamSetPos (SetPos)

IN	cam, xCamPos, yCamPos, zCamPos
--------------------------------------*/
int scn::CamSetPos(lua_State* l)
{
	Camera::CheckLuaTo(1)->pos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	scn::CamOldPos (OldPos)

IN	cam
OUT	xCamOldPos, yCamOldPos, zCamOldPos
--------------------------------------*/
int scn::CamOldPos(lua_State* l)
{
	vec::LuaPushVec(l, Camera::CheckLuaTo(1)->oldPos);
	return 3;
}

/*--------------------------------------
LUA	scn::CamSetOldPos (SetOldPos)

IN	cam, xCamOldPos, yCamOldPos, zCamOldPos
--------------------------------------*/
int scn::CamSetOldPos(lua_State* l)
{
	Camera::CheckLuaTo(1)->oldPos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	scn::CamFinalPos (FinalPos)

IN	cam
OUT	xCamFinPos, yCamFinPos, zCamFinPos
--------------------------------------*/
int scn::CamFinalPos(lua_State* l)
{
	vec::LuaPushVec(l, Camera::CheckLuaTo(1)->FinalPos());
	return 3;
}

/*--------------------------------------
LUA	scn::CamOri (Ori)

IN	cam
OUT	xCamOri, yCamOri, zCamOri, wCamOri
--------------------------------------*/
int scn::CamOri(lua_State* l)
{
	qua::LuaPushQua(l, Camera::CheckLuaTo(1)->ori);
	return 4;
}

/*--------------------------------------
LUA	scn::CamSetOri (SetOri)

IN	cam, xCamOri, yCamOri, zCamOri, wCamOri
--------------------------------------*/
int scn::CamSetOri(lua_State* l)
{
	Camera::CheckLuaTo(1)->ori = qua::LuaToQua(l, 2, 3, 4, 5);
	return 0;
}

/*--------------------------------------
LUA	scn::CamOldOri (OldOri)

IN	cam
OUT	xCamOldOri, yCamOldOri, zCamOldOri, wCamOldOri
--------------------------------------*/
int scn::CamOldOri(lua_State* l)
{
	qua::LuaPushQua(l, Camera::CheckLuaTo(1)->oldOri);
	return 4;
}

/*--------------------------------------
LUA	scn::CamSetOldOri (SetOldOri)

IN	cam, xCamOldOri, yCamOldOri, zCamOldOri, wCamOldOri
--------------------------------------*/
int scn::CamSetOldOri(lua_State* l)
{
	Camera::CheckLuaTo(1)->oldOri = qua::LuaToQua(l, 2, 3, 4, 5);
	return 0;
}

/*--------------------------------------
LUA	scn::CamFinalOri (FinalOri)

IN	cam
OUT	xCamFinOri, yCamFinOri, zCamFinOri, wCamFinOri
--------------------------------------*/
int scn::CamFinalOri(lua_State* l)
{
	qua::LuaPushQua(l, Camera::CheckLuaTo(1)->FinalOri());
	return 4;
}

/*--------------------------------------
LUA	scn::CamPlace (Place)

IN	cam
OUT	v3Pos, q4Ori
--------------------------------------*/
int scn::CamPlace(lua_State* l)
{
	Camera& cam = *Camera::CheckLuaTo(1);
	vec::LuaPushVec(l, cam.pos);
	qua::LuaPushQua(l, cam.ori);
	return 7;
}

/*--------------------------------------
LUA	scn::CamSetPlace (SetPlace)

IN	cam, v3Pos, q4Ori
--------------------------------------*/
int scn::CamSetPlace(lua_State* l)
{
	Camera& cam = *Camera::CheckLuaTo(1);
	cam.pos = vec::LuaToVec(l, 2, 3, 4);
	cam.ori = qua::LuaToQua(l, 5, 6, 7, 8);
	return 0;
}

/*--------------------------------------
LUA	scn::CamFOV (FOV)

IN	cam
OUT	nCamFOV
--------------------------------------*/
int scn::CamFOV(lua_State* l)
{
	lua_pushnumber(l, Camera::CheckLuaTo(1)->fov);
	return 1;
}

/*--------------------------------------
LUA	scn::CamSetFOV (SetFOV)

IN	cam, nCamFOV
--------------------------------------*/
int scn::CamSetFOV(lua_State* l)
{
	Camera::CheckLuaTo(1)->fov = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::CamOldFOV (OldFOV)

IN	cam
OUT	nCamOldFOV
--------------------------------------*/
int scn::CamOldFOV(lua_State* l)
{
	lua_pushnumber(l, Camera::CheckLuaTo(1)->oldFOV);
	return 1;
}

/*--------------------------------------
LUA	scn::CamSetOldFOV (SetOldFOV)

IN	cam, nCamOldFOV
--------------------------------------*/
int scn::CamSetOldFOV(lua_State* l)
{
	Camera::CheckLuaTo(1)->oldFOV = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::CamFinalFOV (FinalFOV)

IN	cam
OUT	nCamFinalFOV
--------------------------------------*/
int scn::CamFinalFOV(lua_State* l)
{
	lua_pushnumber(l, Camera::CheckLuaTo(1)->FinalFOV());
	return 1;
}

/*--------------------------------------
LUA	scn::CamFinalHFOV (FinalHFOV)

IN	cam
OUT	nCamFinalHFOV
--------------------------------------*/
int scn::CamFinalHFOV(lua_State* l)
{
	lua_pushnumber(l, com::HorizontalFOV(Camera::CheckLuaTo(1)->FinalFOV(),
		(float)wrp::VideoWidth() / (float)wrp::VideoHeight()));

	return 1;
}

/*--------------------------------------
LUA	scn::CamSkewOrigin (SkewOrigin)

IN	cam
OUT	v3Origin
--------------------------------------*/
int scn::CamSkewOrigin(lua_State* l)
{
	vec::LuaPushVec(l, Camera::CheckLuaTo(1)->skewOrigin);
	return 3;
}

/*--------------------------------------
LUA	scn::CamSetSkewOrigin (SetSkewOrigin)

IN	cam, v3Origin
--------------------------------------*/
int scn::CamSetSkewOrigin(lua_State* l)
{
	Camera::CheckLuaTo(1)->skewOrigin = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	scn::CamZSkew (ZSkew)

IN	cam
OUT	v2ZSkew
--------------------------------------*/
int scn::CamZSkew(lua_State* l)
{
	vec::LuaPushVec(l, Camera::CheckLuaTo(1)->zSkew);
	return 2;
}

/*--------------------------------------
LUA	scn::CamSetZSkew (SetZSkew)

IN	cam, v2ZSkew
--------------------------------------*/
int scn::CamSetZSkew(lua_State* l)
{
	Camera::CheckLuaTo(1)->zSkew = vec::LuaToVec(l, 2, 3);
	return 0;
}

/*--------------------------------------
LUA	scn::CamLerpPos (LerpPos)

IN	cam
OUT	bCamLerpPos
--------------------------------------*/
int scn::CamLerpPos(lua_State* l)
{
	lua_pushboolean(l, Camera::CheckLuaTo(1)->flags & Camera::LERP_POS);
	return 1;
}

/*--------------------------------------
LUA	scn::CamSetLerpPos (SetLerpPos)

IN	cam, bCamLerpPos
--------------------------------------*/
int scn::CamSetLerpPos(lua_State* l)
{
	scn::Camera* c = Camera::CheckLuaTo(1);
	c->flags = com::FixedBits(c->flags, Camera::LERP_POS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::CamLerpOri (LerpOri)

IN	cam
OUT	bCamLerpOri
--------------------------------------*/
int scn::CamLerpOri(lua_State* l)
{
	lua_pushboolean(l, Camera::CheckLuaTo(1)->flags & Camera::LERP_ORI);
	return 1;
}

/*--------------------------------------
LUA	scn::CamSetLerpOri (SetLerpOri)

IN	cam, bCamLerpOri
--------------------------------------*/
int scn::CamSetLerpOri(lua_State* l)
{
	scn::Camera* c = Camera::CheckLuaTo(1);
	c->flags = com::FixedBits(c->flags, Camera::LERP_ORI, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::CamLerpFOV (LerpFOV)

IN	cam
OUT	bCamLerpFOV
--------------------------------------*/
int scn::CamLerpFOV(lua_State* l)
{
	lua_pushboolean(l, Camera::CheckLuaTo(1)->flags & Camera::LERP_FOV);
	return 1;
}

/*--------------------------------------
LUA	scn::CamSetLerpFOV (SetLerpFOV)

IN	cam, bCamLerpFOV
--------------------------------------*/
int scn::CamSetLerpFOV(lua_State* l)
{
	scn::Camera* c = Camera::CheckLuaTo(1);
	c->flags = com::FixedBits(c->flags, Camera::LERP_FOV, lua_toboolean(l, 2) != 0);
	return 0;
}