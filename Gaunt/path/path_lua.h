// path_lua.h
// Martynas Ceicys

#ifndef PATH_LUA_H
#define PATH_LUA_H

#include "../script/script.h"

namespace pat
{
	// FLIGHT MAP LUA
	int BestFlightMap(lua_State* l);

	int FMapHull(lua_State* l);
	// FIXME: node info funcs
	int FMapDraw(lua_State* l);

	// FLIGHT SEARCH LUA
	int InstantFlightPathToPoint(lua_State* l);

	// FLIGHT NAVIGATOR LUA
	int CreateFlightNavigator(lua_State* l);

	// metaFlightNavigator
	int FNavMap(lua_State* l);
	int FNavSetMap(lua_State* l);
	int FNavDeveloping(lua_State* l);
	int FNavSearching(lua_State* l);
	int FNavClearAttempt(lua_State* l);
	int FNavFindPath(lua_State* l);
	int FNavContinueSearch(lua_State* l);
	int FNavDrawSearch(lua_State* l);
}

#endif