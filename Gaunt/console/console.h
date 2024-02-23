// console.h
// Martynas Ceicys

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdarg.h>

#include "option.h"
#include "../../GauntCommon/io.h"

#define CON_ERRORF(format, ...) con::AlertF("%s: " format, COM_FUNC_NAME, __VA_ARGS__)
#define CON_ERROR(str) con::AlertF("%s: " str, COM_FUNC_NAME)

#if ENGINE_DEBUG
#define CON_DLOGF(format, ...) con::LogF(format, __VA_ARGS__)
#define CON_DLOG(str) con::LogF("%s", str)
#else
#define CON_DLOGF(format, ...)
#define CON_DLOG(str)
#endif

namespace con
{

/*
################################################################################################
	LOG
################################################################################################
*/

void	VLogF(const char* format, va_list args);
void	LogF(const char* format, ...);
void	VAlertF(const char* format, va_list args);
void	AlertF(const char* format, ...);
bool	AlertMode();
void	SetAlertMode(bool alert);
void	CloseLog();

/*
################################################################################################
	EXEC
################################################################################################
*/

bool	CreateCommand(const char* name);
void	ExecF(const char* format, ...);
bool	ExecFile(const char* fileName);

/*
################################################################################################
	USER INTERFACE
################################################################################################
*/

int		Scale();
bool	Opened();
void	SetScale(int i);
void	SetOpen(bool b);
void	InitUI();

/*
################################################################################################
	GENERAL
################################################################################################
*/

extern Option
	debug, /* If true, ExecF parses non-command strings as Lua statements, and script
		   breakpoints are activated (handled by scr system) */
	showMini;

void	Init();
void	InitLua();
void	Update();

}

#endif