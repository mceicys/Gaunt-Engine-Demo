// record_load.cpp
// Martynas Ceicys

#include "record.h"
#include "record_private.h"
#include "../audio/audio.h"
#include "../console/console.h"
#include "../../GauntCommon/json_ext.h"
#include "../hit/hit.h"
#include "../mod/mod.h"
#include "../path/path.h"
#include "../render/render.h"
#include "../scene/scene.h"

namespace rec
{
	// LOAD
	void LoadFail(const char* str, FILE* file);
	bool InterpretGlobalPairs();

	template<class T, void (&Interpret)(com::PairMap<com::JSVar>&)>
	bool InterpretResources(const char* key);
}

/*
################################################################################################


	LOAD


################################################################################################
*/

char* rec::loadFilePath = 0;
char* rec::currentLevel = 0;
static bool loading = false;

/*--------------------------------------
	rec::RequestLoad

Makes a request to load a game state after the current frame.
--------------------------------------*/
void rec::RequestLoad(const char* filePath)
{
	if(!filePath)
		return;

	loadFilePath = (char*)realloc(loadFilePath, sizeof(char) * (strlen(filePath) + 1));
	strcpy(loadFilePath, filePath);
}

/*--------------------------------------
	rec::Loading
--------------------------------------*/
bool rec::Loading()
{
	return loading;
}

/*--------------------------------------
	rec::Load

Loads a game state.

TODO: Add optional state clearing (textures, hulls, etc.)
FIXME: Should entity Init functions be called? When? Load functions can do that.
FIXME: Needs perfect clean-up in case of error.
	Engine will only have entity terminate functions, so the script will be in charge of
	checking whether they've been initialized before cleaning up
--------------------------------------*/
void rec::Load()
{
	loading = true;
	con::LogF("Loading '%s'", loadFilePath);
	size_t pathLen = strlen(loadFilePath);
	currentLevel = (char*)realloc(currentLevel, sizeof(char) * pathLen + 1);
	strcpy(currentLevel, loadFilePath);

	// Read whole file into buffer
	const char* err = 0;
	FILE* file = mod::FOpen(0, loadFilePath, "r", err);

	if(err)
		return LoadFail(err, file);

	com::Arr<char> buf(REC_DEFAULT_BUFFER_SIZE);
	com::FReadAll(buf, file);
	fclose(file);
	file = 0;

	// Convert to JSVar
	unsigned line;
	bool parsed = com::ParseJSON(buf.o, lvlRoot, &line);
	buf.Free();

	if(!parsed)
	{
		con::AlertF("Invalid JSON ln %u", line);
		return LoadFail(0, file);
	}

	if(lvlRoot.Type() != com::JSVar::OBJECT)
		return LoadFail("Root is not an object", file);

	// Cleanup previous state
	aud::StopVoices();
	scn::ClearEntities();
	scn::ClearBulbs();
	scn::ClearWorld(); // FIXME: Check if same world before clearing

	hit::CleanupBegin();
	lua_gc(scr::state, LUA_GCCOLLECT, 0);
	hit::CleanupEnd();

	ClearIDs(); // So surviving resources don't reserve any record IDs
	
	// Interpret
	if(!InterpretGlobalPairs())
		return LoadFail("Failed to parse 'global' pairs", file);

	mod::GameForeLoad();
	InterpretResources<scn::Entity, scn::InterpretEntities>("entities");
	InterpretResources<scn::Bulb, scn::InterpretBulbs>("bulbs");
	mod::GameLoad();
	scn::CallEntityFunctions(scn::ENT_FUNC_LOAD);
	mod::GamePostLoad();

	// Done
	aud::DeleteUnused();
	rnd::DeleteUnused();
	UnassignResourceTranscripts();
	com::FreeJSON(lvlRoot);
	free(loadFilePath);
	loadFilePath = 0;
	loading = false;

	lua_gc(scr::state, LUA_GCCOLLECT, 0); // Clean up objects unreferenced during loading
}

/*--------------------------------------
	rec::LoadFail
--------------------------------------*/
void rec::LoadFail(const char* str, FILE* file)
{
	CON_ERRORF("Failed to load '%s'", loadFilePath);

	if(str)
		con::AlertF("(%s)", str);

	if(file)
		fclose(file);

	UnassignResourceTranscripts();
	com::FreeJSON(lvlRoot);
	free(currentLevel);
	currentLevel = 0;
	free(loadFilePath);
	loadFilePath = 0;
	loading = false;
}

/*--------------------------------------
	rec::InterpretGlobalPairs

Returns true if successful and load may continue.
--------------------------------------*/
bool rec::InterpretGlobalPairs()
{
	com::Pair<com::JSVar>* globalPair = lvlRoot.Object()->Find("global");
	if(!globalPair)
		return false;

	com::PairMap<com::JSVar>* global = globalPair->Value().Object();
	if(!global)
		return false;

	if(!scn::InterpretGlobalPairs(*global))
		return false;

	return true;
}

/*--------------------------------------
	rec::InterpretResources
--------------------------------------*/
template<class T, void (&Interpret)(com::PairMap<com::JSVar>&)>
bool rec::InterpretResources(const char* key)
{
	if(com::Pair<com::JSVar>* pair = lvlRoot.Object()->Find(key))
	{
		if(com::PairMap<com::JSVar>* resources = pair->Value().Object())
		{
			Interpret(*resources);
			return true;
		}
	}

	return false;
}

/*
################################################################################################


	LOAD LUA


################################################################################################
*/

/*--------------------------------------
LUA	rec::RequestLoad

IN	sFileName
--------------------------------------*/
int rec::RequestLoad(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	RequestLoad(fileName);
	return 0;
}

/*--------------------------------------
LUA	rec::CurrentLevel

OUT	[sLevelName]
--------------------------------------*/
int rec::CurrentLevel(lua_State* l)
{
	const char* cur = CurrentLevel();

	if(cur)
	{
		lua_pushstring(l, cur);
		return 1;
	}

	return 0;
}