// vec_lua.h -- Lua stuff for common vectors
// Martynas Ceicys

#ifndef VEC_LUA_H
#define VEC_LUA_H

#include "../../GauntCommon/vec.h"
#include "../script/script.h"

namespace vec
{
	void		Init();

	int			LuaPushVec(lua_State* l, const com::Vec2& v);
	com::Vec2	LuaToVec(lua_State* l, int indexX, int indexY);
	com::Vec2	CheckLuaToVec(lua_State* l, int indexX, int indexY);

	int			LuaPushVec(lua_State* l, const com::Vec3& v);
	com::Vec3	LuaToVec(lua_State* l, int indexX, int indexY, int indexZ);
	com::Vec3	CheckLuaToVec(lua_State* l, int indexX, int indexY, int indexZ);
}

#endif