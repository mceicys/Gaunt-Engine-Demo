// console.cpp
// Martynas Ceicys

// FIXME: test file-stream based logging on a hard disk, make sure it's never causing stutters
// FIXME: handle 2 GB log; position longs probably overflow; fwrite probably sets an error
// FIXME: ancient code, needs refactoring

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "console.h"
#include "../../GauntCommon/io.h"
#include "../gui/gui.h"
#include "../input/input.h"
#include "../mod/mod.h"
#include "../render/render.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

#define CMD_BUFFER_SIZE			256
#define CMD_HISTORY				32
#define LOG_NEWLINE				"\r\n"
#define LOG_NEWLINE_LEN			2
#define NO_ORIGIN -1000

namespace con
{
	Option
		debug("con_debug", false),
		showMini("con_show_mini", true);

	// LOG
	static FILE*				logFile = 0;
	static long					logEnd = 0;
	static unsigned long long	logTime = 0;
	static long					updateStart = -1, updateEnd = -1;
									// Range changed in file since readBuf was updated
	static bool					alertMode = true;

	// LOG LUA
	int		LogF(lua_State* l);
	int		Log(lua_State* l);
	int		AlertMode(lua_State* l);
	int		SetAlertMode(lua_State* l);

	// EXEC
	struct	cmd;
	bool	ParseString(const char* str, bool printErr);

	// EXEC LUA
	int		CreateCommand(lua_State* l);
	int		ExecF(lua_State* l);
	int		ExecFile(lua_State* l);

	// USER INTERFACE
	static bool						uiInitialized	= false;

	static int						scale			= 2;
	static bool						opened			= false;

	static const unsigned			NUM_OPEN_LINES	= 25;
	static const unsigned			NUM_MINI_LINES	= 4;
	static const unsigned			NUM_MAX_LINES	= COM_MAX(NUM_OPEN_LINES, NUM_MINI_LINES);
	static unsigned					numTxtLines		= 0;
	static const unsigned			NUM_TXT_IMAGES	= 39;
	static unsigned					miniShowTime	= 3000;
	static unsigned long long		miniScrollTime	= 0;
	static com::Arr<char>			readBuf;
	static const long				LEFT_PROBE		= 2; /* First char in readBuf is a "probe"
														 that is assumed to belong to a line
														 that isn't entirely buffered; probe is
														 two chars to check for a newline */
	static const long				RIGHT_PROBE		= 2; /* Two chars at the end of readBuf
														 should not be displayed, only used to
														 check for a newline */
	static long						bufStart		= -LEFT_PROBE,	bufEnd = -LEFT_PROBE;
	static long						scrollPos		= -1;
	static long						moveLines		= 0;

	static char*					cmdBuffer;
	static char*					cmdHistBuffer;
	static char*					cmdHistLast;
	static int						cmdHistScroll	= 0;
	static unsigned					cmdNumHist		= 0;

	static res::Ptr<rnd::Texture>	texFont;
	static res::Ptr<rnd::Texture>	texConBack;
	static gui::iText*				txtCmdLine;
	static gui::iText**				txtLog;
	static gui::iImage*				imgConBack;

	void	SetNumLines(unsigned num);
	void	SetTextPos();
	void	UpdateUI();
	char*	CurrentLineStart(char* buf, char* line);
	char*	PreviousLineStart(char* buf, char* line);
	size_t	CheckNewline(long pos);
	void	ExcludeNewline(long lineEnd, long& excludeIO, char& ncIO);
	void	CheckReadBufferSize();
	void	UpdateReadBuffer(long pos, int origin = -1);
	void	JumpScrollPos(long pos);
	long	NextVisible(long pos, size_t* visLenOut = 0, bool* fullOut = 0);
	void	CountVisible(unsigned* numVisLines, long* visEndOut);

	// UI LUA
	int		SetOpen(lua_State* l);
}

/*
################################################################################################


	LOG


################################################################################################
*/

/*--------------------------------------
	con::VLogF

FIXME: Add no-newline option
--------------------------------------*/
void con::VLogF(const char* format, va_list args)
{
	updateStart = updateStart == -1 ? logEnd : com::Min(updateStart, logEnd);

	// Print into temporary buffer
	static com::Arr<char> writeBuf(16);
	int printed = com::VArrPrintF(writeBuf, 0, format, args);

	// Convert "\n" to "\r\n"
	bool skipConvert = false;
	int numConvert = 0;

	for(int i = 0; i < printed; i++)
	{
		if(writeBuf[i] == '\r')
			skipConvert = true;
		else
		{
			if(writeBuf[i] == '\n' && !skipConvert)
				numConvert++;

			skipConvert = false;
		}
	}

	size_t finalSize = printed + numConvert;

	if(numConvert)
	{
		writeBuf.Ensure(finalSize);
		writeBuf[finalSize] = 0;

		for(int r = printed - 1, w = printed - 1 + numConvert; r >= 0;)
		{
			writeBuf[w] = writeBuf[r];

			if(writeBuf[r] == '\n' && r && writeBuf[r - 1] != '\r')
			{
				w--;
				writeBuf[w] = '\r';
			}

			r--;
			w--;
		}
	}

	// Write into file
	fseek(logFile, logEnd, SEEK_SET);
	fwrite(writeBuf.o, sizeof(char), finalSize, logFile);
	fwrite(LOG_NEWLINE, sizeof(char), LOG_NEWLINE_LEN, logFile);
	logEnd = ftell(logFile);
	logTime = wrp::Time();
	miniScrollTime = logTime; // FIXME: would be better if Log func didn't have to manage this
	updateEnd = updateEnd == -1 ? logEnd : com::Max(updateEnd, logEnd);
}

/*--------------------------------------
	con::LogF
--------------------------------------*/
void con::LogF(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	VLogF(format, args);
	va_end(args);
}

/*--------------------------------------
	con::VAlertF
--------------------------------------*/
void con::VAlertF(const char* format, va_list args)
{
	VLogF(format, args);

	if(alertMode)
		SetOpen(true);
}

/*--------------------------------------
	con::AlertF

Logs and, if alertMode is on, opens the console.
--------------------------------------*/
void con::AlertF(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	VAlertF(format, args);
	va_end(args);
}

/*--------------------------------------
	con::AlertMode
--------------------------------------*/
bool con::AlertMode()
{
	return alertMode;
}

/*--------------------------------------
	con::SetAlertMode
--------------------------------------*/
void con::SetAlertMode(bool alert)
{
	alertMode = alert;
	LogF("SetAlertMode: %u", (unsigned)alertMode);
}

/*--------------------------------------
	con::CloseLog
--------------------------------------*/
void con::CloseLog()
{
	if(logFile)
		fclose(logFile);

	logFile = 0;
}

/*
################################################################################################


	LOG LUA


################################################################################################
*/

/*--------------------------------------
LUA	con::LogF

IN	sFormat, ...

FIXME: it would be better to make this Log and create LogF by wrapping it in script and doing a
string.format there. Cmd Log would have to be renamed.
--------------------------------------*/
int con::LogF(lua_State* l)
{
	const char* format = lua_tostring(l, 1);
	if(!format) return 0;
	int numArgs = lua_gettop(l);

	lua_getglobal(l, "string");
	lua_getfield(l, -1, "format");
	lua_insert(l, 1);
	lua_pop(l, 1);
	lua_pcall(l, numArgs, 1, 0);

	const char* result = lua_tostring(l, 1);
	con::LogF("%s", result);

	return 0;
}

/*--------------------------------------
LUA	con::Log

IN	...

Concatenates all args as strings (with space in between) and logs the resulting string.
Intended to be used as a console command.
--------------------------------------*/
int con::Log(lua_State* l)
{
	int numArgs = lua_gettop(l);
	int numStrs = numArgs  * 2 - 1; // Includes spaces
	for(int i = 1; i < numStrs; i += 2)
	{
		lua_pushstring(l, " ");
		lua_insert(l, i + 1);
	}
	lua_concat(l, numStrs);
	con::LogF("%s", lua_tostring(l, -1));
	return 0;
}

/*--------------------------------------
LUA	con::AlertMode

OUT	bAlertMode
--------------------------------------*/
int con::AlertMode(lua_State* l)
{
	lua_pushboolean(l, AlertMode());
	return 1;
}

/*--------------------------------------
LUA	con::SetAlertMode

IN	bAlertMode
--------------------------------------*/
int con::SetAlertMode(lua_State* l)
{
	SetAlertMode(lua_toboolean(l, 1));
	return 0;
}

/*
################################################################################################


	EXEC


################################################################################################
*/

/* Commands are Lua functions accessible from the console via a simpler syntax:
commandname arg1 arg2 ... */
struct con::cmd
{
	char*	name;
	int		funcRef;
};

static com::linker<con::cmd>*	lastCmd		= 0;

/*--------------------------------------
	con::CreateCommand

Creates a console command with the given name that corresponds to the Lua function at the top of
the stack. Returns true if a command was created. Returns false if name is 0 or is taken, or the
value at the top of the stack is not a function. Pops 1 value.

FIXME: Consider removing commands. It's a hassle maintaining 3 avenues of system interaction and
	there's only minor benefit (simpler syntax, easy "cheat mode").
--------------------------------------*/
bool con::CreateCommand(const char* name)
{
	if(!name || !lua_isfunction(scr::state, -1))
	{
		lua_pop(scr::state, 1);
		return false; // FIXME: do fatal error when CreateCommand fails?
	}

	for(com::linker<cmd>* it = lastCmd; it != 0; it = it->prev)
	{
		if(!strcmp(it->o->name, name))
		{
			AlertF("CreateCommand: name \"%s\" already taken", name);
			lua_pop(scr::state, 1);
			return false;
		}
	}

	cmd* c = new cmd;
	c->name = new char[strlen(name) + 1];
	strcpy(c->name, name);
	c->funcRef = luaL_ref(scr::state, LUA_REGISTRYINDEX);
	com::Link(lastCmd, c);
	return true;
}

/*--------------------------------------
	con::ExecF

Executes the resulting string.

FIXME: Make an Exec version that doesn't do printf'ing and mallocing unless the string needs to be modified
--------------------------------------*/
void con::ExecF(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	com::Arr<char> buf;
	com::VArrPrintF(buf, 0, format, args);
	va_end(args);

	if(ParseString(buf.o, !debug.Bool()) || !debug.Bool())
	{
		buf.Free();
		return;
	}

	// Executing as Lua

	// Replace a starting "=" with "return " for easier queries
	if(buf[0] == '=')
	{
		buf[0] = ' ';
		buf.Ensure(strlen(buf.o) + 7);
		com::StrNPrepend("return", buf.o, buf.n - 1);
	}

#if 0
	// Automatically prepend "return " unless the statement doesn't work as a retstat
	// FIXME: don't do this for ExecFile
	// FIXME: basic implementation prevents user from setting vars since assignments aren't allowed in retstats
		// FIXME: don't prepend 'return' if an isolated equals sign is in the string?
	if(strncmp(printBuffer, "return ", 7) &&
	strncmp(printBuffer, "do ", 3) &&
	strncmp(printBuffer, "for ", 4) &&
	strncmp(printBuffer, "if ", 3) &&
	strncmp(printBuffer, "repeat ", 7) &&
	strncmp(printBuffer, "while ", 6))
		com::StrNPrepend("return ", printBuffer, PRINT_BUFFER_SIZE - 1);
#endif

	int preTop = lua_gettop(scr::state);

	if(scr::DoString(scr::state, buf.o, 0, true, true) == LUA_OK)
	{
		for(int i = preTop + 1; i <= lua_gettop(scr::state); i++)
		{
			// FIXME: Better printing of types (table contents, function address, etc.)
			// FIXME: Print on one line
			int type = lua_type(scr::state, i);

			switch(type)
			{
			case LUA_TNUMBER:
				LogF(COM_DBL_PRNT, (double)lua_tonumber(scr::state, i));
				break;
			case LUA_TBOOLEAN:
				LogF("%s", lua_toboolean(scr::state, i) != 0 ? "true" : "false");
				break;
			case LUA_TSTRING:
				LogF("\"%s\"", lua_tostring(scr::state, i));
				break;
			default:
				LogF("%s", lua_typename(scr::state, type));
			}
		}

		lua_settop(scr::state, preTop);
	}

	buf.Free();
}

/*--------------------------------------
	con::ExecFile

Opens the file and executes each line. Returns false if the file could not be opened.
--------------------------------------*/
bool con::ExecFile(const char* fileName)
{
	if(!fileName)
		return false;

	const char* err = 0;
	FILE* f = mod::FOpen(0, fileName, "r", err);

	if(err)
	{
		CON_ERRORF("Failed to exec '%s' (%s)", fileName, err);
		return false;
	}

	const char* buf;
	while((buf = com::ReadLine(f)) != 0)
		ExecF("%s", buf);

	fclose(f);
	return true;
}

/*--------------------------------------
	con::ParseString

Splits str into space-delimited tokens. Parses first token of str for a cmd name and loads
corresponding Lua function. Pushes args onto stack as booleans, numbers, or strings. If there
are no tokens, prints a blank line.

Returns false if str contains tokens but the first token is not a cmd.

FIXME: Refactor token counting and check for all whitespace
--------------------------------------*/
bool con::ParseString(const char* str, bool printErr)
{
	const char* const WHITESPACE = " \t";
	unsigned int length = strlen(str);
	char* tempStr = new char[length + 1]; // FIXME: don't malloc
		// FIXME: keep static resizable buffer that's abandoned as soon as anything else is called?
			// FIXME: also don't use it or tokenize until a valid command is found
	strcpy(tempStr, str);

	size_t argCount = 0;

	// Get token count
	for(size_t s = 0; s < length; s++)
	{
		if((strchr(WHITESPACE, tempStr[s]) && strchr(WHITESPACE, tempStr[s + 1]) == 0 && tempStr[s + 1] != 0) ||
		(s == 0 && strchr(WHITESPACE, tempStr[s]) == 0 && tempStr[s] != 0))
			argCount++;
	}

	// Print blank line if no tokens
	if(!argCount)
	{
		LogF("");
		delete[] tempStr;
		return true;
	}

	// Get tokens
	char** arg = new char*[argCount]; // FIXME: don't malloc, tokenize while sending the parameters to Lua
	arg[0] = strtok(tempStr, WHITESPACE);
	
	for(size_t s = 1; s < argCount; s++)
	{
		arg[s] = strtok(NULL, WHITESPACE);
	}

	// Ignore comments
	if(strspn(arg[0], "/") >= 2)
	{
		delete[] arg;
		delete[] tempStr;
		return true;
	}

	// Call if cmd
	for(com::linker<cmd>* it = lastCmd; it != 0; it = it->prev)
	{
		if(!com::StrNCmpLower(it->o->name, arg[0], -1))
		{
			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, it->o->funcRef);

			for(size_t s = 1; s < argCount; s++)
			{
				if(!strcmp(arg[s], "true"))
					lua_pushboolean(scr::state, 1);
				else if(!strcmp(arg[s], "false"))
					lua_pushboolean(scr::state, 0);
				else
					lua_pushstring(scr::state, arg[s]);
			}

			scr::Call(scr::state, argCount - 1, 0, false);
			delete[] arg;
			delete[] tempStr;
			return true;
		}
	}

	if(printErr)
		LogF("\"%s\" is not a recognized cmd", arg[0]);

	delete[] arg;
	delete[] tempStr;
	return false;
}

/*
################################################################################################


	EXEC LUA


################################################################################################
*/

/*--------------------------------------
LUA	con::CreateCommand

IN	sName, Func
--------------------------------------*/
int con::CreateCommand(lua_State* l)
{
	const char* name = luaL_checkstring(l, 1);
	if(!CreateCommand(name))
		luaL_error(l, "bad args");
	return 0;
}

/*--------------------------------------
LUA	con::ExecF

IN	sFormat, ...

FIXME: Like LogF, better to make a wrapper
--------------------------------------*/
int con::ExecF(lua_State* l)
{
	const char* format = lua_tostring(l, 1);
	if(!format) return 0;
	int numArgs = lua_gettop(l);

	lua_getglobal(l, "string");
	lua_getfield(l, -1, "format");
	lua_insert(l, 1);
	lua_pop(l, 1);
	lua_pcall(l, numArgs, 1, 0);

	const char* result = lua_tostring(l, 1);
	con::ExecF("%s", result);

	return 0;
}

/*--------------------------------------
LUA	con::ExecFile

IN	sFileName
--------------------------------------*/
int con::ExecFile(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	ExecFile(fileName);
	return 0;
}

/*
################################################################################################


	USER INTERFACE

Console visuals.

FIXME: Rewrite after revising GUI system
FIXME: Set pos only when needed
FIXME: Stretching + word-based wrapping
FIXME: Per-message timer for mini console
################################################################################################
*/

#define CHAR_WIDTH	8
#define CHAR_HEIGHT	9

/*--------------------------------------
	con::SetNumLines
--------------------------------------*/
void con::SetNumLines(unsigned num)
{
	if(num == numTxtLines)
		return;

	numTxtLines = COM_MIN(num, NUM_MAX_LINES);
	for(unsigned txt = 0; txt < NUM_MAX_LINES; txt++)
		txtLog[txt]->SetVisible(txt < numTxtLines);
}

/*--------------------------------------
	con::SetTextPos
--------------------------------------*/
void con::SetTextPos()
{
	float x = opened ? wrp::VideoWidth() * 0.5f - (160.0f - CHAR_WIDTH) * scale : CHAR_WIDTH * scale;
	float h = wrp::VideoHeight();

	for(unsigned int i = 0; i < numTxtLines; i++)
		txtLog[i]->SetPos(com::Vec3(x, h - CHAR_HEIGHT * (i + 1) * scale, 0.0f));

	if(opened)
		txtCmdLine->SetPos(com::Vec3(x, h - CHAR_HEIGHT * (numTxtLines + 1) * scale, 0.0f));
}

/*--------------------------------------
	con::UpdateUI

Updates console UI text element strings.

FIXME: Update strings only after a change (in buffer, console size, scroll, etc.)

FIXME: GUI code should take care of displaying a buffer w/ wrapping via TextBox

FIXME: Auto scroll during script breakpoint debugging is buggy
	Breakpoint will open console with last logged line in the middle of the terminal
	Hitting F10 will sometimes scroll a little instead of waiting for lines to reach bottom
--------------------------------------*/
void con::UpdateUI()
{
	if(opened)
		SetNumLines(NUM_OPEN_LINES);
	else
	{
		SetNumLines(showMini.Bool() ? NUM_MINI_LINES : 0);

		// FIXME: scroll past whole messages, based on how much time passed since each was logged
		if(scrollPos < logEnd)
		{
			unsigned long long time = wrp::Time();
			unsigned long long timeSinceScroll = time - miniScrollTime;
			long miniScroll = timeSinceScroll / miniShowTime;

			if(miniScroll)
			{
				miniScrollTime = time;
				moveLines += miniScroll;
			}
		}
	}

	SetTextPos();

	// Update/validate read buffer
	long saveUpdateStart = updateStart, saveUpdateEnd = updateEnd;
	CheckReadBufferSize();

	if(scrollPos <= -1)
	{
		JumpScrollPos(logEnd);
		moveLines = moveLines - numTxtLines;
	}
	else
		JumpScrollPos(scrollPos);

	if(updateStart != -1)
		UpdateReadBuffer(0, NO_ORIGIN);

	// Drag scrollPos if updates happened right below display
	// FIXME: should have an appendStart separate from updateStart for this
	unsigned numVisLines;
	long visEnd;
	CountVisible(&numVisLines, &visEnd);

	if(saveUpdateStart <= visEnd && saveUpdateEnd > visEnd)
	{
		if(numVisLines < numTxtLines)
		{
			// The buffer might have run out of room to count all new visible lines
			UpdateReadBuffer(scrollPos, -1);
			CountVisible(&numVisLines, &visEnd);
		}

		if(numVisLines >= numTxtLines)
		{
			JumpScrollPos(logEnd);
			moveLines = moveLines - numTxtLines;
		}
	}

	// Respond to scroll input
	while(moveLines > 0)
	{
		while(1)
		{
			// Find start of next displayable line
			bool full;
			scrollPos = NextVisible(scrollPos, 0, &full);

			if(full)
				break;
			else
				UpdateReadBuffer(scrollPos, 0);
		}

		moveLines--;

		if(scrollPos > logEnd)
		{
			moveLines = 0;
			scrollPos = logEnd;
		}
	}

	if(moveLines < 0)
	{
		do
		{
			// Make sure scrollPos is not in the left probe so we can check rear characters
			if(scrollPos < bufStart + LEFT_PROBE)
				UpdateReadBuffer(scrollPos, 0);

			long lineEnd = scrollPos;

			// Check length of rear newline
			long excludeNewline = 0;

			if(readBuf[lineEnd - 1 - bufStart] == '\n')
			{
				excludeNewline++;

				if(readBuf[lineEnd - 2 - bufStart] = '\r')
					excludeNewline++;
			}

			// Get line start
			char* line = PreviousLineStart(readBuf.o, &readBuf[scrollPos - bufStart]);

			if(!line) // This shouldn't happen
				line = readBuf.o;

			if(line == readBuf.o && bufStart > 0)
			{
				// Hit the left probe, can't tell if this is the line's start
				// Move buffer back until we reach start of line
				do
				{
					long lineSave = bufStart + (line - readBuf.o);
					UpdateReadBuffer(bufStart, 0);
					line = CurrentLineStart(readBuf.o, &readBuf[lineSave - bufStart]);

					if(lineSave == bufStart + (line - readBuf.o))
						break; // Didn't move back, this is the line start
				} while(line == readBuf.o);
			}

			// Get visual line length
			long lineStart = bufStart + (line - readBuf.o);
			size_t len = lineEnd - lineStart - excludeNewline;

			// Move across visual lines
			size_t numFullLines = len / NUM_TXT_IMAGES;
			size_t remainder = len % NUM_TXT_IMAGES;

			if(remainder)
				moveLines++;
			else if(!len)
				moveLines++; // Empty line

			long subtractFullLines = com::Min((long)numFullLines, -moveLines);
			moveLines += subtractFullLines;

			scrollPos = lineEnd - excludeNewline - remainder -
				subtractFullLines * NUM_TXT_IMAGES;

			if(scrollPos < 0)
			{
				scrollPos = 0;
				moveLines = 0;
			}
		} while(moveLines < 0);

		/* scrollPos might be outside of buffer range again if we moved into the middle of a
		very big line, check it again */
		if(scrollPos < bufStart + LEFT_PROBE || scrollPos > bufEnd - RIGHT_PROBE)
			UpdateReadBuffer(scrollPos, -1);
	}

	// Make sure enough is buffered to fill the screen
	CountVisible(&numVisLines, &visEnd);

	if(numVisLines < numTxtLines && visEnd < logEnd)
		UpdateReadBuffer(scrollPos, -1);

	// Set console visuals
	long curPos = scrollPos;
	unsigned txt = 0;

	while(txt < numTxtLines)
	{
		if(curPos >= bufStart)
		{
			size_t visLen;
			long lineEnd = NextVisible(curPos, &visLen);
			txtLog[txt++]->SetString(&readBuf[curPos - bufStart], visLen, 0, false);
			curPos = lineEnd;
		}
		else
		{
			txtLog[txt++]->SetString(0);
			curPos++;
		}

		if(curPos >= bufEnd - RIGHT_PROBE)
			break;
	}

	for(; txt < numTxtLines; txt++)
		txtLog[txt]->SetString(0);

	// Display last characters of command line
	size_t cmdSize = strlen(cmdBuffer) + 1;
	txtCmdLine->SetString(&cmdBuffer[cmdSize > NUM_TXT_IMAGES ? cmdSize - NUM_TXT_IMAGES : 0], -1, 0, false);

	// Console background pos
	imgConBack->pos.x = (float)wrp::VideoWidth()/2.0f;
	imgConBack->pos.y = (float)wrp::VideoHeight() - 120.0f * imgConBack->scale;
}

/*--------------------------------------
	con::Scale
--------------------------------------*/
int con::Scale()
{
	return scale;
}

/*--------------------------------------
	con::Opened
--------------------------------------*/
bool con::Opened()
{
	return opened;
}

/*--------------------------------------
	con::SetScale
--------------------------------------*/
void con::SetScale(int i)
{
	scale = i;
	for(unsigned int i = 0; i < NUM_MAX_LINES; i++)
		txtLog[i]->SetScale(scale);
	txtCmdLine->SetScale(scale);
	SetTextPos();
	imgConBack->scale = scale;
}

/*--------------------------------------
	con::SetOpen

Can also be used before initializing the UI.
--------------------------------------*/
void con::SetOpen(bool b)
{
	opened = b;

	if(uiInitialized)
	{
		txtCmdLine->SetVisible(opened);
		imgConBack->visible = opened;
		UpdateUI(); // Instant feedback
	}
}

/*--------------------------------------
	con::InitUI

Creates console UI elements.
--------------------------------------*/
void con::InitUI()
{
	uiInitialized = true;

	cmdBuffer = new char[CMD_BUFFER_SIZE];
	memset(cmdBuffer, 0, CMD_BUFFER_SIZE);

	cmdHistBuffer = new char[CMD_BUFFER_SIZE * CMD_HISTORY];
	memset(cmdHistBuffer, 0, CMD_BUFFER_SIZE * CMD_HISTORY);

	cmdHistLast = cmdHistBuffer;

	txtLog = new gui::iText*[NUM_MAX_LINES];

	texFont.Set(rnd::EnsureTexture("font_small.tex"));
	texConBack.Set(rnd::EnsureTexture("console_bg.tex"));

	for(unsigned int i = 0; i < NUM_MAX_LINES; i++)
	{
		txtLog[i] = gui::CreateText(texFont, CHAR_WIDTH);
		txtLog[i]->AddLock();
		txtLog[i]->SetImageCount(NUM_TXT_IMAGES);
	}

	txtCmdLine = gui::CreateText(texFont, CHAR_WIDTH);
	txtCmdLine->AddLock();
	txtCmdLine->SetImageCount(NUM_TXT_IMAGES);
	txtCmdLine->SetSubPalette(3); // FIXME: should not be a constant (sub-palettes are moddable)

	imgConBack = gui::CreateImage(texConBack);
	imgConBack->AddLock();
	imgConBack->pos.z = 0.0f;

	SetScale(scale);
	SetOpen(opened);
}

/*--------------------------------------
	con::CurrentLineStart

Returns start of current line.
--------------------------------------*/
char* con::CurrentLineStart(char* buf, char* ptr)
{
	// Move to start of current line
	while(ptr != buf && *(ptr - 1) != '\n')
		ptr--;

	return ptr;
}

/*--------------------------------------
	con::PreviousLineStart

Returns first line-start before ptr. Returns 0 if ptr == buf.
--------------------------------------*/
char* con::PreviousLineStart(char* buf, char* ptr)
{
	if(ptr == buf)
		return 0;

	if((*ptr) == 0)
		ptr--; // Starting on null terminator
	else if(ptr != buf && *(ptr - 1) == '\n')
		ptr--; // Starting on a line-start, get past it

	return CurrentLineStart(buf, ptr);
}

/*--------------------------------------
	con::CheckNewline
--------------------------------------*/
size_t con::CheckNewline(long pos)
{
	if(pos < bufStart)
		return 0;

	if(pos >= bufEnd)
		return 0;

	if(readBuf[pos - bufStart] == '\n')
		return 1;

	if(pos < bufEnd - 1 && readBuf[pos - bufStart] == '\r' &&
	readBuf[pos - bufStart + 1] == '\n')
		return 2;

	return 0;
}

/*--------------------------------------
	con::ExcludeNewline

FIXME: remove
--------------------------------------*/
void con::ExcludeNewline(long lineEnd, long& exclude, char& nc)
{
	if(nc == 0)
		return;

	if(nc == '\n')
	{
		long pos = lineEnd - 1;

		if(pos >= bufEnd)
		{
			nc = 0;
			return;
		}

		if(pos < bufStart)
			return;

		if(readBuf[pos - bufStart] == '\n')
		{
			exclude++;
			nc = '\r';
		}
		else
		{
			nc = 0;
			return;
		}
	}

	if(nc == '\r')
	{
		long pos = lineEnd - 2;

		if(pos >= bufEnd)
		{
			nc = 0;
			return;
		}

		if(pos < bufStart)
			return;

		if(readBuf[pos - bufStart] == '\r')
			exclude++;

		nc = 0;
	}
}

/*--------------------------------------
	con::CheckReadBufferSize
--------------------------------------*/
void con::CheckReadBufferSize()
{
	// Ensure buffer can hold a screenful of characters
	// FIXME: Set a minimum
	// FIXME: Round to next power of two?
	size_t numScreenBytes = NUM_MAX_LINES * (NUM_TXT_IMAGES + LOG_NEWLINE_LEN) + LEFT_PROBE +
		RIGHT_PROBE + 1;

	numScreenBytes = COM_MIN(numScreenBytes, LONG_MAX);

	if(readBuf.n != numScreenBytes)
	{
		readBuf.Init(numScreenBytes);
		readBuf[0] = 0;
		bufStart = bufEnd = -LEFT_PROBE;
	}
}

/*--------------------------------------
	con::UpdateReadBuffer

origin can be -1 (start), 0 (middle), 1 (end), or NO_ORIGIN (keep position).
--------------------------------------*/
void con::UpdateReadBuffer(long pos, int origin)
{
	if(bufEnd > logEnd + RIGHT_PROBE)
	{
		bufEnd = logEnd + RIGHT_PROBE;

		if(bufStart > bufEnd)
			bufStart = bufEnd;

		readBuf[bufEnd - bufStart] = 0;
	}

	// Get new buffer range
	size_t maxBufLen = readBuf.n - 1;
	long wantStart, wantEnd;

	if(origin == NO_ORIGIN)
	{
		wantStart = bufStart;
		wantEnd = bufEnd;
	}
	else
	{
		if(origin < 0)
			wantStart = pos - LEFT_PROBE;
		else if(origin == 0)
			wantStart = pos - LEFT_PROBE - long((maxBufLen - LEFT_PROBE - RIGHT_PROBE) / 2);
		else if(origin > 0)
			wantStart = pos + RIGHT_PROBE - long(maxBufLen);

		if(wantStart > logEnd)
			wantStart = logEnd;

		if(wantStart < -LEFT_PROBE)
			wantStart = -LEFT_PROBE;

		wantEnd = com::Min(wantStart + (long)maxBufLen, logEnd + RIGHT_PROBE);
	}

	// Get read range
	long readStart = wantStart, readEnd = wantEnd;

		// Reduce read range by using what's already buffered on the edges
	long shift = 0;

	if(bufStart <= wantStart)
	{
		if(bufEnd >= wantEnd)
		{
			// Everything's already buffered
			readStart = readEnd = wantStart;
			shift = wantStart - bufStart;
		}
		else if(bufEnd > wantStart)
		{
			// Start of wanted range is already in buffer
			readStart = bufEnd;
			shift = wantStart - bufStart;
		}
	}
	else if(bufEnd >= wantEnd && bufStart < wantEnd)
	{
		// End of wanted range is already in buffer
		readEnd = bufStart;
		shift = wantStart - bufStart;
	}

		// Expand read range to include updates
	if(updateStart != -1 && updateEnd >= wantStart && updateStart <= wantEnd)
	{
		readStart = com::Min(com::Max(wantStart, updateStart), readStart);
		readEnd = com::Max(com::Min(wantEnd, updateEnd), readEnd);
	}

	// Shift
	if(shift > 0)
	{
		for(long r = shift, w = 0; r <= bufEnd - bufStart; r++, w++)
			readBuf[w] = readBuf[r];
	}
	else if(shift < 0)
	{
		long rem = readBuf.n - 1 - (bufEnd - bufStart);
		long trunc = com::Max((long)0, -(rem + shift));
		long rInit = bufEnd - bufStart - 1 - trunc;
		long wInit = rInit - shift;

		for(long r = rInit, w = wInit; r >= 0; r--, w--)
			readBuf[w] = readBuf[r];
	}

	// Read
	if(readStart < readEnd)
	{
		long clampStart = readStart;

		if(readStart < 0)
		{
			for(long i = 0; i < -readStart; i++)
				readBuf[i] = 0;

			clampStart = 0;
		}

		long clampEnd = readEnd;

		if(readEnd > logEnd)
		{
			for(long i = readEnd - 1; i >= logEnd; i--)
				readBuf[i - wantStart] = 0;

			clampEnd = logEnd;
		}

		fseek(logFile, clampStart, SEEK_SET);
		fread(&readBuf[clampStart - wantStart], sizeof(char), clampEnd - clampStart, logFile);
	}

	readBuf[wantEnd - wantStart] = 0;
	bufStart = wantStart;
	bufEnd = wantEnd;

	updateStart = updateEnd = -1;
}

/*--------------------------------------
	con::JumpScrollPos
--------------------------------------*/
void con::JumpScrollPos(long pos)
{
	if(pos < 0)
		pos = 0;

	if(pos > logEnd)
		pos = logEnd;

	scrollPos = pos;

	if(scrollPos < bufStart + LEFT_PROBE || scrollPos > bufEnd - RIGHT_PROBE)
		UpdateReadBuffer(scrollPos, 0);
}

/*--------------------------------------
	con::NextVisible

Given pos is the starting position of a display line. Returns starting position of next display
line. Returned position will never be on top of a newline belonging to the given display line.
Note that due to this guarantee the returned position can be inside or past the right probe, so
don't blindly assign it to scrollPos. All characters in [pos, pos + visLen) exist before the
right probe. If fullOut is given, it's set to true if the given line wasn't cut off by a buffer
limit.
--------------------------------------*/
long con::NextVisible(long pos, size_t* visLenOut, bool* fullOut)
{
	size_t visLen = 0;
	bool full = false;

	while(pos < bufEnd - RIGHT_PROBE)
	{
		if(size_t newline = CheckNewline(pos))
		{
			pos += newline;
			full = true;
			break;
		}
		else
		{
			visLen++;
			pos++;

			if(visLen >= NUM_TXT_IMAGES)
			{
				full = true;

				/* Break if the next chars don't form a newline, otherwise let one more
				iteration go to get pos past the newline (which might be in the right probe).
				This way the newline is ignored since wrapping already takes care of it. */
				if(CheckNewline(pos) == 0)
					break;
			}
		}
	}

	if(pos == logEnd)
		full = true;

	if(visLenOut)
		*visLenOut = visLen;

	if(fullOut)
		*fullOut = full;

	return pos;
}

/*--------------------------------------
	con::CountVisible
--------------------------------------*/
void con::CountVisible(unsigned* numVisLinesOut, long* visEndOut)
{
	unsigned numVisLines = 0;
	long visEnd = -1;

	if(scrollPos < bufStart || scrollPos > bufEnd - RIGHT_PROBE)
		goto end;

	long pos = scrollPos;

	while(pos < bufEnd - RIGHT_PROBE)
	{
		bool full;
		pos = NextVisible(pos, 0, &full);
		numVisLines += full ? 1 : 0;

		if(numVisLines == numTxtLines)
			break;
	}

	visEnd = pos;

end:
	if(numVisLinesOut)
		*numVisLinesOut = numVisLines;

	if(visEndOut)
		*visEndOut = visEnd;
}

/*
################################################################################################


	UI LUA


################################################################################################
*/

/*--------------------------------------
LUA	con::SetOpen

IN	bVis
--------------------------------------*/
int con::SetOpen(lua_State* l)
{
	SetOpen(lua_toboolean(l, 1));
	return 0;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	con::Init
--------------------------------------*/
void con::Init()
{
	logFile = fopen("log.txt", "w+b");

	if(!logFile)
		WRP_FATAL("Could not open 'log.txt'");

	logEnd = ftell(logFile);
}

/*--------------------------------------
	con::InitLua
--------------------------------------*/
void con::InitLua()
{
	luaL_Reg regs[] =
	{
		{"LogF", con::LogF},
		{"AlertMode", con::AlertMode},
		{"SetAlertMode", con::SetAlertMode},
		{"CreateCommand", con::CreateCommand},
		{"ExecF", con::ExecF},
		{"ExecFile", con::ExecFile},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "gcon", regs, 0, 0, 0, 0);
	lua_pushcfunction(scr::state, Log); CreateCommand("log");
	lua_pushcfunction(scr::state, SetAlertMode); CreateCommand("set_alert_mode");
	lua_pushcfunction(scr::state, ExecFile); CreateCommand("exec_file");
	lua_pushcfunction(scr::state, SetOpen); CreateCommand("console");
	Option::RegisterAll();
}

/*--------------------------------------
	con::Update

FIXME: call every event (tick or frame) and check event keys
FIXME: typing latency, related to above?
FIXME: movable cursor
--------------------------------------*/
void con::Update()
{
	if(opened)
	{
		// Console controls
		if(in::Pressed(in::ESCAPE))
			SetOpen(false);

		if(in::Repeated(in::PAGE_DOWN))
			moveLines++;

		if(in::Repeated(in::PAGE_UP))
			moveLines--;

		// History
		int histKeys = in::Repeated(in::DOWN_ARROW) - in::Repeated(in::UP_ARROW);

		if(histKeys)
		{
			cmdHistScroll += histKeys;

			if(abs(cmdHistScroll) > cmdNumHist) cmdHistScroll = 0;

			if(cmdHistScroll)
			{
				char* histRestore;

				if(cmdHistScroll > 0)
				{
					if(cmdNumHist == CMD_HISTORY)
						histRestore = cmdHistLast + CMD_BUFFER_SIZE * cmdHistScroll;
					else
						histRestore = cmdHistBuffer + CMD_BUFFER_SIZE * (cmdHistScroll - 1);
				}
				else
					histRestore = cmdHistLast + CMD_BUFFER_SIZE * (cmdHistScroll + 1);

				if(histRestore < cmdHistBuffer)
					histRestore += CMD_BUFFER_SIZE * CMD_HISTORY;
				else if(histRestore >= cmdHistBuffer + CMD_BUFFER_SIZE * CMD_HISTORY)
					histRestore -= CMD_BUFFER_SIZE * CMD_HISTORY;

				strcpy(cmdBuffer, histRestore);
			}
			else
				cmdBuffer[0] = 0;
		}

		// Typing
		const char* kb = in::KeyBuffer();

		while(1)
		{
			kb += in::ApplyKeyBuffer(cmdBuffer, CMD_BUFFER_SIZE, kb, "\n");

			if(*kb == '\n')
			{
				if(cmdBuffer[0])
				{
					// Save to history
					if(cmdNumHist)
						cmdHistLast += CMD_BUFFER_SIZE;

					cmdNumHist = COM_MIN(cmdNumHist + 1, CMD_HISTORY);

					if(cmdHistLast - cmdHistBuffer >= CMD_BUFFER_SIZE * CMD_HISTORY)
						cmdHistLast = cmdHistBuffer;

					strcpy(cmdHistLast, cmdBuffer);
				}

				cmdHistScroll = 0;

				// Execute cmd
				ExecF("%s", cmdBuffer);
				memset(cmdBuffer, 0, CMD_BUFFER_SIZE);

				kb++; // Go past newline
			}
			else
				break;
		}
	}

	UpdateUI();

	// Toggle key
	if(in::Pressed(in::GRAVE) && !in::Down(in::SHIFT) && !in::Down(in::CTRL))
	{
		if(opened)
			scrollPos = logEnd; // Hide mini console upon closing full console
		else
			scrollPos = -1;

		memset(cmdBuffer, 0, CMD_BUFFER_SIZE);
		SetOpen(!opened);
	}
}