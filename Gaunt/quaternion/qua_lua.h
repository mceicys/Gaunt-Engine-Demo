// qua_lua.h -- Lua stuff for common quaternions
// Martynas Ceicys

#ifndef QUA_LUA_H
#define QUA_LUA_H

#include "../../GauntCommon/qua.h"
#include "../script/script.h"

namespace qua
{
	void		Init();

	void		LuaPushQua(lua_State* l, const com::Qua& q);
	com::Qua	LuaToQua(lua_State* l, int i0, int i1, int i2, int i3);
	com::Qua	CheckLuaToQua(lua_State* l, int i0, int i1, int i2, int i3);
}

#endif