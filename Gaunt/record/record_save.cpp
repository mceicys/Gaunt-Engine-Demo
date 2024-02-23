// record_save.cpp
// Martynas Ceicys

#include "record.h"
#include "record_private.h"
#include "../console/console.h"
#include "../../GauntCommon/json_ext.h"
#include "../mod/mod.h"
#include "../path/path.h"
#include "../scene/scene.h"

namespace rec
{
	// SAVE
	void					SetGlobalPairs(com::PairMap<com::JSVar>& globalOut);
	template <class T> void	CreateResourceMap(const char* key);
	template <class T> void	TranscribeResources();
}

/*
################################################################################################


	SAVE


################################################################################################
*/

char* rec::saveFilePath = 0;
static bool saving = false;
static bool saveNeat = false;

/*--------------------------------------
	rec::RequestSave

Makes a request to save the game state after the current frame.

FIXME: only accept names with letters, numbers, and underscores
FIXME: automatically write to {mod}/saves/
--------------------------------------*/
void rec::RequestSave(const char* filePath, bool neat)
{
	if(!filePath)
		return;

	saveFilePath = (char*)realloc(saveFilePath, sizeof(char) * (strlen(filePath) + 1));
	strcpy(saveFilePath, filePath);
	saveNeat = neat;
}

/*--------------------------------------
	rec::Saving
--------------------------------------*/
bool rec::Saving()
{
	return saving;
}

/*--------------------------------------
	rec::Save

Saves the game state.
--------------------------------------*/
void rec::Save()
{
	lua_gc(scr::state, LUA_GCCOLLECT, 0);

	saving = true;
	con::LogF("Saving '%s'", saveFilePath);

	// Open file
	FILE* file = fopen(saveFilePath, "w");
	if(!file)
	{
		con::LogF("Failed to open file", saveFilePath);
		free(saveFilePath);
		saveFilePath = 0;
		saving = false;
		return;
	}

	SetDefaultIDs();

	// FIXME: Call a before-save script so record IDs can be modified before creating the tree
	com::PairMap<com::JSVar>& rootMap = *lvlRoot.SetObject();
	com::PairMap<com::JSVar>& global = *rootMap.Ensure("global")->Value().SetObject();
	CreateResourceMap<scn::Entity>("entities");
	CreateResourceMap<scn::Bulb>("bulbs");

	// Standard transcripts
	SetGlobalPairs(global);
	TranscribeResources<scn::Entity>();
	TranscribeResources<scn::Bulb>();

	// Script adjustments
	mod::GameSave();
	scn::CallEntityFunctions(scn::ENT_FUNC_SAVE);

	// Write
	if(saveNeat)
		com::WriteJSON(lvlRoot, file);
	else
		com::WriteJSON(lvlRoot, file, 0, false);

	// Done
	fclose(file);
	UnassignResourceTranscripts();
	com::FreeJSON(lvlRoot);
	free(saveFilePath);
	saveFilePath = 0;
	saving = false;
}

/*--------------------------------------
	rec::SetGlobalPairs
--------------------------------------*/
void rec::SetGlobalPairs(com::PairMap<com::JSVar>& globalOut)
{
	scn::SetGlobalPairs(globalOut);
}

/*--------------------------------------
	rec::CreateResourceMap
--------------------------------------*/
template <class T> void rec::CreateResourceMap(const char* key)
{
	com::PairMap<com::JSVar>* rootObj = lvlRoot.Object();
	if(!rootObj)
		return;

	com::PairMap<com::JSVar>* obj = 0;

	for(com::linker<T>* it = T::List().f; it; it = it->next)
	{
		T& t = *it->o;

		if(!t.RecordID())
			continue;

		if(!obj)
			obj = rootObj->Ensure(key)->Value().SetObject();

		t.SetTranscriptPtr(&obj->Ensure(t.RecordID())->Value());
	}
}

/*--------------------------------------
	rec::TranscribeResources
--------------------------------------*/
template <class T> void rec::TranscribeResources()
{
	for(com::linker<T>* it = T::List().f; it; it = it->next)
		it->o->Transcribe();
}

/*
################################################################################################


	SAVE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rec::RequestSave

IN	sFilePath, bNeat
--------------------------------------*/
int rec::RequestSave(lua_State* l)
{
	if(!lua_gettop(l))
	{
		con::LogF("%s: sFilePath, bNeat", COM_FUNC_NAME);
		return 0;
	}

	const char* filePath = luaL_checkstring(l, 1);
	bool neat = lua_toboolean(l, 2);
	RequestSave(filePath, neat);
	return 0;
}