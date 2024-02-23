// qua_lua.cpp
// Martynas Ceicys

#include "qua_lua.h"
#include "../vector/vec_lua.h"

namespace qua
{
	// QUATERNION LUA
	int	Norm(lua_State* l);
	int	Product(lua_State* l);
	int	Normalized(lua_State* l);
	int Conjugate(lua_State* l);
	int	Inverse(lua_State* l);
	int	Roll(lua_State* l);
	int	Pitch(lua_State* l);
	int	Yaw(lua_State* l);
	int	PitchYaw(lua_State* l);
	int Euler(lua_State* l);
	int Dir(lua_State* l);
	int Left(lua_State* l);
	int Up(lua_State* l);
	int Angle(lua_State* l);
	int AxisAngle(lua_State* l);
	int AngleSum(lua_State* l);
	int AngleApproach(lua_State* l);
	int AngleClamp(lua_State* l);
	int AngleSet(lua_State* l);
	int LookDelta(lua_State* l);
	int LookDeltaAxisAngle(lua_State* l);
	int	Dot(lua_State* l);
	int	QuaAxisAngle(lua_State* l);
	int QuaAngularVelocity(lua_State* l);
	int QuaYaw(lua_State* l);
	int QuaPitchYaw(lua_State* l);
	int	QuaEuler(lua_State* l);
	int QuaEulerReverse(lua_State* l);
	int QuaLook(lua_State* l);
	int QuaLookUpright(lua_State* l);
	int Delta(lua_State* l);
	int LocalDelta(lua_State* l);
	int GlobalToLocal(lua_State* l);
	int LocalToGlobal(lua_State* l);
	int AngleDelta(lua_State* l);
	int	VecRot(lua_State* l);
	int	VecRotInv(lua_State* l);
	int	Slerp(lua_State* l);
}

/*--------------------------------------
	qua::Init
--------------------------------------*/
void qua::Init()
{
	luaL_Reg regs[] =
	{
		{"Norm", Norm},
		{"Product", Product},
		{"Normalized", Normalized},
		{"Conjugate", Conjugate},
		{"Inverse", Inverse},
		{"Roll", Roll},
		{"Pitch", Pitch},
		{"Yaw", Yaw},
		{"PitchYaw", PitchYaw},
		{"Euler", Euler},
		{"Dir", Dir},
		{"Left", Left},
		{"Up", Up},
		{"Angle", Angle},
		{"AxisAngle", AxisAngle},
		{"AngleSum", AngleSum},
		{"AngleApproach", AngleApproach},
		{"AngleClamp", AngleClamp},
		{"AngleSet", AngleSet},
		{"LookDelta", LookDelta},
		{"LookDeltaAxisAngle", LookDeltaAxisAngle},
		{"Dot", Dot},
		{"QuaAxisAngle", QuaAxisAngle},
		{"QuaAngularVelocity", QuaAngularVelocity},
		{"QuaYaw", QuaYaw},
		{"QuaPitchYaw", QuaPitchYaw},
		{"QuaEuler", QuaEuler},
		{"QuaEulerReverse", QuaEulerReverse},
		{"QuaLook", QuaLook},
		{"QuaLookUpright", QuaLookUpright},
		{"Delta", Delta},
		{"LocalDelta", LocalDelta},
		{"GlobalToLocal", GlobalToLocal},
		{"LocalToGlobal", LocalToGlobal},
		{"AngleDelta", AngleDelta},
		{"VecRot", VecRot},
		{"VecRotInv", VecRotInv},
		{"Slerp", Slerp},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "gqua", regs, 0, 0, 0, 0);
}

/*--------------------------------------
	qua::LuaPushQua
--------------------------------------*/
void qua::LuaPushQua(lua_State* l, const com::Qua& q)
{
	lua_pushnumber(l, q[0]);
	lua_pushnumber(l, q[1]);
	lua_pushnumber(l, q[2]);
	lua_pushnumber(l, q[3]);
}

/*--------------------------------------
	qua::LuaToQua

Returns normalized.
--------------------------------------*/
com::Qua qua::LuaToQua(lua_State* l, int i0, int i1, int i2, int i3)
{
	com::Qua q =
	{
		lua_tonumber(l, i0),
		lua_tonumber(l, i1),
		lua_tonumber(l, i2),
		lua_tonumber(l, i3)
	};
	
	return q == com::QUA_IDENTITY ? com::QUA_IDENTITY : q.Normalized();
}

/*--------------------------------------
	qua::CheckLuaToQua
--------------------------------------*/
com::Qua qua::CheckLuaToQua(lua_State* l, int i0, int i1, int i2, int i3)
{
	com::Qua q =
	{
		luaL_checknumber(l, i0),
		luaL_checknumber(l, i1),
		luaL_checknumber(l, i2),
		luaL_checknumber(l, i3)
	};
	
	return q == com::QUA_IDENTITY ? com::QUA_IDENTITY : q.Normalized();
}

/*
################################################################################################


	QUATERNION LUA


################################################################################################
*/

/*--------------------------------------
LUA	qua::Norm

IN	xQ, yQ, zQ, wQ
OUT	nNorm
--------------------------------------*/
int qua::Norm(lua_State* l)
{
	com::Qua q = {
		luaL_checknumber(l, 1),
		luaL_checknumber(l, 2),
		luaL_checknumber(l, 3),
		luaL_checknumber(l, 4)
	};

	lua_pushnumber(l, q.Norm());
	return 1;
}

/*--------------------------------------
LUA	qua::Product

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR
OUT	xS, yS, zS, wS
--------------------------------------*/
int qua::Product(lua_State* l)
{
	LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4) * CheckLuaToQua(l, 5, 6, 7, 8));
	return 4;
}

/*--------------------------------------
LUA	qua::Normalized

IN	xQ, yQ, zQ, wQ
OUT	xR, yR, zR, wR
--------------------------------------*/
int qua::Normalized(lua_State* l)
{
	LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4));
	return 4;
}

/*--------------------------------------
LUA	qua::Conjugate

IN	xQ, yQ, zQ, wQ
OUT	-xQ, -yQ, -zQ, wQ
--------------------------------------*/
int qua::Conjugate(lua_State* l)
{
	LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).Conjugate());
	return 4;
}

/*--------------------------------------
LUA	qua::Inverse

IN	xQ, yQ, zQ, wQ
OUT	xR, yR, zR, wR
--------------------------------------*/
int qua::Inverse(lua_State* l)
{
	LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).Inverse());
	return 4;
}

/*--------------------------------------
LUA	qua::Roll

IN	xQ, yQ, zQ, wQ
OUT	nRoll
--------------------------------------*/
int qua::Roll(lua_State* l)
{
	lua_pushnumber(l, CheckLuaToQua(l, 1, 2, 3, 4).Roll());
	return 1;
}

/*--------------------------------------
LUA	qua::Pitch

IN	xQ, yQ, zQ, wQ
OUT	nPitch
--------------------------------------*/
int qua::Pitch(lua_State* l)
{
	lua_pushnumber(l, CheckLuaToQua(l, 1, 2, 3, 4).Pitch());
	return 1;
}

/*--------------------------------------
LUA	qua::Yaw

IN	xQ, yQ, zQ, wQ
OUT	nYaw
--------------------------------------*/
int qua::Yaw(lua_State* l)
{
	lua_pushnumber(l, CheckLuaToQua(l, 1, 2, 3, 4).Yaw());
	return 1;
}

/*--------------------------------------
LUA	qua::PitchYaw

IN	xQ, yQ, zQ, wQ
OUT	nPitch, nYaw
--------------------------------------*/
int qua::PitchYaw(lua_State* l)
{
	com::Qua q = CheckLuaToQua(l, 1, 2, 3, 4);
	lua_pushnumber(l, q.Pitch());
	lua_pushnumber(l, q.Yaw());
	return 2;
}

/*--------------------------------------
LUA	qua::Euler

IN	xQ, yQ, zQ, wQ
OUT	nRoll, nPitch, nYaw
--------------------------------------*/
int qua::Euler(lua_State* l)
{
	com::Qua q = CheckLuaToQua(l, 1, 2, 3, 4);
	lua_pushnumber(l, q.Roll());
	lua_pushnumber(l, q.Pitch());
	lua_pushnumber(l, q.Yaw());
	return 3;
}

/*--------------------------------------
LUA	qua::Dir

IN	q4Q
OUT	v3Dir
--------------------------------------*/
int qua::Dir(lua_State* l)
{
	vec::LuaPushVec(l, CheckLuaToQua(l, 1, 2, 3, 4).Dir());
	return 3;
}

/*--------------------------------------
LUA	qua::Left

IN	q4Q
OUT	v3Left
--------------------------------------*/
int qua::Left(lua_State* l)
{
	vec::LuaPushVec(l, CheckLuaToQua(l, 1, 2, 3, 4).Left());
	return 3;
}

/*--------------------------------------
LUA	qua::Up

IN	q4Q
OUT	v3Up
--------------------------------------*/
int qua::Up(lua_State* l)
{
	vec::LuaPushVec(l, CheckLuaToQua(l, 1, 2, 3, 4).Up());
	return 3;
}

/*--------------------------------------
LUA	qua::Angle

IN	q4Q
OUT	nAngle
--------------------------------------*/
int qua::Angle(lua_State* l)
{
	lua_pushnumber(l, CheckLuaToQua(l, 1, 2, 3, 4).Angle());
	return 1;
}

/*--------------------------------------
LUA	qua::AxisAngle

IN	q4Q
OUT	v3Axis, nAngle
--------------------------------------*/
int qua::AxisAngle(lua_State* l)
{
	com::Vec3 axis;
	com::fp angle;
	CheckLuaToQua(l, 1, 2, 3, 4).AxisAngle(axis, angle);
	vec::LuaPushVec(l, axis);
	lua_pushnumber(l, angle);
	return 4;
}

/*--------------------------------------
LUA	qua::AngleSum

IN	q4Q, nAdd
OUT	q4R
--------------------------------------*/
int qua::AngleSum(lua_State* l)
{
	qua::LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).AngleSum(luaL_checknumber(l, 5)));
	return 4;
}

/*--------------------------------------
LUA	qua::AngleApproach

IN	q4Q, nTarget, nDelta
OUT	q4R
--------------------------------------*/
int qua::AngleApproach(lua_State* l)
{
	qua::LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).AngleApproach(luaL_checknumber(l, 5),
		luaL_checknumber(l, 6)));

	return 4;
}

/*--------------------------------------
LUA	qua::AngleClamp

IN	q4Q, nMax
OUT	q4R
--------------------------------------*/
int qua::AngleClamp(lua_State* l)
{
	qua::LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).AngleClamp(luaL_checknumber(l, 5)));
	return 4;
}

/*--------------------------------------
LUA	qua::AngleSet

IN	q4Q, nAngle
OUT	q4R
--------------------------------------*/
int qua::AngleSet(lua_State* l)
{
	qua::LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).AngleSet(luaL_checknumber(l, 5)));
	return 4;
}

/*--------------------------------------
LUA	qua::LookDelta

IN	q4Q, v3V
OUT	q4Delta
--------------------------------------*/
int qua::LookDelta(lua_State* l)
{
	LuaPushQua(l, CheckLuaToQua(l, 1, 2, 3, 4).LookDelta(vec::CheckLuaToVec(l, 5, 6, 7)));
	return 4;
}

/*--------------------------------------
LUA	qua::LookDeltaAxisAngle

IN	q4Q, v3V
OUT	v3Axis, nAngle
--------------------------------------*/
int qua::LookDeltaAxisAngle(lua_State* l)
{
	com::Vec3 axis;
	com::fp angle;
	CheckLuaToQua(l, 1, 2, 3, 4).LookDeltaAxisAngle(vec::CheckLuaToVec(l, 5, 6, 7), axis, angle);
	vec::LuaPushVec(l, axis);
	lua_pushnumber(l, angle);
	return 4;
}

/*--------------------------------------
LUA	qua::Dot

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR
OUT	nDot
--------------------------------------*/
int qua::Dot(lua_State* l)
{
	lua_pushnumber(l, com::Dot(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 1;
}

/*--------------------------------------
LUA	qua::QuaAxisAngle

IN	xV, yV, zV, nAngle
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaAxisAngle(lua_State* l)
{
	LuaPushQua(l, com::QuaAxisAngle(vec::CheckLuaToVec(l, 1, 2, 3), luaL_checknumber(l, 4)));
	return 4;
}

/*--------------------------------------
LUA	qua::QuaAngularVelocity

IN	v3Vel, [nTime = 1.0]
OUT	q4Q
--------------------------------------*/
int qua::QuaAngularVelocity(lua_State* l)
{
	LuaPushQua(l, com::QuaAngularVelocity(vec::CheckLuaToVec(l, 1, 2, 3),
		luaL_optnumber(l, 4, (lua_Number)1.0)));

	return 4;
}

/*--------------------------------------
LUA	qua::QuaYaw

IN	nYaw
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaYaw(lua_State* l)
{
	LuaPushQua(l, com::QuaEulerYaw(luaL_checknumber(l, 1)));
	return 4;
}

/*--------------------------------------
LUA	qua::QuaPitchYaw

IN	nPitch, nYaw
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaPitchYaw(lua_State* l)
{
	LuaPushQua(l, com::QuaEulerPitchYaw(luaL_checknumber(l, 1), luaL_checknumber(l, 2)));
	return 4;
}

/*--------------------------------------
LUA	qua::QuaEuler

IN	nRoll, nPitch, nYaw
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaEuler(lua_State* l)
{
	LuaPushQua(l, com::QuaEuler(luaL_checknumber(l, 1), luaL_checknumber(l, 2), luaL_checknumber(l, 3)));
	return 4;
}

/*--------------------------------------
LUA	qua::QuaEulerReverse

IN	nRoll, nPitch, nYaw
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaEulerReverse(lua_State* l)
{
	LuaPushQua(l, com::QuaEulerReverse(luaL_checknumber(l, 1), luaL_checknumber(l, 2),
		luaL_checknumber(l, 3)));

	return 4;
}

/*--------------------------------------
LUA	qua::QuaLook

IN	xV, yV, zV
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaLook(lua_State* l)
{
	LuaPushQua(l, com::QuaLook(vec::CheckLuaToVec(l, 1, 2, 3)));
	return 4;
}

/*--------------------------------------
LUA	qua::QuaLookUpright

IN	xV, yV, zV
OUT	xQ, yQ, zQ, wQ
--------------------------------------*/
int qua::QuaLookUpright(lua_State* l)
{
	LuaPushQua(l, com::QuaLookUpright(vec::CheckLuaToVec(l, 1, 2, 3)));
	return 4;
}

/*--------------------------------------
LUA	qua::Delta

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR
OUT	xS, yS, zS, wS
--------------------------------------*/
int qua::Delta(lua_State* l)
{
	LuaPushQua(l, com::Delta(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 4;
}

/*--------------------------------------
LUA	qua::LocalDelta

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR
OUT	xS, yS, zS, wS
--------------------------------------*/
int qua::LocalDelta(lua_State* l)
{
	LuaPushQua(l, com::LocalDelta(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 4;
}

/*--------------------------------------
LUA	qua::GlobalToLocal

IN	q4Ref, q4GlobalDelta
OUT	q4LocalDelta
--------------------------------------*/
int qua::GlobalToLocal(lua_State* l)
{
	LuaPushQua(l, com::GlobalToLocal(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 4;
}

/*--------------------------------------
LUA	qua::LocalToGlobal

IN	q4Ref, q4LocalDelta
OUT	q4GlobalDelta
--------------------------------------*/
int qua::LocalToGlobal(lua_State* l)
{
	LuaPushQua(l, com::LocalToGlobal(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 4;
}

/*--------------------------------------
LUA	qua::AngleDelta

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR
OUT	nDelta
--------------------------------------*/
int qua::AngleDelta(lua_State* l)
{
	lua_pushnumber(l, com::AngleDelta(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8)));
	return 1;
}

/*--------------------------------------
LUA	qua::VecRot

IN	xU, yU, zU, xQ, yQ, zQ, wQ
OUT	xV, yV, zV
--------------------------------------*/
int qua::VecRot(lua_State* l)
{
	vec::LuaPushVec(l, VecRot(vec::CheckLuaToVec(l, 1, 2, 3), CheckLuaToQua(l, 4, 5, 6, 7)));
	return 3;
}

/*--------------------------------------
LUA	qua::VecRotInv

IN	xU, yU, zU, xQ, yQ, zQ, wQ
OUT	xV, yV, zV
--------------------------------------*/
int qua::VecRotInv(lua_State* l)
{
	vec::LuaPushVec(l, VecRotInv(vec::CheckLuaToVec(l, 1, 2, 3), CheckLuaToQua(l, 4, 5, 6, 7)));
	return 3;
}

/*--------------------------------------
LUA	qua::Slerp

IN	xQ, yQ, zQ, wQ, xR, yR, zR, wR, nF
OUT	xS, yS, zS, wS
--------------------------------------*/
int qua::Slerp(lua_State* l)
{
	LuaPushQua(l, com::Slerp(CheckLuaToQua(l, 1, 2, 3, 4), CheckLuaToQua(l, 5, 6, 7, 8),
		luaL_checknumber(l, 9)));
	return 4;
}