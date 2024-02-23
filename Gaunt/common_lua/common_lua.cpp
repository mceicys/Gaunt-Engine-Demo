// common_lua.cpp
// Martynas Ceicys

#include "common_lua.h"
#include "../../GauntCommon/vec.h"
#include "spline_lua.h"

const char* const comlua::LuaSpline<com::Vec3>::METATABLE = "metaSpline3";

/*--------------------------------------
	comlua::Init
--------------------------------------*/
void comlua::Init()
{
	luaL_Reg spl3Regs[] = {
		{"Pt", LuaSpline<com::Vec3>::CLuaPt},
		{"NumPts", LuaSpline<com::Vec3>::CLuaNumPts},
		{"SetPt", LuaSpline<com::Vec3>::CLuaSetPt},
		{"SetNumPts", LuaSpline<com::Vec3>::CLuaSetNumPts},
		{"VelInit", LuaSpline<com::Vec3>::CLuaVelInit},
		{"SetVelInit", LuaSpline<com::Vec3>::CLuaSetVelInit},
		{"VelFinal", LuaSpline<com::Vec3>::CLuaVelFinal},
		{"SetVelFinal", LuaSpline<com::Vec3>::CLuaSetVelFinal},
		{"SetTimesToChordLengths", LuaSpline<com::Vec3>::CLuaSetTimesToChordLengths},
		{"CalculateCoefficients", LuaSpline<com::Vec3>::CLuaCalculateCoefficients},
		{"Pos", LuaSpline<com::Vec3>::CLuaPos},
		{"PosSearch", LuaSpline<com::Vec3>::CLuaPosSearch},
		{"Vel", LuaSpline<com::Vec3>::CLuaVel},
		{"VelSearch", LuaSpline<com::Vec3>::CLuaVelSearch},
		{"Acc", LuaSpline<com::Vec3>::CLuaAcc},
		{"AccSearch", LuaSpline<com::Vec3>::CLuaAccSearch},
		{"NumPtsCalc", LuaSpline<com::Vec3>::CLuaNumPtsCalc},
		{"NumPolynomials", LuaSpline<com::Vec3>::CLuaNumPolynomials},
		{"__gc", LuaSpline<com::Vec3>::CLuaDelete},
		{0, 0}
	};

	LuaSpline<com::Vec3>::RegisterMetatable(spl3Regs, 0);

	luaL_Reg regs[] = {
		{"Spline3", LuaSpline<com::Vec3>::CLuaCreate},
		
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		spl3Regs,
		0
	};

	const char* prefixes[] = {
		"Spl3",
		0
	};

	scr::RegisterLibrary(scr::state, "gcom", regs, 0, 0, metas, prefixes);
}