// record_edit.cpp
// Martynas Ceicys

#include "record.h"
#include "record_private.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../mod/mod.h"

namespace rec
{
	void SaveEditsFail(FILE* file, com::JSVar* root, const char* extraErr);
}

#define SAVE_EDITS_FAIL(err) SaveEditsFail(file, &root, err)

/*--------------------------------------
	rec::SaveEdits
--------------------------------------*/
void rec::SaveEdits(const com::JSVar& edits)
{
	FILE* file = 0;
	com::JSVar root;

	if(!currentLevel)
		return SAVE_EDITS_FAIL("No current level");

	con::LogF("Saving edits on '%s'", currentLevel);

	const char* pathErr = 0;
	const char* path = mod::Path(0, currentLevel, pathErr);

	if(pathErr)
		return SAVE_EDITS_FAIL(pathErr);

	file = fopen(path, "r");

	if(!file)
		return SAVE_EDITS_FAIL(strerror(errno));

	com::Arr<char> buf(REC_DEFAULT_BUFFER_SIZE);
	com::FReadAll(buf, file);
	fclose(file);
	file = 0;
	unsigned line;
	bool parsed = com::ParseJSON(buf.o, root, &line);
	buf.Free();

	if(!parsed)
	{
		con::AlertF("Invalid JSON ln %u", line);
		return SAVE_EDITS_FAIL(0);
	}

	ApplyJSVarDiff(root, edits);
	file = fopen(path, "w");

	if(!file)
		return SAVE_EDITS_FAIL(strerror(errno));

	com::WriteJSON(root, file);
	fclose(file);
	com::FreeJSON(root);
}

/*--------------------------------------
	rec::SaveEditsFail
--------------------------------------*/
void rec::SaveEditsFail(FILE* file, com::JSVar* root, const char* extraErr)
{
	if(file)
		fclose(file);

	if(root)
		com::FreeJSON(*root);

	if(currentLevel)
		CON_ERRORF("Failed to edit '%s'", currentLevel);
	else
		CON_ERROR("Failed to edit current level");

	if(extraErr)
		con::AlertF("(%s)", extraErr);
}

/*
################################################################################################


	EDIT LUA


################################################################################################
*/

/*--------------------------------------
LUA	rec::SaveEdits

IN	tEdits
--------------------------------------*/
int rec::SaveEdits(lua_State* l)
{
	com::JSVar edits;
	LuaToJSVar(l, 1, edits);
	SaveEdits(edits);
	com::FreeJSON(edits);
	return 0;
}

/*--------------------------------------
LUA	rec::LoadJSON

IN	sFilePath
OUT	[tJSON]

FIXME: these functions don't belong in this source file
--------------------------------------*/
int rec::LoadJSON(lua_State* l)
{
	const char* filePath = luaL_checkstring(l, 1);
	con::LogF("Loading file '%s'", filePath);
	const char* err;
	const char* path = mod::Path(0, filePath, err);

	if(err)
		luaL_argerror(l, 1, err);

	FILE* file = fopen(path, "r");

	if(!file)
	{
		CON_ERRORF("%s", strerror(errno));
		return 0;
	}

	com::Arr<char> buf(REC_DEFAULT_BUFFER_SIZE);
	com::FReadAll(buf, file);
	fclose(file);
	unsigned line;
	com::JSVar root;
	bool parsed = com::ParseJSON(buf.o, root, &line);
	buf.Free();

	if(!parsed)
	{
		com::FreeJSON(root);
		CON_ERRORF("Invalid JSON on ln %u", line);
		return 0;
	}

	LuaPushJSVar(l, root);
	com::FreeJSON(root);
	return 1;
}

/*--------------------------------------
LUA	rec::SaveJSON

IN	sFilePath, tJSON
--------------------------------------*/
int rec::SaveJSON(lua_State* l)
{
	const char* filePath = luaL_checkstring(l, 1);
	con::LogF("Saving file '%s'", filePath);
	const char* err;
	const char* path = mod::Path(0, filePath, err);

	if(err)
		luaL_argerror(l, 1, err);

	com::JSVar root;
	LuaToJSVar(l, 2, root);
	FILE* file = fopen(path, "w");

	if(!file)
	{
		CON_ERRORF("%s", strerror(errno));
		return 0;
	}

	com::WriteJSON(root, file);
	fclose(file);
	com::FreeJSON(root);
	return 0;
}