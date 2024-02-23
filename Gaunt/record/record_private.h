// record_private.h
// Martynas Ceicys

#ifndef RECORD_PRIVATE_H
#define RECORD_PRIVATE_H

#include "pairs.h"

namespace rec
{

#define REC_DEFAULT_BUFFER_SIZE 8192

extern com::JSVar lvlRoot;
extern char *saveFilePath, *loadFilePath, *currentLevel;

/*
################################################################################################
	RESOURCE MANAGEMENT
################################################################################################
*/

void SetDefaultIDs();
void ClearIDs();
void UnassignResourceTranscripts();

/*
################################################################################################
	SAVE
################################################################################################
*/

void Save();

/*
################################################################################################
	LOAD
################################################################################################
*/

void Load();

/*
################################################################################################
	LUA
################################################################################################
*/

int RequestSave(lua_State* l);
int RequestLoad(lua_State* l);
int CurrentLevel(lua_State* l);
int SaveEdits(lua_State* l);
int LoadJSON(lua_State* l);
int SaveJSON(lua_State* l);

}

#endif