// script.cpp
// Martynas Ceicys

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "script.h"
#include "../console/console.h"
#include "../console/option.h"
#include "../../GauntCommon/io.h"
#include "../input/input.h"
#include "../mod/mod.h"
#include "../wrap/wrap.h"

extern "C"
{
	#include "../lua/lualib.h"
	#include "../../lua_fake_vector/lfv.h"
}

namespace scr
{
	lua_State* state;

	size_t		NumRegs(const luaL_Reg* regs);
	size_t		NumConstants(const constant* constants);
	size_t		NumDups(const luaL_Reg** metaRegs);
	size_t		NumLibOptions(const char* prefix);
	void		LuaSetConstants(lua_State* l, const constant* constants);
	void		LuaSetDups(lua_State* l, const luaL_Reg** metaRegs, const char** metaPrefixes);
	void		LuaSetLibOptions(lua_State* l, const char* prefix);
	void		LuaSetFields(lua_State* l, int numFields);
	void		SetOutputExpansion(con::Option& opt, float set);

	// LUA
	int			EnsureScript(lua_State* l);
	int			LoadString(lua_State* l);
	int			Debugging(lua_State* l);
	int			HandleError(lua_State* l);
	int			Panic(lua_State* l);

	// BREAKPOINTS
	bool		bpContinue = false, bpStep = false, bpConsole = true;
	lua_Debug*	bpar = 0; // 0 if not breaking

	int			Breakpoint(lua_State* l);
	void		Hook(lua_State* l, lua_Debug *ar);
	const char*	HookString(int ev);
	int			Continue(lua_State* l);
	int			Step(lua_State* l);
	int			Locals(lua_State* l);

	// DEBUG ENVIRONMENT
	int			debugEnvRef, metaDebugEnvRef;

	int			DebugEnvIndex(lua_State* l);
	int			DebugEnvNewIndex(lua_State* l);

	con::Option
		outputExpansion("scr_output_expansion", false, SetOutputExpansion);
}

/*--------------------------------------
	scr::Init

Creates lua_State and loads certain standard libraries.
--------------------------------------*/
void scr::Init()
{
	state = luaL_newstate();

	if(!state)
		WRP_FATAL("Failed to create Lua state");

	lua_atpanic(state, Panic);

	lua_createtable(state, 0, 0);
	RawSetField(state, LUA_REGISTRYINDEX, "SCR_LOADED"); // registry.SCR_LOADED = {}

#ifdef SCR_UNSAFE
	luaL_openlibs(state);
#else
	const luaL_Reg LIBS[] = {
		{"_G", luaopen_base}, // This loads unsafe functions unless Lua source is modified
		{LUA_TABLIBNAME, luaopen_table},
		{LUA_STRLIBNAME, luaopen_string},
		{LUA_MATHLIBNAME, luaopen_math},
		{0, 0}
	};

	for(const luaL_Reg* lib = LIBS; lib->func; lib++)
	{
		luaL_requiref(state, lib->name, lib->func, 1);
		lua_pop(state, 1);
	}

	// Dumb reminder to remove these if they've been loaded
	const char* UNSAFE_GLOBALS[] = {
		"collectgarbage",
		"dofile",
		"loadfile",
		"load",
		"require",
		"coroutine",
		"io",
		"os",
		"debug",
		"package",
		0
	};

	for(const char** name = UNSAFE_GLOBALS; *name; name++)
	{
		if(lua_getglobal(state, *name) != LUA_TNIL)
			wrp::FatalF("Restricted global '%s' was loaded", *name);

		lua_pop(state, 1);
	}
#endif

	luaL_Reg regs[] =
	{
		{"EnsureScript", EnsureScript},
		{"LoadString", LoadString},
		{"Debugging", Debugging},
		{"Breakpoint", Breakpoint},
		{0, 0}
	};

	const int NUM_FIELDS = 1;
	lua_pushliteral(state, "loaded");
	RawGetField(state, LUA_REGISTRYINDEX, "SCR_LOADED"); // gscr.loaded = registry.SCR_LOADED
	RegisterLibrary(state, "gscr", regs, 0, NUM_FIELDS, 0, 0);

	outputExpansion.SetValue(outputExpansion.Float());

	lua_pushcfunction(state, Continue); con::CreateCommand("cont");
	lua_pushcfunction(state, Step); con::CreateCommand("step");
	lua_pushcfunction(state, Locals); con::CreateCommand("locals");

	in::EnsureAction("DEBUG_CONTINUE");
	in::EnsureAction("DEBUG_STEP");

	// Create debug environment for breakpoints
	luaL_Reg debugEnvRegs[] =
	{
		{"__index", DebugEnvIndex},
		{"__newindex", DebugEnvNewIndex},
		{0, 0}
	};

	// Environment's metatable
	metaDebugEnvRef = RegisterMetatable(state, "metaDebugEnv", debugEnvRegs, 0, false);

	// Actual environment table
	lua_newtable(state);
	lua_rawgeti(state, LUA_REGISTRYINDEX, metaDebugEnvRef);
	lua_setmetatable(state, -2);
	debugEnvRef = luaL_ref(state, LUA_REGISTRYINDEX);
}

/*--------------------------------------
	scr::PushScript

Limited imitation of require() that takes a file name instead of a module name and stores loaded
modules in a Lua registry table SCR_LOADED. Returns 1 and pushes SCR_LOADED[file]. If good is
given, sets it to the Lua boolean value of SCR_LOADED[file].
--------------------------------------*/
int scr::PushScript(lua_State* l, const char* file, int* good)
{
	EnsureStack(l, 5);

	if(RawGetField(l, LUA_REGISTRYINDEX, "SCR_LOADED") != LUA_TTABLE)
		WRP_FATAL("Loaded script table does not exist");

	RawGetField(l, -1, file); // SCR_LOADED[file]

	if(lua_toboolean(l, -1)) // Already loaded
	{
		lua_remove(l, -2); // Remove table
		if(good)
			*good = 1;
		return 1;
	}

	if(good)
		*good = 0;

	const char *err = 0, *path = mod::Path(0, file, err);

	if(err)
	{
		CON_ERRORF("%s '%s'", err, file);
		lua_remove(l, -2); // Remove table
		return 1;
	}

	int res = lfvLoadFile(l, path, 0);
	LogExpansionError(file, 0);

	if(res != LUA_OK)
	{
		CON_ERRORF("%s", lua_tostring(l, -1));
		lua_pop(l, 1); // Pop error
		lua_remove(l, -2); // Remove table
		return 1;
	}

	lua_pushstring(l, file); // Arguments
	lua_pushstring(l, path);

	if(Call(l, 2, 1)) // Call file
	{
		lua_remove(l, -2); // Remove table
		return 1;
	}

	lua_remove(l, -2); // Remove old field

	if(lua_type(l, -1) != LUA_TNIL) // File returned something
	{
		lua_pushvalue(l, -1); // Copy result to return
		RawSetField(l, -3, file); // SCR_LOADED[file] = result
	}
	else
	{
		lua_pop(l, 1); // Pop nil result

		if(RawGetField(l, -1, file) == LUA_TNIL) // File did not set SCR_LOADED[file]
		{
			lua_pop(l, 1); // Pop nil field
			lua_pushboolean(l, 1);
			lua_pushvalue(l, -1); // Copy bool to return
			RawSetField(l, -3, file); // SCR_LOADED[file] = true
		}
	}
	
	lua_remove(l, -2); // Remove table

	if(good)
		*good = lua_toboolean(l, -1);

	return 1;
}

/*--------------------------------------
	scr::EnsureScript
--------------------------------------*/
bool scr::EnsureScript(lua_State* l, const char* file)
{
	int good;
	PushScript(l, file, &good);
	lua_pop(l, 1);
	return good != 0;
}

/*--------------------------------------
	scr::LoadString
--------------------------------------*/
int scr::LoadString(lua_State* l, const char* chunk, const char* chunkName, bool forceExpand)
{
	int ret = lfvLoadString(l, chunk, chunkName, forceExpand);
	LogExpansionError(chunk, chunkName);
	return ret;
}

/*--------------------------------------
	scr::DoString
--------------------------------------*/
int scr::DoString(lua_State* l, const char* chunk, const char* chunkName, bool forceExpand,
	bool inDebugEnv)
{
	if(int ret = LoadString(l, chunk, chunkName, forceExpand))
	{
		const char* str = lua_tostring(scr::state, -1);
		con::AlertF("%s", str);
		lua_pop(scr::state, 1);
		return ret;
	}

	if(bpar && inDebugEnv)
	{
		// Execute this string in the debug environment
		lua_rawgeti(l, LUA_REGISTRYINDEX, debugEnvRef);
		const char* upvalue = lua_setupvalue(l, -2, 1);

		if(!upvalue)
		{
			con::LogF("Could not set chunk's env to debug env");
			lua_pop(l, 1);
		}
	}

	return Call(l, 0, LUA_MULTRET);
}

/*--------------------------------------
	scr::CheckMetatable

Returns true if metatable of object at index and metatable in registry with name are equal.
Returns false if not, or if the object does not have a metatable.
--------------------------------------*/
bool scr::CheckMetatable(lua_State* l, int index, const char* name)
{
	if(!lua_getmetatable(l, index))
		return false;

	luaL_getmetatable(l, name);

	if(lua_rawequal(l, -1, -2))
	{
		lua_pop(l, 2);
		return true;
	}
	else
	{
		lua_pop(l, 2);
		return false;
	}
}

/*--------------------------------------
	scr::Call

Like lua_pcall but logs and pops errors. If trace is true, error string contains a trace back.
--------------------------------------*/
int scr::Call(lua_State* l, int args, int results, bool trace)
{
	int handler = 0;

	if(trace)
	{
		EnsureStack(l, 1);
		handler = lua_gettop(l) - args;
		lua_pushcfunction(l, HandleError);
		lua_insert(l, handler);
	}

	if(int ret = lua_pcall(l, args, results, handler))
	{
		const char* str = lua_tostring(l, -1);
		con::AlertF("%s", str);
		lua_pop(l, 1 + trace); // Pop string (and handler if tracing)
		return ret;
	}

	if(trace)
		lua_remove(l, handler);

	return LUA_OK;
}

/*--------------------------------------
	scr::EnsureStack

FIXME: Unclear about when to call this
	CLua function calls are given LUA_MINSTACK "extra slots"
		This probably means beyond the given arguments (since the script can give any number),
		which means no function returning less than LUA_MINSTACK + 1 results needs to check the
		stack; verify this and check all EnsureStack calls
--------------------------------------*/
void scr::EnsureStack(lua_State* l, int n, const char* msg)
{
	if(!lua_checkstack(l, n))
	{
		static const char* ERR = "Reached Lua stack limit";

		if(msg)
			WRP_FATALF("%s (%s)", ERR, msg);
		else
			WRP_FATALF("%s", ERR);
	}
}

/*--------------------------------------
	scr::CopyTable

Does a shallow copy.

FIXME: optional deep copy
--------------------------------------*/
bool scr::CopyTable(lua_State* l, int destIndex, int srcIndex, bool clearDest)
{
	destIndex = lua_absindex(l, destIndex);
	srcIndex = lua_absindex(l, srcIndex);

	if(lua_type(l, destIndex) != LUA_TTABLE)
	{
		CON_ERROR("Destination value is not a table");
		return false;
	}

	if(lua_type(l, srcIndex) != LUA_TTABLE)
	{
		CON_ERROR("Source value is not a table");
		return false;
	}

	scr::EnsureStack(l, 4);

	if(clearDest)
	{
		lua_pushnil(l);
		while(lua_next(l, destIndex))
		{
			lua_pop(l, 1); // Pop value
			lua_pushvalue(l, -1); // Copy key
			lua_pushnil(l);
			lua_rawset(l, destIndex); // Set value to nil
		}
	}

	lua_pushnil(l);
	while(lua_next(l, srcIndex))
	{
		lua_pushvalue(l, -2); // Copy key
		lua_insert(l, -2);
		lua_rawset(l, destIndex);
	}

	return true;
}

/*--------------------------------------
	scr::CheckTable
--------------------------------------*/
void scr::CheckTable(lua_State* l, int index)
{
	luaL_argcheck(l, lua_istable(l, index), index, "Expected table");
}

/*--------------------------------------
	scr::OptionalTable
--------------------------------------*/
bool scr::OptionalTable(lua_State* l, int index)
{
	int t = lua_type(l, index);

	if(t == LUA_TTABLE)
		return true;
	else if(t == LUA_TNIL)
		return false;
	else
		luaL_argerror(l, index, "Expected table or nil");

	return false;
}

/*--------------------------------------
	scr::RawGetField
--------------------------------------*/
int scr::RawGetField(lua_State* l, int index, const char* k)
{
	EnsureStack(l, 1);
	index = lua_absindex(l, index);
	lua_pushstring(l, k);
	return lua_rawget(l, index);
}

/*--------------------------------------
	scr::RawSetField
--------------------------------------*/
void scr::RawSetField(lua_State* l, int index, const char* k)
{
	EnsureStack(l, 1);
	index = lua_absindex(l, index);
	lua_pushstring(l, k);
	lua_insert(l, -2);
	lua_rawset(l, index);
}

/*--------------------------------------
	scr::TypeError
--------------------------------------*/
void scr::TypeError(lua_State* l, int index, const char* expected)
{
	index = lua_absindex(scr::state, index);
	const char* type;

	if(luaL_getmetafield(scr::state, index, "__name") == LUA_TSTRING)
		type = lua_tostring(scr::state, -1);
	else
		type = luaL_typename(scr::state, index);

	luaL_argerror(scr::state, index, lua_pushfstring(scr::state,
		"%s expected, got %s", expected, type));
}

/*--------------------------------------
	scr::LogExpansionError
--------------------------------------*/
void scr::LogExpansionError(const char* chunk, const char* chunkName)
{
	if(lfvError())
	{
		con::AlertF("Expansion error ('%s' ln %u): %s", chunkName ? chunkName : chunk,
			lfvErrorLine(), lfvError());
	}
}

/*--------------------------------------
	scr::PushLocal

Checks all locals for name and returns highest index matching it.
--------------------------------------*/
int scr::PushLocal(lua_State* l, const lua_Debug* ar, const char* name)
{
	const char* got = 0;
	int last = 0;

	for(int n = 1; got = lua_getlocal(l, ar, n); n++)
	{
		if(!strcmp(got, name))
			last = n;

		lua_pop(l, 1);
	}

	if(last)
		lua_getlocal(l, ar, last);

	return last;
}

/*--------------------------------------
	scr::PushUpvalue
--------------------------------------*/
int scr::PushUpvalue(lua_State* l, int funcIndex, const char* name)
{
	funcIndex = lua_absindex(l, funcIndex);
	const char* got = 0;

	for(int n = 1; got = lua_getupvalue(l, funcIndex, n); n++)
	{
		if(strcmp(got, name))
			lua_pop(l, 1); // Wrong upvalue, pop
		else
			return n;
	}

	return 0;
}

int scr::PushUpvalue(lua_State* l, lua_Debug* ar, const char* name)
{
	// Push top function of activation record
	if(!lua_getinfo(l, "f", ar))
	{
		con::AlertF("Could not get activation record's top function from lua_getinfo");
		return 0;
	}
	
	if(int n = PushUpvalue(l, -1, name))
	{
		lua_remove(l, -2); // Remove top function
		return n;
	}
	
	lua_pop(l, 1); // Pop top function
	return 0;
}

/*--------------------------------------
	scr::CheckLuaToUnsigned
--------------------------------------*/
lua_Integer scr::CheckLuaToUnsigned(lua_State* l, int index)
{
	lua_Integer i = luaL_checkinteger(l, index);

	if(i < 0)
		luaL_argerror(l, index, "Expected unsigned");

	return i;
}

/*--------------------------------------
	scr::RegisterLibrary

regs, constants, and metaRegs must have a sentinel element in which all values are 0. constants
can be 0. numFields key-value pairs are popped from the stack and added to the library. metaRegs
is an array of reg arrays for metatables that should have their functions also registered to
this library with prefixes given in metaPrefixes; names starting with '_' are not duplicated.
Console options starting with name + 1 have getters and setters registered to the library.
--------------------------------------*/
void scr::RegisterLibrary(lua_State* l, const char* name, const luaL_Reg* regs,
	const constant* constants, int numFields, const luaL_Reg** metaRegs,
	const char** metaPrefixes)
{
	const char* optionPrefix = name + 1; // Assuming name starts with a 'g' prefix

	size_t numRegs = NumRegs(regs), numConstants = NumConstants(constants),
		numDups = NumDups(metaRegs), numLibOptionRegs = NumLibOptions(optionPrefix) * 2;

	lua_createtable(l, 0, numRegs + numConstants + numDups + numLibOptionRegs + numFields);

	if(regs)
		luaL_setfuncs(l, regs, 0);

	LuaSetConstants(l, constants);
	LuaSetFields(l, numFields);
	LuaSetDups(l, metaRegs, metaPrefixes);
	LuaSetLibOptions(l, optionPrefix);
	lua_setglobal(l, name);
}

/*--------------------------------------
	scr::RegisterMetatable

Same as RegisterLibrary but creates a metatable that's put in the registry by name and with an
integer reference. Returns the integer reference.
--------------------------------------*/
int scr::RegisterMetatable(lua_State* l, const char* name, const luaL_Reg* regs,
	const constant* constants, bool indexSelf)
{
	size_t numRegs = NumRegs(regs), numConstants = NumConstants(constants);

	luaL_newmetatable(l, name);

	if(regs)
		luaL_setfuncs(l, regs, 0);

	LuaSetConstants(l, constants);

	// Hide metatable
	lua_pushnil(l);
	lua_setfield(l, -2, "__metatable");

	if(indexSelf)
	{
		lua_pushvalue(l, -1); // Copy metatable
		lua_setfield(l, -1, "__index"); // Tables inherit metatable stuff
	}

	return luaL_ref(l, LUA_REGISTRYINDEX); // Pop and reference metatable
}

/*--------------------------------------
	scr::NumRegs
--------------------------------------*/
size_t scr::NumRegs(const luaL_Reg* regs)
{
	if(!regs)
		return 0;

	size_t numRegs = 0;
	for(; regs[numRegs].name || regs[numRegs].func; numRegs++);
	return numRegs;
}

/*--------------------------------------
	scr::NumConstants
--------------------------------------*/
size_t scr::NumConstants(const constant* constants)
{
	if(!constants)
		return 0;

	size_t numConstants = 0;
	for(; constants[numConstants].name || constants[numConstants].value; numConstants++);
	return numConstants;
}

/*--------------------------------------
	scr::NumDups
--------------------------------------*/
size_t scr::NumDups(const luaL_Reg** metaRegs)
{
	if(!metaRegs)
		return 0;

	size_t numDups = 0;

	for(size_t i = 0; metaRegs[i]; i++)
	{
		const luaL_Reg* meta = metaRegs[i];

		for(size_t j = 0; meta[j].name; j++)
		{
			if(meta[j].name[0] != '_')
				numDups++;
		}
	}
	
	return numDups;
}

/*--------------------------------------
	scr::NumLibOptions
--------------------------------------*/
size_t scr::NumLibOptions(const char* prefix)
{
	if(!prefix)
		return 0;

	size_t preLen = strlen(prefix);
	size_t numLibOptions = 0;

	for(size_t i = 0; i < con::Option::NumOptions(); i++)
	{
		if(!strncmp(con::Option::Options()[i]->Name(), prefix, preLen))
			numLibOptions++;
	}

	return numLibOptions;
}

/*--------------------------------------
	scr::LuaSetConstants
--------------------------------------*/
void scr::LuaSetConstants(lua_State* l, const constant* constants)
{
	if(!constants)
		return;

	for(size_t i = 0; constants[i].name || constants[i].value; i++)
	{
		lua_pushstring(l, constants[i].name);
		lua_pushinteger(l, constants[i].value);
		lua_rawset(l, -3);
	}
}

/*--------------------------------------
	scr::LuaSetDups
--------------------------------------*/
void scr::LuaSetDups(lua_State* l, const luaL_Reg** metaRegs, const char** metaPrefixes)
{
	if(!metaRegs)
		return;

	com::Arr<char> buf(32);

	for(size_t i = 0; metaRegs[i]; i++)
	{
		const luaL_Reg* meta = metaRegs[i];
		const char* prefix = metaPrefixes[i];
		size_t preLen = strlen(prefix);
		buf.Ensure(preLen + 1);
		strcpy(buf.o, prefix);

		for(size_t j = 0; meta[j].name && meta[j].func; j++)
		{
			if(meta[j].name[0] == '_')
				continue;

			buf.Ensure(preLen + strlen(meta[j].name) + 1);
			strcpy(buf.o + preLen, meta[j].name);
			lua_pushstring(l, buf.o);
			lua_pushcfunction(l, meta[j].func);
			lua_rawset(l, -3);
		}
	}

	buf.Free();
}

/*--------------------------------------
	scr::LuaSetLibOptions
--------------------------------------*/
void scr::LuaSetLibOptions(lua_State* l, const char* prefix)
{
	if(!prefix)
		return;

	com::Arr<char> buf(32);
	const char* const SETTER_PREFIX = "Set";
	const size_t SETTER_PREFIX_SIZE = strlen(SETTER_PREFIX);
	strcpy(buf.o, SETTER_PREFIX);
	size_t preLen = strlen(prefix);
	size_t numLibOptions = 0;

	for(size_t i = 0; i < con::Option::NumOptions(); i++)
	{
		con::Option& opt = *con::Option::Options()[i];
		const char* name = opt.Name();

		if(strncmp(name, prefix, preLen))
			continue;

		/* FIXME: converting from underscore to camel case is dumb and won't work right for
		names w/ acronyms; make console command interpretation case insensitive and name options
		with camel case so this conversion isn't needed */
		buf.Ensure(SETTER_PREFIX_SIZE + strlen(name) + 1);
		bool up = true;
		size_t len = SETTER_PREFIX_SIZE;

		for(size_t j = preLen; name[j]; j++)
		{
			if(name[j] == '_')
			{
				up = true;
				continue;
			}

			buf[len++] = up ? toupper(name[j]) : name[j];
			up = false;
		}

		buf[len] = 0;
		lua_pushstring(l, buf.o + SETTER_PREFIX_SIZE);
		opt.LuaPushGetter(l);
		lua_rawset(l, -3);
		lua_pushstring(l, buf.o);
		opt.LuaPushSetter(l);
		lua_rawset(l, -3);
	}

	buf.Free();
}

/*--------------------------------------
	scr::LuaSetFields

numFields key-value pairs are consumed and assigned to the table following at the stack's top.
--------------------------------------*/
void scr::LuaSetFields(lua_State* l, int numFields)
{
	if(!numFields)
		return;

	int table = numFields * -2 - 1;
	lua_insert(l, table); // Move table below pairs

	for(; table < -2; table += 2)
		lua_rawset(l, table);
}

/*--------------------------------------
	scr::SetOutputExpansion

FIXME: Output a different file for each load? Expanded non-file strings can all be put into one
	general file, though
--------------------------------------*/
void scr::SetOutputExpansion(con::Option& opt, float set)
{
	if(set)
		lfvSetDebugPath("expanded_scripts.txt", 1, 0, 1);
	else
		lfvSetDebugPath(0, 0, 0, 0);

	opt.ForceValue(set);
}

/*
################################################################################################


	LUA


################################################################################################
*/

/*--------------------------------------
LUA	scr::EnsureScript

IN	sFile
OUT	module
--------------------------------------*/
int scr::EnsureScript(lua_State* l)
{
	const char* file = luaL_checkstring(l, 1);
	PushScript(l, file);
	return 1;
}

/*--------------------------------------
LUA	scr::LoadString

IN	sChunk, [sChunkName], [bForceExpand]
OUT	CompiledChunk | (nil, sError)
--------------------------------------*/
int scr::LoadString(lua_State* l)
{
	const char* chunk = luaL_checkstring(l, 1);
	const char* chunkName = luaL_optstring(l, 2, 0);
	int ret = lfvCLuaLoadString(l);
	LogExpansionError(chunk, chunkName);
	return ret;
}

/*--------------------------------------
LUA	scr::Debugging

OUT	bDebugging

Returns true if the debug version which loads all the standard Lua libraries is running.

FIXME: rename, thought this was to check if console debug mode is on

FIXME: fix Lua debugging
	Statically link LuaJIT or Lua 5.3 and make a proxy DLL for debugging if needed
		DLL must be proxy to avoid heap corruption
		studio.zerobrane.com/doc-lua53-debugging
	Check if debugger is calling functions with the same lua_State (scr::state)
		If not, Resource functions will need to take a lua_State*
			Not sure if this will actually work; I don't know what this other state is
--------------------------------------*/
int scr::Debugging(lua_State* l)
{
	#ifdef SCR_UNSAFE
	lua_pushboolean(l, true);
	#else
	lua_pushboolean(l, false);
	#endif

	return 1;
}

/*--------------------------------------
LUA	scr::HandleError

IN	sErr
OUT	sTrace

Pushes error string with traceback.
--------------------------------------*/
int scr::HandleError(lua_State* l)
{
	const char* err = lua_tostring(l, 1);
	luaL_traceback(l, l, err, 1);
	return 1;
}

/*--------------------------------------
LUA	scr::Panic
--------------------------------------*/
int scr::Panic(lua_State* l)
{
	const char* err = lua_tostring(l, -1);
	wrp::FatalF("Lua panic (%s)", err);
	return 0;
}

/*
################################################################################################


	BREAKPOINTS


################################################################################################
*/

/*--------------------------------------
LUA	scr::Breakpoint
--------------------------------------*/
int scr::Breakpoint(lua_State* l)
{
	if(con::debug.Bool() == true && lua_gethookmask(l) == 0)
		lua_sethook(l, Hook, LUA_MASKRET, 0); // Will hook before returning from Breakpoint
	
	return 0;
}

/*--------------------------------------
	scr::Hook

FIXME: save source files so the current line can be logged
FIXME: commands to inspect local vars
FIXME: step into/out commands
FIXME: runtime breakpoint adding?
FIXME: would be better if game script's actions weren't affected by inputs being cleared in ForceFrame
	another event set? (tick, frame, forced)
--------------------------------------*/
void scr::Hook(lua_State* l, lua_Debug *ar)
{
	lua_sethook(l, Hook, 0, 0);
	bpContinue = bpStep = false;
	int ev = ar->event;

	if(ev != LUA_HOOKRET && ev != LUA_HOOKLINE)
		luaL_error(l, "Unexpected hook event: %s", HookString(ev));

	if(!lua_getstack(l, ev == LUA_HOOKRET ? 1 : 0, ar))
		luaL_error(l, "lua_getstack error");
	
	if(!lua_getinfo(l, "Sln", ar))
		luaL_error(l, "lua_getinfo error");

	con::LogF("*ln %d%s; %s; %s", ar->currentline, ev == LUA_HOOKRET ? " (bp)" : "", ar->name,
		ar->short_src);

	con::SetOpen(bpConsole);
	bool loop = true;
	bpar = ar;

	while(loop && wrp::ForceFrameStart())
	{
		bpConsole = con::Opened();

		if(in::RepeatedFrame(in::FindAction("DEBUG_CONTINUE")))
			Continue(l);

		if(in::RepeatedFrame(in::FindAction("DEBUG_STEP")))
			Step(l);

		if(bpContinue)
		{
			con::SetOpen(false);
			loop = false;
		}
		else if(bpStep)
		{
			con::SetOpen(false);
			loop = false;
			lua_sethook(l, Hook, LUA_MASKLINE, 0);
		}

		wrp::ForceFrameEnd();
	}

	bpar = 0;
}

/*--------------------------------------
	scr::HookString
--------------------------------------*/
const char* scr::HookString(int ev)
{
	switch(ev)
	{
		case LUA_HOOKCALL: return "call";
		case LUA_HOOKRET: return "ret";
		case LUA_HOOKTAILCALL: return "tail call";
		case LUA_HOOKLINE: return "line";
		case LUA_HOOKCOUNT: return "count";
		default: return "unknown";
	}
}

/*--------------------------------------
LUA	scr::Continue
--------------------------------------*/
int scr::Continue(lua_State* l)
{
	bpContinue = true;
	return 0;
}

/*--------------------------------------
LUA	scr::Step
--------------------------------------*/
int scr::Step(lua_State* l)
{
	bpStep = true;
	return 0;
}

/*--------------------------------------
LUA	scr::Locals
--------------------------------------*/
int scr::Locals(lua_State* l)
{
	if(!bpar)
		return 0;

	int n = 1;

	while(const char* name = lua_getlocal(l, bpar, n))
	{
		const char* strVal = lua_tostring(l, -1);

		if(!strVal)
			strVal = luaL_typename(l, -1);

		con::LogF("%d %s = %s", n, name, strVal);
		lua_pop(l, 1);
		n++;
	}

	return 0;
}

/*
################################################################################################


	DEBUG ENVIRONMENT


################################################################################################
*/

/*--------------------------------------
LUA	scr::DebugEnvIndex (__index)

IN	t, key
OUT	value
--------------------------------------*/
int scr::DebugEnvIndex(lua_State* l)
{
	if(bpar && lua_type(l, 2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(l, 2);

		if(PushLocal(l, bpar, key) || PushUpvalue(l, bpar, key))
			return 1;
	}

	lua_rawgeti(l, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // Push globals table
	lua_pushvalue(l, 2); // Push copy of key
	lua_gettable(l, -2); // Pop key, push globals[key]
	return 1;
}

/*--------------------------------------
LUA	scr::DebugEnvNewIndex (__newindex)

IN	t, key, value

FIXME: If Lua functions may consume arguments, this can avoid copying key and value
--------------------------------------*/
int scr::DebugEnvNewIndex(lua_State* l)
{
	if(bpar && lua_type(l, 2) == LUA_TSTRING)
	{
		const char* key = lua_tostring(l, 2);

		if(int n = PushLocal(l, bpar, key))
		{
			lua_pop(l, 1); // Pop local value
			lua_pushvalue(l, 3); // Push copy of value

			if(!lua_setlocal(l, bpar, n)) // Pop value, locals.key = value
				luaL_error(l, "Could not set local");

			return 0;
		}

		// Push top function of debug activation record
		if(!lua_getinfo(l, "f", bpar))
		{
			luaL_error(l, "Could not get activation record's top function");
			return 0;
		}

		if(int n = PushUpvalue(l, -1, key))
		{
			lua_pop(l, 1); // Pop upvalue value
			lua_pushvalue(l, 3); // Push copy of value

			if(!lua_setupvalue(l, -2, n)) // Pop value, upvalues.key = value
				luaL_error(l, "Could not set upvalue");

			return 0;
		}

		lua_pop(l, 1); // Pop top function
	}

	lua_rawgeti(l, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // Push globals table
	lua_pushvalue(l, 2); // Push copy of key
	lua_pushvalue(l, 3); // Push copy of value
	lua_settable(l, -3); // Pop key and value, globals[key] = value
	return 0;
}