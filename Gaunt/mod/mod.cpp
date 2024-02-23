// mod.cpp
// Martynas Ceicys

#include <string.h>
#include <stdio.h>

#include "mod.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

namespace mod
{
	void CallGlobal(lua_State* l, const char* name);
}

static char* gameDir = 0;

/*
################################################################################################


	GAME


################################################################################################
*/

/*--------------------------------------
	mod::CallGlobal
--------------------------------------*/
void mod::CallGlobal(lua_State* l, const char* name)
{
	lua_getglobal(l, name);

	if(!lua_isfunction(l, -1))
	{
		lua_pop(l, 1);
		return;
	}

	scr::Call(l, 0, 0);
}

/*--------------------------------------
	mod::Game...
--------------------------------------*/
void mod::GameInit(){CallGlobal(scr::state, "GameInit");}
void mod::GameTick(){CallGlobal(scr::state, "GameTick");}
void mod::GamePostTick(){CallGlobal(scr::state, "GamePostTick");}
void mod::GameFrame(){CallGlobal(scr::state, "GameFrame");}
void mod::GameSave(){CallGlobal(scr::state, "GameSave");}
void mod::GameForeLoad(){CallGlobal(scr::state, "GameForeLoad");}
void mod::GameLoad(){CallGlobal(scr::state, "GameLoad");}
void mod::GamePostLoad(){CallGlobal(scr::state, "GamePostLoad");}

/*--------------------------------------
	mod::Path

Returns string in format [game]/[prefix][path], where game is either the game directory chosen
on startup or "DEFAULT", depending on whether the former file could be temporarily opened.
Returns ptr to static buffer. Calling again will overwrite it. prefix may be 0.

If 0 is returned, err is set. Otherwise, err is set to 0.

FIXME: game list
--------------------------------------*/
const char* mod::Path(const char* prefix, const char* path, const char*& err)
{
	static const size_t FILE_PATH_SIZE = 1024;
	static char filePath[FILE_PATH_SIZE];

	if(!prefix)
		prefix = "";

	if(gameDir)
	{
		com::SNPrintF(filePath, FILE_PATH_SIZE, 0, "%s/%s%s", gameDir, prefix, path);

		if(err = wrp::RestrictedPath(filePath))
			return 0;

		if(FILE* file = fopen(filePath, "r"))
		{
			fclose(file);
			return filePath;
		}
	}

	com::SNPrintF(filePath, FILE_PATH_SIZE, 0, "DEFAULT/%s%s", prefix, path);

	if(err = wrp::RestrictedPath(filePath))
		return 0;

	return filePath;
}

/*--------------------------------------
	mod::FOpen

If 0 is returned, err is set. Otherwise, err is set to 0.
--------------------------------------*/
FILE* mod::FOpen(const char* prefix, const char* path, const char* mode, const char*& err)
{
	err = 0;
	const char* finalPath = Path(prefix, path, err);

	if(err)
		return 0;

	FILE* f = fopen(finalPath, mode);

	if(!f)
		err = strerror(errno);

	return f;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	mod::Init

Sets the given directory as the first path to check in the Path function and loads standard game
scripts (game.lua and config.txt).

gameDirectory can be 0 or empty to always load default game files.

FIXME: config can't give values before initialization (e.g. video settings, shadow resolution,
	etc.) which probably slows down startup and makes it impossible for user to set options
	which affect initialization
		Make a config.txt and a game_config.txt; config.txt is executed as soon as Lua state is
		up, but systems need to be aware options may be modified before initialization
			Or make an init.txt run at the very beginning that can only set options (without
			invoking their func)
--------------------------------------*/
void mod::Init(const char* gameDirectory)
{
	// Set up Path
	size_t len = 0;
	if(gameDirectory && (len = strlen(gameDirectory)))
	{
		gameDir = new char[len + 1];
		strcpy(gameDir, gameDirectory);
	}

	// Load game.lua
	if(!scr::EnsureScript(scr::state, "game.lua"))
		WRP_FATAL("game.lua load failure");

	// Exec config.txt
	if(!con::ExecFile("config.txt"))
		WRP_FATAL("config.txt load failure");
}