// scene_enttype.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../render/render.h"

/*
################################################################################################


	ENTITY TYPE


################################################################################################
*/

const char* const scn::EntityType::METATABLE = "metaEntityType";

/*--------------------------------------
	scn::EntityType::EntityType

Pops numFuncs values from scr::state's stack. These should be the type's functions or nil pushed
in enum order.
--------------------------------------*/
scn::EntityType::EntityType(const char* name, int numFuncs)
{
	this->name = new char[strlen(name) + 1];
	strcpy(this->name, name);
	int num = COM_MIN(numFuncs, NUM_ENT_FUNCS);
	
	for(int i = 0; i < NUM_ENT_FUNCS; i++)
		ref[i] = LUA_NOREF;

	for(int i = num - 1; i >= 0; i--)
	{
		int t = lua_type(scr::state, -1);

		if(t == LUA_TFUNCTION)
			ref[i] = luaL_ref(scr::state, LUA_REGISTRYINDEX);
		else
		{
			if(t != LUA_TNIL)
				con::AlertF("Invalid func arg %d to entity type %s", i + 1, name);

			lua_pop(scr::state, 1);
		}
	}

	if(numFuncs > NUM_ENT_FUNCS)
		lua_pop(scr::state, numFuncs - NUM_ENT_FUNCS);

	AddLock();
	EnsureLink();
}

/*--------------------------------------
	scn::CreateEntityType
--------------------------------------*/
scn::EntityType* scn::CreateEntityType(const char* name, int numFuncs)
{
	return new EntityType(name, numFuncs);
}

/*--------------------------------------
	scn::FindEntityType
--------------------------------------*/
scn::EntityType* scn::FindEntityType(const char* name)
{
	if(!name)
		return 0;

	for(const com::linker<EntityType>* it = EntityType::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->Name(), name))
			return it->o;
	}

	return 0;
}

/*
################################################################################################


	ENTITY TYPE LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::CreateEntityType

IN	sName, Init, Tick, Term, Save, Load
OUT	et

Function arguments can be nil.
--------------------------------------*/
int scn::CreateEntityType(lua_State* l)
{
	const char* name = luaL_checkstring(l, 1);
	lua_settop(l, 1 + NUM_ENT_FUNCS);
	EntityType* et = CreateEntityType(name, NUM_ENT_FUNCS);
	et->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::FindEntityType

IN	sName
OUT	etT
--------------------------------------*/
int scn::FindEntityType(lua_State* l)
{
	const char* name = luaL_checkstring(l, 1);
	EntityType* t = FindEntityType(name);

	if(t)
		t->LuaPush();
	else
		lua_pushnil(l);

	return 1;
}

/*--------------------------------------
LUA	scn::EntityTypeNames

OUT	tNames

Returns a table filled with the names of all entity types.
--------------------------------------*/
int scn::EntityTypeNames(lua_State* l)
{
	lua_createtable(l, 0, 0);
	int index = 1;

	for(const com::linker<EntityType>* it = EntityType::List().f; it; it = it->next)
	{
		lua_pushstring(l, it->o->Name());
		lua_rawseti(l, 1, index++);
	}

	return 1;
}

/*--------------------------------------
LUA	scn::TypName

IN	etT
OUT	sName
--------------------------------------*/
int scn::TypName(lua_State* l)
{
	EntityType* t = EntityType::CheckLuaTo(1);
	lua_pushstring(l, t->Name());
	return 1;
}

/*--------------------------------------
LUA	scn::TypFuncs (Funcs)

IN	etT
OUT	Init, Tick, Term, Save, Load
--------------------------------------*/
int scn::TypFuncs(lua_State* l)
{
	EntityType* t = EntityType::CheckLuaTo(1);

	for(int i = 0; i < NUM_ENT_FUNCS; i++)
	{
		if(t->HasRef(i))
			lua_rawgeti(l, LUA_REGISTRYINDEX, t->Refs()[i]);
		else
			lua_pushnil(l);
	}

	return NUM_ENT_FUNCS;
}