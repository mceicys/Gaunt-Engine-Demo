// script.h -- Lua stuff
// Martynas Ceicys

#ifndef SCRIPT_H
#define SCRIPT_H

extern "C"
{
	#include "../lua/lua.h"
	#include "../lua/lauxlib.h"
}

#include "../console/option.h"
#include "../../GauntCommon/io.h"

namespace scr
{

extern lua_State* state;
extern con::Option
	outputExpansion;

void		Init();
int			PushScript(lua_State* l, const char* file, int* goodOut = 0);
bool		EnsureScript(lua_State* l, const char* file);
int			LoadString(lua_State* l, const char* chunk, const char* chunkName,
			bool forceExpand);
int			DoString(lua_State* l, const char* chunk, const char* chunkName, bool forceExpand,
			bool inDebugEnv);
bool		CheckMetatable(lua_State* l, int index, const char* name);
int			Call(lua_State* l, int args, int results, bool trace = true);
void		EnsureStack(lua_State* l, int n, const char* msg = 0);
bool		CopyTable(lua_State* l, int destIndex, int srcIndex, bool clearDest);
void		CheckTable(lua_State* l, int index);
bool		OptionalTable(lua_State* l, int index);
int			RawGetField(lua_State* l, int index, const char* k);
void		RawSetField(lua_State* l, int index, const char* k);
void		TypeError(lua_State* l, int index, const char* expected);
void		LogExpansionError(const char* chunk, const char* chunkName);
int			PushLocal(lua_State* l, const lua_Debug* ar, const char* name);
int			PushUpvalue(lua_State* l, int funcIndex, const char* name);
int			PushUpvalue(lua_State* l, lua_Debug* ar, const char* name);
lua_Integer	CheckLuaToUnsigned(lua_State* l, int index);

struct constant
{
	char*		name;
	lua_Integer	value;
};

void	RegisterLibrary(lua_State* l, const char* name, const luaL_Reg* regs,
		const constant* constants, int numFields, const luaL_Reg** metaRegs,
		const char** metaPrefixes);
int		RegisterMetatable(lua_State* l, const char* name, const luaL_Reg* regs,
		const constant* constants, bool indexSelf = true);

}

#endif