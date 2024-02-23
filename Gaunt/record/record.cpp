// record.cpp -- Save/load state
// Martynas Ceicys

#include <stdlib.h>
#include <string.h>

#include "record.h"
#include "record_private.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../path/path.h"
#include "../scene/scene.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

namespace rec
{
	// RECORD LUA
	int GlobalTranscript(lua_State* l);
	int SetGlobalTranscript(lua_State* l);
}

com::JSVar rec::lvlRoot;

/*
################################################################################################


	RESOURCE MANAGEMENT


################################################################################################
*/

/*--------------------------------------
	rec::SetDefaultIDs
--------------------------------------*/
void rec::SetDefaultIDs()
{
	scn::Entity::SetDefaultRecordIDs();
	scn::Bulb::SetDefaultRecordIDs();
}

/*--------------------------------------
	rec::ClearIDs
--------------------------------------*/
void rec::ClearIDs()
{
	scn::Entity::ClearRecordIDs();
	scn::Bulb::ClearRecordIDs();
}

/*--------------------------------------
	rec::UnassignResourceTranscripts
--------------------------------------*/
void rec::UnassignResourceTranscripts()
{
	scn::Entity::UnsetTranscripts();
	scn::Bulb::UnsetTranscripts();
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	rec::Init
--------------------------------------*/
void rec::Init()
{
	luaL_Reg regs[] =
	{
		{"RequestSave", RequestSave},
		{"RequestLoad", RequestLoad},
		{"CurrentLevel", CurrentLevel},
		{"SaveEdits", SaveEdits},
		{"LoadJSON", LoadJSON},
		{"SaveJSON", SaveJSON},
		{"GlobalTranscript", GlobalTranscript},
		{"SetGlobalTranscript", SetGlobalTranscript},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "grec", regs, 0, 0, 0, 0);

	lua_pushcfunction(scr::state, RequestSave); con::CreateCommand("save");
	lua_pushcfunction(scr::state, RequestLoad); con::CreateCommand("load");
}

/*--------------------------------------
	rec::Update
--------------------------------------*/
void rec::Update()
{
	if(saveFilePath) Save();
	if(loadFilePath) Load();
}

/*--------------------------------------
	rec::CurrentLevel
--------------------------------------*/
const char* rec::CurrentLevel()
{
	return currentLevel;
}

/*
################################################################################################


	RECORD LUA


################################################################################################
*/

/*--------------------------------------
LUA	rec::GlobalTranscript

OUT	transcript
--------------------------------------*/
int rec::GlobalTranscript(lua_State* l)
{
	if(lvlRoot.Type() != com::JSVar::OBJECT)
		return 0;

	if(com::Pair<com::JSVar>* global = lvlRoot.Object()->Find("global"))
	{
		LuaPushJSVar(l, global->Value());
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	rec::SetGlobalTranscript

IN	transcript
--------------------------------------*/
int rec::SetGlobalTranscript(lua_State* l)
{
	if(lvlRoot.Type() != com::JSVar::OBJECT)
		luaL_error(l, "No root object");

	if(com::Pair<com::JSVar>* global = lvlRoot.Object()->Find("global"))
		LuaToJSVar(l, 1, global->Value());
	else
		luaL_error(l, "Root object does not have a 'global' key");

	return 0;
}