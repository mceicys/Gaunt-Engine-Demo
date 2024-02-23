// spline_lua.h
// Martynas Ceicys

#ifndef SPLINE_LUA_H
#define SPLINE_LUA_H

#include "../../GauntCommon/spline.h"
#include "../../GauntCommon/type.h"
#include "../resource/resource.h"
#include "../script/script.h"
#include "../vector/vec_lua.h"

namespace comlua
{

/*======================================
	comlua::LuaSpline
======================================*/
template <typename u>
class LuaSpline : public res::Resource<LuaSpline<u>>
{
public:
	com::Spline<u> s;

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaCreate (Spline...)

OUT	spl
--------------------------------------*/
static int CLuaCreate(lua_State* l)
{
	LuaSpline<u>* spline = new LuaSpline<u>;
	spline->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaPt (Pt)

IN	splS, iPt
OUT	val, nTime
--------------------------------------*/
static int CLuaPt(lua_State* l)
{
	LuaSpline<u>* spline = CheckLuaTo(1);
	int pt = luaL_checkinteger(l, 2);

	if(pt < 0 || pt >= spline->s.numPts)
		luaL_error(l, "iPt = %d is out of range [0, %d]", pt, (int)spline->s.numPts);

	return LuaPushPoint(l, spline->s.pts[pt]);
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaNumPts (NumPts)

IN	splS
OUT	iNumPts
--------------------------------------*/
static int CLuaNumPts(lua_State* l)
{
	lua_pushinteger(l, CheckLuaTo(1)->s.numPts);
	return 1;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaSetPt (SetPt)

IN	splS, iPt, val, nTime
--------------------------------------*/
static int CLuaSetPt(lua_State* l)
{
	LuaSpline<u>* spline = CheckLuaTo(1);
	int pt = scr::CheckLuaToUnsigned(l, 2);
	const com::Spline<u>::spline_point DEF = {0};
	com::Spline<u>& s = spline->s;
	s.pts.Ensure(pt + 1, DEF);
	s.pts[pt] = PointChecker<u>::CheckLuaToPoint(l, 3);
	s.numPts = com::Max<size_t>(s.numPts, pt + 1);
	s.ClearCoefficients();
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaSetNumPts (SetNumPts)

IN	splS, iNumPts
--------------------------------------*/
static int CLuaSetNumPts(lua_State* l)
{
	LuaSpline<u>* spline = CheckLuaTo(1);
	int numPts = scr::CheckLuaToUnsigned(l, 2);
	const com::Spline<u>::spline_point DEF = {0};
	com::Spline<u>& s= spline->s;
	s.pts.Ensure(numPts, DEF);
	s.numPts = numPts;
	s.ClearCoefficients();
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaVelInit (VelInit)

IN	splS
OUT	velInit
--------------------------------------*/
static int CLuaVelInit(lua_State* l)
{
	return vec::LuaPushVec(l, CheckLuaTo(1)->s.vi);
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaSetVelInit (SetVelInit)

IN	splS, velInit
--------------------------------------*/
static int CLuaSetVelInit(lua_State* l)
{
	CheckLuaTo(1)->s.vi = PointChecker<u>::CheckLuaToVal(l, 2);
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaVelFinal (VelFinal)

IN	splS
OUT	velFinal
--------------------------------------*/
static int CLuaVelFinal(lua_State* l)
{
	return vec::LuaPushVec(l, CheckLuaTo(1)->s.vf);
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaSetVelFinal (SetVelFinal)

IN	splS, velFinal
--------------------------------------*/
static int CLuaSetVelFinal(lua_State* l)
{
	CheckLuaTo(1)->s.vf = PointChecker<u>::CheckLuaToVal(l, 2);
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaSetTimesToChordLengths (SetTimesToChordLengths)

IN	splS
--------------------------------------*/
static int CLuaSetTimesToChordLengths(lua_State* l)
{
	CheckLuaTo(1)->s.SetTimesToChordLengths();
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaCalculateCoefficients (CalculateCoefficients)

IN	splS
--------------------------------------*/
static int CLuaCalculateCoefficients(lua_State* l)
{
	CheckLuaTo(1)->s.CalculateCoefficients();
	return 0;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaPos (Pos)

IN	splS, iPtStart, nTime
OUT	pos, nTime
--------------------------------------*/
static int CLuaPos(lua_State* l)
{
	return LuaPushPoint(l, CheckLuaTo(1)->s.Pos(scr::CheckLuaToUnsigned(l, 2),
		luaL_checknumber(l, 3)));
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaPosSearch (PosSearch)

IN	splS, nTime
OUT	pos, nTime
--------------------------------------*/
static int CLuaPosSearch(lua_State* l)
{
	return LuaPushPoint(l, CheckLuaTo(1)->s.PosSearch(luaL_checknumber(l, 2)));
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaVel (Vel)

IN	splS, iPtStart, nTime
OUT	vel, nTime
--------------------------------------*/
static int CLuaVel(lua_State* l)
{
	return 0;
	/*
	return LuaPushPoint(l, CheckLuaTo(1)->s.Vel(scr::CheckLuaToUnsigned(l, 2),
		luaL_checknumber(l, 3)));
	*/
	// FIXME
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaVelSearch (VelSearch)

IN	splS, nTime
OUT	vel, nTime
--------------------------------------*/
static int CLuaVelSearch(lua_State* l)
{
	return 0;
	//return LuaPushPoint(l, CheckLuaTo(1)->s.VelSearch(luaL_checknumber(l, 2)));
	// FIXME
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaAcc (Acc)

IN	splS, iPtStart, nTime
OUT	acc, nTime
--------------------------------------*/
static int CLuaAcc(lua_State* l)
{
	return 0;
	/*return LuaPushPoint(l, CheckLuaTo(1)->s.Acc(scr::CheckLuaToUnsigned(l, 2),
		luaL_checknumber(l, 3)));*/
	// FIXME
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaAccSearch (AccSearch)

IN	splS, nTime
OUT	acc, nTime
--------------------------------------*/
static int CLuaAccSearch(lua_State* l)
{
	return 0;
	//return LuaPushPoint(l, CheckLuaTo(1)->s.AccSearch(luaL_checknumber(l, 2)));
	// FIXME
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaNumPtsCalc (NumPtsCalc)

IN	splS
OUT	iNumPtsCalc
--------------------------------------*/
static int CLuaNumPtsCalc(lua_State* l)
{
	lua_pushinteger(l, CheckLuaTo(1)->s.NumPtsCalc());
	return 1;
}

/*--------------------------------------
LUA	comlua::LuaSpline::CLuaNumPolynomials (NumPolynomials)

IN	splS
OUT	iNumPolynomials
--------------------------------------*/
static int CLuaNumPolynomials(lua_State* l)
{
	lua_pushinteger(l, CheckLuaTo(1)->s.NumPolynomials());
	return 1;
}

private:

/*--------------------------------------
	comlua::LuaSpline::LuaPushPoint
--------------------------------------*/
static int LuaPushPoint(lua_State* l, const typename com::Spline<u>::spline_point& pt)
{
	vec::LuaPushVec(l, pt.val);
	lua_pushnumber(l, pt.time);
	
	if(COM_EQUAL_TYPES(u, com::Vec3))
		return 4;
	else if(COM_EQUAL_TYPES(u, com::Vec2))
		return 3;
	else
		return 2;
}

/*======================================
	comlua::LuaSpline::PointChecker
======================================*/
template <typename v>
class PointChecker;

template <>
class PointChecker<com::Vec3>
{
public:
	static com::Vec3 CheckLuaToVal(lua_State* l, int index)
	{
		return vec::CheckLuaToVec(l, index, index + 1, index + 2);
	}

	static com::Spline<com::Vec3>::spline_point CheckLuaToPoint(lua_State* l, int index)
	{
		com::Spline<com::Vec3>::spline_point pt = {
			vec::CheckLuaToVec(l, index, index + 1, index + 2),
			luaL_checknumber(l, index + 3)
		};

		return pt;
	}
};

template <>
class PointChecker<com::Vec2>
{
public:
	static com::Vec2 CheckLuaToVal(lua_State* l, int index)
	{
		return vec::CheckLuaToVec(l, index, index + 1);
	}

	static com::Spline<com::Vec2>::spline_point CheckLuaToPoint(lua_State* l, int index)
	{
		com::Spline<com::Vec2>::spline_point pt = {
			vec::CheckLuaToVec(l, index, index + 1),
			luaL_checknumber(l, index + 2)
		};

		return pt;
	}
};

}; // comlua::LuaSpline

}

#endif