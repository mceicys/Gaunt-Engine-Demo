// vec_lua.cpp
// Martynas Ceicys

#include "vec_lua.h"

namespace vec
{
	// 2D VECTOR LUA
	int	Mag2(lua_State* l);
	int	MagSq2(lua_State* l);
	int Normalized2(lua_State* l);
	int Resized2(lua_State* l);
	int Yaw(lua_State* l);
	int Rotated2(lua_State* l);
	int	Dot2(lua_State* l);

	// 3D VECTOR LUA
	int	Mag3(lua_State* l);
	int	MagSq3(lua_State* l);
	int Normalized3(lua_State* l);
	int Resized3(lua_State* l);
	int Pitch(lua_State* l);
	int PitchYaw(lua_State* l);
	int	Dot3(lua_State* l);
	int	Cross(lua_State* l);
	int VecPitchYaw(lua_State* l);
	int DirDelta(lua_State* l);
	int DirAngle3(lua_State* l);
}

/*--------------------------------------
	vec::Init
--------------------------------------*/
void vec::Init()
{
	luaL_Reg regs[] =
	{
		{"Mag2", Mag2},
		{"MagSq2", MagSq2},
		{"Normalized2", Normalized2},
		{"Resized2", Resized2},
		{"Yaw", Yaw},
		{"Rotated2", Rotated2},
		{"Dot2", Dot2},
		{"Mag3", Mag3},
		{"MagSq3", MagSq3},
		{"Normalized3", Normalized3},
		{"Resized3", Resized3},
		{"Pitch", Pitch},
		{"PitchYaw", PitchYaw},
		{"Dot3", Dot3},
		{"Cross", Cross},
		{"VecPitchYaw", VecPitchYaw},
		{"DirDelta", DirDelta},
		{"DirAngle3", DirAngle3},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "gvec", regs, 0, 0, 0, 0);
}

/*--------------------------------------
	vec::LuaPushVec

Pushes number components onto the stack.
--------------------------------------*/
int vec::LuaPushVec(lua_State* l, const com::Vec2& v)
{
	lua_pushnumber(l, v.x);
	lua_pushnumber(l, v.y);
	return 2;
}

int vec::LuaPushVec(lua_State* l, const com::Vec3& v)
{
	lua_pushnumber(l, v.x);
	lua_pushnumber(l, v.y);
	lua_pushnumber(l, v.z);
	return 3;
}

/*--------------------------------------
	vec::LuaToVec

Returns vector with numbers at indices copied to components.
--------------------------------------*/
com::Vec2 vec::LuaToVec(lua_State* l, int indexX, int indexY)
{
	return com::Vec2(lua_tonumber(l, indexX), lua_tonumber(l, indexY));
}

com::Vec3 vec::LuaToVec(lua_State* l, int indexX, int indexY, int indexZ)
{
	return com::Vec3(lua_tonumber(l, indexX), lua_tonumber(l, indexY), lua_tonumber(l, indexZ));
}

/*--------------------------------------
	vec::CheckLuaToVec
--------------------------------------*/
com::Vec2 vec::CheckLuaToVec(lua_State* l, int indexX, int indexY)
{
	return com::Vec2(luaL_checknumber(l, indexX), luaL_checknumber(l, indexY));
}

com::Vec3 vec::CheckLuaToVec(lua_State* l, int indexX, int indexY, int indexZ)
{
	return com::Vec3(luaL_checknumber(l, indexX), luaL_checknumber(l, indexY),
		luaL_checknumber(l, indexZ));
}

/*
################################################################################################


	2D VECTOR LUA


################################################################################################
*/

/*--------------------------------------
LUA	vec::Mag2

IN	xU, yU
OUT	nMagnitude
--------------------------------------*/
int vec::Mag2(lua_State* l)
{
	lua_pushnumber(l, LuaToVec(l, 1, 2).Mag());
	return 1;
}

/*--------------------------------------
LUA	vec::MagSq2

IN	xU, yU
OUT	nMagSq
--------------------------------------*/
int vec::MagSq2(lua_State* l)
{
	lua_pushnumber(l, LuaToVec(l, 1, 2).MagSq());
	return 1;
}

/*--------------------------------------
LUA	vec::Normalized2

IN	xU, yU
OUT	xV, yV, [bError]
--------------------------------------*/
int vec::Normalized2(lua_State* l)
{
	int err = 0;
	LuaPushVec(l, LuaToVec(l, 1, 2).Normalized(&err));

	if(err)
	{
		lua_pushboolean(l, true);
		return 3;
	}

	return 2;
}

/*--------------------------------------
LUA	vec::Resized2

IN	xU, yU, nMag
OUT	xV, yV, [bError]
--------------------------------------*/
int vec::Resized2(lua_State* l)
{
	int err = 0;
	LuaPushVec(l, LuaToVec(l, 1, 2).Resized(luaL_checknumber(l, 3), &err));

	if(err)
	{
		lua_pushboolean(l, true);
		return 3;
	}

	return 2;
}

/*--------------------------------------
LUA	vec::Yaw

IN	xU, yU, default
OUT	(nYaw | default)
--------------------------------------*/
int vec::Yaw(lua_State* l)
{
	com::fp yaw = LuaToVec(l, 1, 2).Yaw(COM_FP_MAX);

	if(yaw == COM_FP_MAX)
		lua_pushvalue(l, 3);
	else
		lua_pushnumber(l, yaw);

	return 1;
}

/*--------------------------------------
LUA	vec::Rotated2

IN	xU, yU, nYawDel
OUT	xU, yU
--------------------------------------*/
int vec::Rotated2(lua_State* l)
{
	vec::LuaPushVec(l, LuaToVec(l, 1, 2).Rotated(luaL_checknumber(l, 3)));
	return 2;
}

/*--------------------------------------
LUA	vec::Dot2

IN	xU, yU, xV, yV
OUT	nDotProduct
--------------------------------------*/
int vec::Dot2(lua_State* l)
{
	lua_pushnumber(l, Dot(LuaToVec(l, 1, 2), LuaToVec(l, 3, 4)));
	return 1;
}

/*
################################################################################################


	3D VECTOR LUA


################################################################################################
*/

/*--------------------------------------
LUA	vec::Mag3

IN	xU, yU, zU
OUT	nMagnitude
--------------------------------------*/
int vec::Mag3(lua_State* l)
{
	lua_pushnumber(l, LuaToVec(l, 1, 2, 3).Mag());
	return 1;
}

/*--------------------------------------
LUA	vec::MagSq3

IN	xU, yU, zU
OUT	nMagSq
--------------------------------------*/
int vec::MagSq3(lua_State* l)
{
	lua_pushnumber(l, LuaToVec(l, 1, 2, 3).MagSq());
	return 1;
}

/*--------------------------------------
LUA	vec::Normalized3

IN	xU, yU, zU
OUT	xV, yV, zV, [bError]
--------------------------------------*/
int vec::Normalized3(lua_State* l)
{
	int err = 0;
	LuaPushVec(l, LuaToVec(l, 1, 2, 3).Normalized(&err));

	if(err)
	{
		lua_pushboolean(l, true);
		return 4;
	}

	return 3;
}

/*--------------------------------------
LUA	vec::Resized3

IN	xU, yU, zU, nMag
OUT	xV, yV, zU, [bError]
--------------------------------------*/
int vec::Resized3(lua_State* l)
{
	int err = 0;
	LuaPushVec(l, LuaToVec(l, 1, 2, 3).Resized(luaL_checknumber(l, 4), &err));

	if(err)
	{
		lua_pushboolean(l, true);
		return 4;
	}

	return 3;
}

/*--------------------------------------
LUA	vec::Pitch

IN	xU, yU, zU
OUT	nPitch
--------------------------------------*/
int vec::Pitch(lua_State* l)
{
	lua_pushnumber(l, LuaToVec(l, 1, 2, 3).Pitch());
	return 1;
}

/*--------------------------------------
LUA	vec::PitchYaw

IN	xU, yU, zU, [defaultYaw]
OUT	nPitch, (nYaw | defaultYaw)
--------------------------------------*/
int vec::PitchYaw(lua_State* l)
{
	com::fp pitch, yaw;
	LuaToVec(l, 1, 2, 3).PitchYaw(pitch, yaw, COM_FP_MAX);
	lua_pushnumber(l, pitch);

	if(yaw == COM_FP_MAX)
	{
		if(lua_gettop(l) == 4)
			lua_pushnil(l); // No default given
		else
			lua_pushvalue(l, 4);
	}
	else
		lua_pushnumber(l, yaw);

	return 2;
}

/*--------------------------------------
LUA	vec::Dot3

IN	xU, yU, zU, xV, yV, zV
OUT	nDotProduct
--------------------------------------*/
int vec::Dot3(lua_State* l)
{
	lua_pushnumber(l, Dot(LuaToVec(l, 1, 2, 3), LuaToVec(l, 4, 5, 6)));
	return 1;
}

/*--------------------------------------
LUA	vec::Cross

IN	xU, yU, zU, xV, yV, zV
OUT	xCross, yCross, zCross
--------------------------------------*/
int vec::Cross(lua_State* l)
{
	LuaPushVec(l, Cross(LuaToVec(l, 1, 2, 3), LuaToVec(l, 4, 5, 6)));
	return 3;
}

/*--------------------------------------
LUA	vec::VecPitchYaw

IN	nPitch, nYaw
OUT	x, y, z
--------------------------------------*/
int vec::VecPitchYaw(lua_State* l)
{
	LuaPushVec(l, com::VecFromPitchYaw(lua_tonumber(l, 1), lua_tonumber(l, 2)));
	return 3;
}

/*--------------------------------------
LUA	vec::DirDelta

IN	v3U, v3V
OUT	v3Axis, nAngle
--------------------------------------*/
int vec::DirDelta(lua_State* l)
{
	com::Vec3 axis;
	com::fp angle;
	com::DirDelta(CheckLuaToVec(l, 1, 2, 3), CheckLuaToVec(l, 4, 5, 6), axis, angle);
	LuaPushVec(l, axis);
	lua_pushnumber(l, angle);
	return 4;
}

/*--------------------------------------
LUA	vec::DirAngle3

IN	v3U, v3V
OUT	nAngle
--------------------------------------*/
int vec::DirAngle3(lua_State* l)
{
	com::fp angle;
	lua_pushnumber(l, com::DirAngle(CheckLuaToVec(l, 1, 2, 3), CheckLuaToVec(l, 4, 5, 6)));
	return 1;
}