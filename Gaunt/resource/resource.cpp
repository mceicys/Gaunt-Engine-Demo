// resource.cpp
// Martynas Ceicys

#include "resource.h"

int res::weakRegistry = LUA_NOREF;

/*--------------------------------------
	res::Init
--------------------------------------*/
void res::Init()
{
	// Create weak registry
	lua_newtable(scr::state);
	lua_pushstring(scr::state, "__mode");
	lua_pushstring(scr::state, "v");
	lua_rawset(scr::state, -3);
	lua_pushvalue(scr::state, -1);
	lua_setmetatable(scr::state, -1);
	weakRegistry = luaL_ref(scr::state, LUA_REGISTRYINDEX);
}