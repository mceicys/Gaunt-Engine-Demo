// mod.h -- Game mod selection
// Martynas Ceicys

// FIXME: rename to "game" or something

#ifndef MOD_H
#define MOD_H

#include <stdio.h>

namespace mod
{

/*
################################################################################################
	GAME
################################################################################################
*/

void	GameInit();
void	GameTick();
void	GamePostTick();
void	GameFrame();
void	GameSave();
void	GameForeLoad();
void	GameLoad();
void	GamePostLoad();

const char* Path(const char* prefix, const char* path, const char*& errOut);
FILE* FOpen(const char* prefix, const char* path, const char* mode, const char*& errOut);

/*
################################################################################################
	GENERAL
################################################################################################
*/

void	Init(const char* gameDirectory);

}

#endif