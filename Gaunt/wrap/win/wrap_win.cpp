// wrap_win.cpp -- Win32 wrapper
// Martynas Ceicys

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <MMSystem.h> //Winmm.lib
#include <stdlib.h>
#include <stdio.h>

#include "../wrap.h"
#include "../../audio/audio.h"
#include "../../common_lua/common_lua.h"
#include "../../console/console.h"
#include "../../flag/flag.h"
#include "../../../GauntCommon/io.h"
#include "../../gui/gui.h"
#include "../../hit/hit.h"
#include "../../input/input.h"
#include "../../mod/mod.h"
#include "../../path/path.h"
#include "../../quaternion/qua_lua.h"
#include "../../record/record.h"
#include "../../render/render.h"
#include "../../resource/resource.h"
#include "../../scene/scene.h"
#include "../../script/script.h"
#include "../../vector/vec_lua.h"

#include "wrap_win_key.h"
#include "wglengine.h"

#define WND_FULLSCREEN_STYLE	WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
	// Only WS_POPUP seems to be necessary for exclusive fullscreen, whatever
#define WND_WINDOWED_STYLE		WS_CAPTION | WS_SYSMENU
#define VID_DEFAULT_WIDTH		640
#define VID_DEFAULT_HEIGHT		480
#define LOOP_MAX_ACCUM_TIME		1010
#define FATAL_ERROR_LENGTH		256

namespace wrp
{
	extern void	LoadGL();

	// FIXME: wrap_lua.h

	// GENERAL LUA
	int			Quit(lua_State* l);
	int			FatalF(lua_State* l);
	int			Time(lua_State* l);
	int			SimTime(lua_State* l);
	int			SystemClock(lua_State* l);
	int			CastUnsigned(lua_State* l);

	// VIDEO
	void		SetVideoDefault(bool callGL);

	// VIDEO LUA
	int			SetVideo(lua_State* l);
	int			VideoWidth(lua_State* l);
	int			VideoHeight(lua_State* l);

	// LOOP
	void		SetTickMS(con::Option& optIO, float set);
	void		SetTickRate(con::Option& optIO, float set);
	void		SetMinFrameMS(con::Option& optIO, float set);
	void		SetMaxFrameRate(con::Option& optIO, float set);
	void		SetUpdateFactor(con::Option& optIO, float set);
	void		SetTimeStepFactor(con::Option& optIO, float set);

	// LOOP LUA
	int			ForceTimeStep(lua_State* l);
	int			ResetTimeStep(lua_State* l);
	int			Fraction(lua_State* l);
	int			NumFrames(lua_State* l);
	int			NumTicks(lua_State* l);

	// INPUT
	void		InitRawInput();
	void		SetLockCursor(bool lock);
	void		SendCursorInput(HWND hWnd, bool delta);
	void		CenterCursor(HWND hWnd);
	void		UpdateCursor();

	// INPUT LUA
	int			SetRawMouse(lua_State* l);
	int			SetLockCursor(lua_State* l);

	// MAIN
	void		Init();

	// --
	con::Option
		showFPS("wrp_show_fps", false),

		// How many normal-game millseconds should be simulated in a tick; integer
		tickMS("wrp_tick_ms", 16, SetTickMS),

		// Number of ticks per normal-game second
		tickRate("wrp_tick_rate", 0, SetTickRate),
		
		// Average target number of milliseconds between frames; 0 means no target
		minFrameMS("wrp_min_frame_ms", 0, SetMinFrameMS),

		// Another way to set minFrameMS
		maxFrameRate("wrp_max_frame_rate", 0, SetMaxFrameRate),

		// The number of real-world milliseconds between each tick calculation; integer
		updateMS("wrp_update_ms", 0, con::ReadOnly),

		/* Multiplies a real-world time span to get a normal-game time span. Changing this slows
		or speeds up time for the user by affecting sim frequency instead of sim calculations
		(and therefore keeping the final result consistent) */
		updateFactor("wrp_update_factor", 1.0f, SetUpdateFactor),

		// The number of normal-game seconds a tick should simulate
		baseTimeStep("wrp_base_time_step", 0, con::ReadOnly),

		/* The number of actual-game seconds a tick should simulate */
		origTimeStep("wrp_orig_time_step", 0, con::ReadOnly),

		/* Like origTimeStep but can be temporarily overridden for frame prediction; used in
		simulation calculations */
		timeStep("wrp_time_step", 0, con::ReadOnly),

		/* Multiplies a normal-game time span to get an actual-game time span. Changing this
		slows or speeds up time for the user by affecting sim calculations instead of sim
		frequency (and therefore changing the final result due to floating-point imprecision) */
		timeStepFactor("wrp_time_step_factor", 1.0f, SetTimeStepFactor),

		/* If true, every frame invokes a tick with a variable time step, which means framerate
		affects the simulation but the player's inputs have instant feedback without needing
		prediction */
		varTickRate("wrp_var_tick_rate", false),

		/* Clamp value of tick ms when it's tied to frame rate so a low frame rate slows the
		game down instead of simulating massive time spans */
		// FIXME: use this to limit number of fixed ticks simulated in one frame, too?
		maxTickMS("wrp_max_tick_ms", 10000, con::PositiveIntegerOnly);
		
		/* FIXME: add option to lock time step to what frame limiter's time span is supposed to
		be; game will slow down for user if framerate ever goes under but simulation
		calculations are kept consistent */
}

static struct
{
	HINSTANCE	hInstance;
	HWND		hWnd;
	HDC			hDC;
	HGLRC		hGLContext;
	HCURSOR		hCursor;
} wnd = {0};

static struct
{
	unsigned int	width;
	unsigned int	height;
	bool			fullScreen;
	bool			vSync;
	bool			show;
} vid = {VID_DEFAULT_WIDTH, VID_DEFAULT_HEIGHT, false, false, false};

static struct
{
	unsigned long long	time;
	DWORD				oldSysTime;
	float				fraction;
	unsigned long long	tickAccumTime;
	float				frameAccumTime;
	unsigned long long	frameWaitTime; // Like frameAccumTime but reset to 0 each frame
	float				avgMSPerFrame;
	unsigned long long	numFrames; // FIXME: save and load these accumulators
	unsigned long long	numTicks;

	// Milliseconds of total game time that should be simulated by end of current tick's work
		// i.e. this is "current time"; the tick needs to update the state to catch it up
	unsigned long long	simTime;
	float				simTimeFraction; // Partial milliseconds for small time-step factors
} loop = {0};

static size_t			resArraySize		= 0;
static unsigned int*	resArray			= 0;

static bool				rawMouse			= false; // FIXME: options
static bool				winLockCursor		= false;

LRESULT CALLBACK WinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	wrp::Quit
--------------------------------------*/
void wrp::Quit()
{
	PostQuitMessage(0);
}

/*--------------------------------------
	wrp::FatalF
--------------------------------------*/
void wrp::FatalF(const char* format, ...)
{
	char str[FATAL_ERROR_LENGTH] = {0};
	va_list args;
	va_start(args, format);
	com::VSNPrintF(str, FATAL_ERROR_LENGTH, 0, format, args);
	va_end(args);
	con::LogF("FATAL ERROR: %s", str);
	timeEndPeriod(1);
	ShowCursor(true);

	if(wnd.hWnd)
	{
		SetVideoDefault(false); // FIXME: don't do this, explicitly exit full screen mode
		DestroyWindow(wnd.hWnd);
	}

	MessageBoxA(0, str, "Fatal Error", 0);

	if(wnd.hDC)
		wglMakeCurrent(wnd.hDC, 0);

	if(wnd.hGLContext)
		wglDeleteContext(wnd.hGLContext);

	con::CloseLog();
	exit(1);
}

/*--------------------------------------
	wrp::Time

Milliseconds since game started.
--------------------------------------*/
unsigned long long wrp::Time()
{
	return loop.time;
}

/*--------------------------------------
	wrp::SystemClock

FIXME: set a standard, this is currently 100-nanosecond intervals since 1601
	do seconds since 1970?
--------------------------------------*/
unsigned long long wrp::SystemClock()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	unsigned long long hi = ft.dwHighDateTime;
	unsigned long long u = (hi << 32) + ft.dwLowDateTime;
	return u;
}

/*--------------------------------------
	wrp::RestrictedPath

Returns 0 or an error string.

FIXME: Links can still escape the working directory
--------------------------------------*/
const char* wrp::RestrictedPath(const char* path)
{
	if(*path == '\\' || *path == '/' || strpbrk(path, ":*?<>|") ||
	strstr(path, "..") || strstr(path, "\\\\") || strstr(path, "//"))
		return "Restricted path";

	static const char* BAD_TOKENS[] = {"CON", "AUX", "PRN", "NUL", "LPT1", "LPT2", "LPT3",
		"LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "LPT0", "COM1", "COM2", "COM3", "COM4",
		"COM5", "COM6", "COM7", "COM8", "COM9", "COM0"};

	for(size_t i = 0; i < sizeof(BAD_TOKENS) / sizeof(char*); i++)
	{
		if(com::StrStrTokLower(path, BAD_TOKENS[i], "/\\."))
			return "Restricted name";
	}

	return 0;
}

/*
################################################################################################


	GENERAL LUA


################################################################################################
*/

/*--------------------------------------
LUA	wrp::Quit
--------------------------------------*/
int wrp::Quit(lua_State* l)
{
	Quit();
	return 0;
}

/*--------------------------------------
LUA	wrp::FatalF

IN	sFormat, ...
--------------------------------------*/
int wrp::FatalF(lua_State* l)
{
	const char* format = lua_tostring(l, 1);
	if(!format) return 0;
	int numArgs = lua_gettop(l);

	lua_getglobal(l, "string");
	lua_getfield(l, -1, "format");
	lua_insert(l, 1);
	lua_remove(l, -1);
	lua_pcall(l, numArgs, 1, 0);

	const char* result = lua_tostring(l, 1);
	wrp::FatalF("%s", result);

	return 0;
}

/*--------------------------------------
LUA	wrp::Time

OUT	iTime
--------------------------------------*/
int wrp::Time(lua_State* l)
{
	lua_pushinteger(l, wrp::Time());
	return 1;
}

/*--------------------------------------
LUA	wrp::SimTime

OUT	iSimTime, nSimTimeFraction
--------------------------------------*/
int wrp::SimTime(lua_State* l)
{
	lua_pushinteger(l, wrp::SimTime());
	lua_pushnumber(l, wrp::SimTimeFraction());
	return 2;
}

/*--------------------------------------
LUA	wrp::SystemClock

OUT	iClock
--------------------------------------*/
int wrp::SystemClock(lua_State* l)
{
	lua_pushinteger(l, wrp::SystemClock());
	return 1;
}

/*--------------------------------------
LUA	wrp::CastUnsigned

IN	i
OUT	iUnsigned
--------------------------------------*/
int wrp::CastUnsigned(lua_State* l)
{
	lua_pushinteger(l, (unsigned)lua_tointeger(l, 1));
	return 1;
}

/*
################################################################################################


	VIDEO


################################################################################################
*/

/*--------------------------------------
	wrp::SetVideoDefault
--------------------------------------*/
void wrp::SetVideoDefault(bool callGL)
{
	con::LogF("Reverting to default video settings");
	ChangeDisplaySettingsEx(0, 0, 0, 0, 0);

	RECT wndRect = {0, 0, VID_DEFAULT_WIDTH, VID_DEFAULT_HEIGHT};
	AdjustWindowRect(&wndRect, WND_WINDOWED_STYLE, false);
	SetWindowLongPtr(wnd.hWnd, GWL_STYLE, WND_WINDOWED_STYLE);
	SetWindowPos(wnd.hWnd, HWND_TOP, 0, 0, wndRect.right - wndRect.left,
		wndRect.bottom - wndRect.top, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	ClipCursor(0);

	vid.width		= VID_DEFAULT_WIDTH;
	vid.height		= VID_DEFAULT_HEIGHT;
	vid.fullScreen	= false;
	vid.vSync		= false;

	if(callGL)
	{
		if(wglSwapIntervalEXT)
			wglSwapIntervalEXT(false);

		rnd::UpdateViewport();
	}
}

/*--------------------------------------
	wrp::SetVideo

Must be called at least once to show window.

FIXME: More mode options? (bpp, hertz)
FIXME: Minimize when alt-tab from fullscreen?
FIXME: Fullscreen is cut off when display scaling is > %100
--------------------------------------*/
bool wrp::SetVideo(unsigned width, unsigned height, bool fullScreen, bool vSync)
{
	vid.show = true;
	con::LogF("Set width: %u, height: %u, fullscreen: %d, vsync: %d", width, height,
		fullScreen, vSync);
	RECT wndRect = {0, 0, width, height};

	if(fullScreen)
	{
		// Set display resolution
		DEVMODE mode = {0};
		mode.dmSize			= sizeof(DEVMODE);
		mode.dmPelsWidth	= width;
		mode.dmPelsHeight	= height;
		mode.dmFields		= DM_PELSWIDTH | DM_PELSHEIGHT;

		if(LONG error = ChangeDisplaySettingsEx(0, &mode, 0, CDS_FULLSCREEN, 0) !=
		DISP_CHANGE_SUCCESSFUL)
		{
			CON_ERRORF("Display mode change failure (%li)", error);
			SetVideoDefault(true);
			return false;
		}

		SetWindowLongPtr(wnd.hWnd, GWL_STYLE, WND_FULLSCREEN_STYLE); // Set fullscreen style
		AdjustWindowRect(&wndRect, WND_FULLSCREEN_STYLE, false); // Get new window size
	}
	else
	{
		ChangeDisplaySettingsEx(0, 0, 0, 0, 0); // Reset display settings
		SetWindowLongPtr(wnd.hWnd, GWL_STYLE, WND_WINDOWED_STYLE); // Set windowed style
		AdjustWindowRect(&wndRect, WND_WINDOWED_STYLE, false); // Get new window size
	}

	// Set window size
	SetWindowPos(wnd.hWnd, HWND_TOP, 0, 0, wndRect.right - wndRect.left,
		wndRect.bottom - wndRect.top, SWP_SHOWWINDOW | SWP_FRAMECHANGED);

	// Test if window client area is requested size
	RECT testRect;
	GetClientRect(wnd.hWnd, &testRect);

	if(unsigned long(testRect.right - testRect.left) < width ||
	unsigned long(testRect.bottom - testRect.top) < height)
	{
		CON_ERROR("Window does not fit in screen");
		SetVideoDefault(true);
		return false;
	}

	// Set vertical sync
	wglSwapIntervalEXT(vSync);

	// Unclip cursor in case it was only clipped due to window being fullscreen
	if(!fullScreen)
		ClipCursor(0);

	// Success
	vid.width		= width;
	vid.height		= height;
	vid.fullScreen	= fullScreen;
	vid.vSync		= vSync;

	rnd::UpdateViewport();

	return true;
}

/*--------------------------------------
	wrp::VideoWidth
--------------------------------------*/
unsigned int wrp::VideoWidth()
{
	return vid.width;
}

/*--------------------------------------
	wrp::VideoHeight
--------------------------------------*/
unsigned int wrp::VideoHeight()
{
	return vid.height;
}

/*--------------------------------------
	wrp::CreateResolutionArray

Gets all supported resolutions of the primary display device and allocates an array where
even elements are width and odd elements are height. Returns element count.

Call ResolutionArray() to copy array of supported resolutions.
--------------------------------------*/
size_t wrp::CreateResolutionArray()
{
	size_t modeCount = 0;
	size_t resCount = 0;

	DEVMODE mode = {0};
	mode.dmSize = sizeof(DEVMODE);

	// Count display modes
	for(DWORD i = 0; EnumDisplaySettings(0, i, &mode); i++) modeCount++;
	if(!modeCount) return 0;

	// Get supported resolutions
	unsigned int* modeWidth = new unsigned int[modeCount];
	unsigned int* modeHeight = new unsigned int[modeCount];
	for(DWORD i = 0; EnumDisplaySettings(0, i, &mode); i++)
	{
		modeWidth[i] = mode.dmPelsWidth;
		modeHeight[i] = mode.dmPelsHeight;
	}

	// Sort supported resolutions
	for(size_t i = 0; i < modeCount; i++)
	{
		for(size_t j = 1; j < modeCount; j++)
		{
			if(modeWidth[j] < modeWidth[j - 1] || (modeWidth[j] == modeWidth[j - 1] && modeHeight[j] < modeHeight[j - 1]))
			{
				unsigned int temp[2] = {modeWidth[j], modeHeight[j]};
				modeWidth[j] = modeWidth[j - 1];
				modeWidth[j - 1] = temp[0];
				modeHeight[j] = modeHeight[j - 1];
				modeHeight[j - 1] = temp[1];
			}
		}
	}

	// Count unique resolutions
	for(size_t i = 1; i < modeCount; i++) if(modeWidth[i] != modeWidth[i - 1] || modeHeight[i] != modeHeight[i - 1]) resCount++;

	// Get unique resolutions
	if(resArray) delete[] resArray;
	resArraySize = resCount * 2;
	resArray = new unsigned int[resArraySize];

	size_t j = 0;
	for(size_t i = 1; i < modeCount; i++)
	{
		if(modeWidth[i] != modeWidth[i - 1] || modeHeight[i] != modeHeight[i - 1])
		{
			resArray[j] = modeWidth[i];
			resArray[j + 1] = modeHeight[i];
			j += 2;
		}
	}

	delete[] modeWidth;
	delete[] modeHeight;

	return resArraySize;
}

/*--------------------------------------
	wrp::ResolutionArraySize
--------------------------------------*/
size_t wrp::ResolutionArraySize()
{
	return resArraySize;
}

/*--------------------------------------
	wrp::CopyResolutionArray
--------------------------------------*/
bool wrp::CopyResolutionArray(const size_t size, unsigned int* resArrayOut)
{
	if(!resArray || !resArrayOut) return false;
	memcpy(resArrayOut, resArray, min(size, resArraySize) * sizeof(unsigned int));
	return true;
}

/*
################################################################################################


	VIDEO LUA


################################################################################################
*/

/*--------------------------------------
LUA	wrp::SetVideo

IN	iWidth, iHeight, bFullScreen, bVSync
OUT	[bSuccess]
--------------------------------------*/
int wrp::SetVideo(lua_State* l)
{
	if(!lua_gettop(l))
	{
		con::LogF("width: %u, height: %u, fullscreen: %d, vsync: %d",
			VideoWidth(), VideoHeight(), vid.fullScreen, vid.vSync);
		return 0;
	}
	else if(lua_gettop(l) < 4)
		luaL_error(l, "Expected 4 arguments; iWidth, iHeight, bFullScreen, bVSync");

	unsigned width = lua_tointeger(l, 1);
	unsigned height = lua_tointeger(l, 2);
	bool fullScreen = lua_toboolean(l, 3) != 0;
	bool vSync = lua_toboolean(l, 4) != 0;
	lua_pushboolean(l, SetVideo(width, height, fullScreen, vSync));
	return 1;
}

/*--------------------------------------
LUA	wrp::VideoWidth

OUT	iVideoWidth
--------------------------------------*/
int wrp::VideoWidth(lua_State* l)
{
	lua_pushinteger(l, vid.width);
	return 1;
}

/*--------------------------------------
LUA	wrp::VideoHeight

OUT	iVideoHeight
--------------------------------------*/
int wrp::VideoHeight(lua_State* l)
{
	lua_pushinteger(l, vid.height);
	return 1;
}

/*
################################################################################################


	LOOP

FIXME: Record system needs to save and load appropriate loop timing stuff
################################################################################################
*/

/*--------------------------------------
	wrp::SetTickMS
--------------------------------------*/
void wrp::SetTickMS(con::Option& opt, float set)
{
	int ms = set;

	if(ms <= 0)
	{
		con::AlertF("Milliseconds per tick cannot be %d, must be > 0", ms);
		return;
	}

	opt.ForceValue(ms);
	tickRate.ForceValue(1000.0f / ms);
	baseTimeStep.ForceValue(ms / 1000.0f);
	timeStepFactor.SetValue(timeStepFactor.Float(), false); // Updates timeStep
	int upd = (int)(ms / updateFactor.Float());

	if(upd <= 0)
		upd = 1;

	updateMS.ForceValue(upd);
}

/*--------------------------------------
	wrp::SetTickRate
--------------------------------------*/
void wrp::SetTickRate(con::Option& opt, float set)
{
	if(set <= 0.0f)
	{
		con::AlertF("Tick rate cannot be %g, must be > 0", set);
		return;
	}

	tickMS.SetValue(1000.0f / set);
}

/*--------------------------------------
	wrp::SetMinFrameMS
--------------------------------------*/
void wrp::SetMinFrameMS(con::Option& opt, float set)
{
	if(set < 0.0f || set > 1000.0f)
	{
		con::AlertF("Min frame MS cannot be %g, must be [0, 1000]", set);
		return;
	}

	minFrameMS.ForceValue(set);

	if(set > 0.0f)
		maxFrameRate.ForceValue(1000.0f / set);
	else
		maxFrameRate.ForceValue(0.0f);
	
	loop.frameAccumTime = loop.frameWaitTime = 0.0f;
}

/*--------------------------------------
	wrp::SetMaxFrameRate
--------------------------------------*/
void wrp::SetMaxFrameRate(con::Option& opt, float set)
{
	minFrameMS.SetValue(set > 0.0f ? 1000.0f / set : 0.0f);
}

/*--------------------------------------
	wrp::SetUpdateFactor
--------------------------------------*/
void wrp::SetUpdateFactor(con::Option& opt, float set)
{
	if(set <= 0.0f)
	{
		con::AlertF("Update factor cannot be %g, must be > 0", set);
		return;
	}

	updateFactor.ForceValue(set);
	tickMS.SetValue(tickMS.Float());
}

/*--------------------------------------
	wrp::SetTimeStepFactor
--------------------------------------*/
void wrp::SetTimeStepFactor(con::Option& opt, float set)
{
	if(set <= 0.0f)
	{
		con::AlertF("Time step factor cannot be %g, must be > 0", set);
		return;
	}

	opt.ForceValue(set);
	origTimeStep.ForceValue(baseTimeStep.Float() * set);
	timeStep.ForceValue(origTimeStep.Float());
}

/*--------------------------------------
	wrp::ForceTimeStep

FIXME: Remove if not using
--------------------------------------*/
bool wrp::ForceTimeStep(float f)
{
	if(f <= 0.0f)
		return false;

	timeStep.ForceValue(f);
	return true;
}

/*--------------------------------------
	wrp::ResetTimeStep

FIXME: Remove if not using
--------------------------------------*/
void wrp::ResetTimeStep()
{
	timeStep.ForceValue(origTimeStep.Float());
}

/*--------------------------------------
	wrp::Fraction
--------------------------------------*/
float wrp::Fraction()
{
	return loop.fraction;
}

/*--------------------------------------
	wrp::NumFrames
--------------------------------------*/
unsigned long long wrp::NumFrames()
{
	return loop.numFrames;
}

/*--------------------------------------
	wrp::NumTicks
--------------------------------------*/
unsigned long long wrp::NumTicks()
{
	return loop.numTicks;
}

/*--------------------------------------
	wrp::SimTime

FIXME: reset on level load
--------------------------------------*/
unsigned long long wrp::SimTime()
{
	return loop.simTime;
}

/*--------------------------------------
	wrp::SimTimeFraction
--------------------------------------*/
float wrp::SimTimeFraction()
{
	return loop.simTimeFraction;
}

/*--------------------------------------
	wrp::ForceFrame

Handles Windows messages, updates command console, and renders a frame. Returns false if no more
frames can be forced until the application returns to the main loop (caller should break out of
any pausing loop).

FIXME: make sure time vars are being updated correctly; sim afterwards shouldn't be stopped or skipping forward
FIXME: generalize main loop more so that this function has less copy-pasting?
FIXME: limit framerate
--------------------------------------*/
bool wrp::ForceFrame()
{
	if(!ForceFrameStart())
		return false;

	ForceFrameEnd();
	return true;
}

/*--------------------------------------
	wrp::ForceFrameStart
--------------------------------------*/
bool wrp::ForceFrameStart()
{
	MSG msg;

	while(PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
	{
		if(msg.message == WM_QUIT)
			exit(0); // FIXME: do a proper quit that saves logs, etc.

		PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
		DispatchMessage(&msg);
	}

	DWORD sysTime = timeGetTime();
	unsigned long timeDelta = sysTime - loop.oldSysTime;
	loop.time += timeDelta;
	loop.oldSysTime = sysTime;
	con::Update();
	return true;
}

/*--------------------------------------
	wrp::ForceFrameEnd
--------------------------------------*/
void wrp::ForceFrameEnd()
{
	in::ClearTick();
	in::ClearFrame();
	loop.numFrames++;
	loop.frameAccumTime = 0.0f;
	loop.frameWaitTime = 0;
	loop.fraction = 1.0f;
	loop.tickAccumTime = 0;
	rnd::Frame();
	SwapBuffers(wnd.hDC);
}

/*
################################################################################################


	LOOP LUA


################################################################################################
*/

/*--------------------------------------
LUA	wrp::ForceTimeStep

IN	nStep
OUT	bForced
--------------------------------------*/
int wrp::ForceTimeStep(lua_State* l)
{
	lua_pushboolean(l, wrp::ForceTimeStep(luaL_checknumber(l, 1)) != 0);
	return 1;
}

/*--------------------------------------
LUA	wrp::ResetTimeStep
--------------------------------------*/
int wrp::ResetTimeStep(lua_State* l)
{
	wrp::ResetTimeStep();
	return 0;
}

/*--------------------------------------
LUA	wrp::Fraction

OUT	nFraction
--------------------------------------*/
int wrp::Fraction(lua_State* l)
{
	lua_pushnumber(l, Fraction());
	return 1;
}

/*--------------------------------------
LUA	wrp::NumFrames

OUT	iNumFrames
--------------------------------------*/
int wrp::NumFrames(lua_State* l)
{
	lua_pushinteger(l, wrp::NumFrames());
	return 1;
}

/*--------------------------------------
LUA	wrp::NumTicks

OUT	iNumTicks
--------------------------------------*/
int wrp::NumTicks(lua_State* l)
{
	lua_pushinteger(l, wrp::NumTicks());
	return 1;
}

/*
################################################################################################


	INPUT


################################################################################################
*/

/*--------------------------------------
	wrp::InitRawInput
--------------------------------------*/
void wrp::InitRawInput()
{
	RAWINPUTDEVICE keyboard;
	keyboard.usUsagePage	= 1;
	keyboard.usUsage		= 6;
	keyboard.dwFlags		= 0;
	keyboard.hwndTarget		= 0;
	// FIXME: Disable some Windows shortcut stuff (ALT index keys)

	if(!RegisterRawInputDevices(&keyboard, 1, sizeof(RAWINPUTDEVICE)))
		WRP_FATALF("Raw key input registration failure (CODE %u)", (unsigned)GetLastError());

	RAWINPUTDEVICE mouse;
	mouse.usUsagePage		= 1;
	mouse.usUsage			= 2;
	mouse.dwFlags			= 0;
	mouse.hwndTarget		= 0;

	if(!RegisterRawInputDevices(&mouse, 1, sizeof(RAWINPUTDEVICE)))
		// Use default Windows mouse in this case
		con::LogF("Raw mouse input registration failure (CODE %u)", (unsigned)GetLastError());
}

/*--------------------------------------
	wrp::SetLockCursor
--------------------------------------*/
void wrp::SetLockCursor(bool lock)
{
	winLockCursor = lock;

	if(lock)
		CenterCursor(wnd.hWnd);
	else
		ClipCursor(0);
}

/*--------------------------------------
	wrp::SendCursorInput
--------------------------------------*/
void wrp::SendCursorInput(HWND hWnd, bool delta)
{
	POINT cursor;
	GetCursorPos(&cursor);

	if(delta)
	{
		RECT rect;
		GetWindowRect(hWnd, &rect);
		POINT center = {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
		in::AddMouseDelta(0, cursor.x - center.x);
		in::AddMouseDelta(1, cursor.y - center.y);
	}
	else
	{
		RECT rect;
		GetClientRect(hWnd, &rect);
		POINT client = cursor;
		ScreenToClient(hWnd, &client);

		if(client.x < 0 || client.x >= (rect.right - rect.left) ||
		client.y < 0 || client.y > (rect.bottom - rect.top))
			return;

		in::SetMouse(0, client.x);
		in::SetMouse(1, client.y);
	}
}

/*--------------------------------------
	wrp::CenterCursor
--------------------------------------*/
void wrp::CenterCursor(HWND hWnd)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	POINT center = {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
	SetCursorPos(center.x, center.y);
}

/*--------------------------------------
	wrp::UpdateCursor
--------------------------------------*/
void wrp::UpdateCursor()
{
	if(GetFocus() != NULL)
	{
		bool lockCursor = winLockCursor || vid.fullScreen;

		if(lockCursor)
		{
			/* Make sure cursor clip space is up to date so user can't swipe their mouse and
			click outside the window */
			RECT rect;

#if 1
			GetClientRect(wnd.hWnd, &rect);
			MapWindowPoints(wnd.hWnd, 0, (LPPOINT)(&rect), 2);
#else
			GetWindowRect(wnd.hWnd, &rect);
#endif

			RECT clip;
			GetClipCursor(&clip);

			if(!EqualRect(&rect, &clip))
			{
				ClipCursor(&rect);

				// Confine cursor to new clip rect immediately, before it's read for input
				POINT cur;
				GetCursorPos(&cur);
				SetCursorPos(cur.x, cur.y);
			}
		}

		if(!rawMouse)
			SendCursorInput(wnd.hWnd, lockCursor);

		if(lockCursor)
			CenterCursor(wnd.hWnd);
	}
}

/*
################################################################################################


	INPUT LUA


################################################################################################
*/

/*--------------------------------------
LUA	wrp::SetRawMouse

IN	[bRawMouse]
--------------------------------------*/
int wrp::SetRawMouse(lua_State* l)
{
	if(lua_gettop(l))
		rawMouse = lua_toboolean(l, 1) != 0;
	con::LogF("Raw mouse: %s", rawMouse ? "true" : "false");
	return 0;
}

/*--------------------------------------
LUA	wrp::SetLockCursor

IN	[bLockCursor]
--------------------------------------*/
int wrp::SetLockCursor(lua_State* l)
{
	if(lua_gettop(l))
		SetLockCursor(lua_toboolean(l, 1) != 0);
	con::LogF("Window-lock cursor: %s", winLockCursor ? "true" : "false");
	return 0;
}

/*
################################################################################################


	MAIN


################################################################################################
*/

/*--------------------------------------
	wrp::Init

Call SetVideo to show window.

FIXME: Print GetLastError
--------------------------------------*/
void wrp::Init()
{
	//SetProcessDPIAware(); // FIXME: necessary for exclusive fullscreen? (but rendering isn't actually DPI aware)

	// Register window class
	WNDCLASS wc = {0};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WinProc;
	wc.hInstance = wnd.hInstance;
	wc.lpszClassName = "GauntClass";

	if(!RegisterClass(&wc))
		WRP_FATAL("Window class registration failure");

	// Create window
	wnd.hWnd = CreateWindow
	(
		wc.lpszClassName,
		"Gaunt",
		WND_WINDOWED_STYLE,
		0,
		0,
		0,
		0,
		0,
		0,
		wnd.hInstance,
		0
	);

	if(!wnd.hWnd)
		WRP_FATAL("Window creation failure");

	// Create gl context
	wnd.hDC = GetDC(wnd.hWnd);

	if(!wnd.hDC)
		WRP_FATAL("Get device context failure");

	PIXELFORMATDESCRIPTOR pfd = {0};
	pfd.nSize			= sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion		= 1;
	pfd.dwFlags			= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER; // FIXME: if using FBOs, double buffering is probably not needed
	pfd.iPixelType		= PFD_TYPE_RGBA;
	pfd.cColorBits		= 24;
	pfd.cDepthBits		= 24; // FIXME: if using FBOs, then depth, stencil, and alpha are not needed in the main framebuffer
	pfd.cStencilBits	= 8;
	pfd.cAlphaBits		= 8;

	int pfi = ChoosePixelFormat(wnd.hDC, &pfd);
	if(!SetPixelFormat(wnd.hDC, pfi, &pfd))
		WRP_FATAL("Pixel format set failure");

	wnd.hGLContext = wglCreateContext(wnd.hDC);
	
	if(!wnd.hGLContext)
		WRP_FATAL("GL context create failure");

	if(!wglMakeCurrent(wnd.hDC, wnd.hGLContext))
		WRP_FATAL("GL context set failure");

	LoadGL();
	InitRawInput();
	tickMS.SetValue(tickMS.Integer());
	wnd.hCursor = LoadCursor(0, IDC_ARROW);

	// Register lua functions
	luaL_Reg regs[] =
	{
		{"Quit", Quit},
		{"FatalF", FatalF},
		{"Time", Time},
		{"SimTime", SimTime},
		{"SystemClock", SystemClock},
		{"CastUnsigned", CastUnsigned},
		{"VideoWidth", VideoWidth},
		{"VideoHeight", VideoHeight},
		{"SetRawMouse", SetRawMouse},
		{"ForceTimeStep", ForceTimeStep},
		{"ResetTimeStep", ResetTimeStep},
		{"Fraction", Fraction},
		{"NumFrames", NumFrames},
		{"NumTicks", NumTicks},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "gwrp", regs, 0, 0, 0, 0);

	// Create console cmds
	lua_pushcfunction(scr::state, Quit); con::CreateCommand("quit");
	lua_pushcfunction(scr::state, SetVideo); con::CreateCommand("video");
	lua_pushcfunction(scr::state, SetRawMouse); con::CreateCommand("raw_mouse");
	lua_pushcfunction(scr::state, SetLockCursor); con::CreateCommand("lock_cursor");
}

/*--------------------------------------
	WinMain
--------------------------------------*/
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	wnd.hInstance = hInstance;

	con::Init();
	scr::Init();
	res::Init();
	comlua::Init();
	con::InitLua();
	vec::Init();
	qua::Init();
	flg::Init();
	in::Init();
	scn::Init();
	hit::Init();
	gui::Init();
	pat::Init();
	wrp::Init();
	rnd::Init();

	if(!aud::Init())
		con::LogF("Audio is disabled");

	rec::Init();
	mod::Init(lpCmdLine);
	con::InitUI();
	gui::QuickDrawInit();

	if(!vid.show)
		wrp::SetVideo(vid.width, vid.height, vid.fullScreen, vid.vSync);

	mod::GameInit();

	timeBeginPeriod(1); // FIXME: reset when window is out of focus?
		// FIXME: use QueryPerformanceCounter?
	loop.time = 0;
	loop.oldSysTime = timeGetTime();
	loop.numFrames = 1; // FIXME: start at 0 and increment before update? what's more useful?
	loop.numTicks = 1;
	loop.simTime = 0;
	loop.simTimeFraction = 0.0f;

	MSG msg;
	PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE);
	while(msg.message != WM_QUIT)
	{
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			DispatchMessage(&msg);
		else
		{
			DWORD sysTime = timeGetTime();
			unsigned long timeDelta = sysTime - loop.oldSysTime;
			loop.time += timeDelta;
			loop.oldSysTime = sysTime;
			bool cursorUpdated = false;
			bool play = !con::Opened();
			unsigned updateMSU = wrp::updateMS.Unsigned();
			bool fixedTimeStep = !wrp::varTickRate.Bool();
			float minFrameMSF = wrp::minFrameMS.Float();
			loop.frameAccumTime += timeDelta;
			loop.frameWaitTime += timeDelta;

			if(play)
			{
				if(fixedTimeStep)
				{
					loop.tickAccumTime += timeDelta;

					// Slow down if falling behind
					// FIXME: When simulation is slower than simulated time, prefer slow down, death spiral, or lower precision?
					if(loop.tickAccumTime > LOOP_MAX_ACCUM_TIME)
						loop.tickAccumTime = updateMSU;
				}
				else
				{
					// Force tick if frame is going to be calculated
					if(loop.frameAccumTime >= minFrameMSF && loop.frameWaitTime)
					{
						loop.tickAccumTime = updateMSU;

						wrp::tickMS.SetValue(com::Min<unsigned long long>(loop.frameWaitTime,
							wrp::maxTickMS.Unsigned()), false);
						// FIXME: restore original setting when varTickRate is turned off
					}
					else
						loop.tickAccumTime = 0;
				}

				while(loop.tickAccumTime >= updateMSU)
				{
					loop.tickAccumTime -= updateMSU;

					if(!cursorUpdated)
					{
						wrp::UpdateCursor();
						cursorUpdated = true;
					}

					rnd::PreTick();
					scn::SaveSpace();
					aud::SaveOld();

					loop.simTimeFraction += wrp::timeStep.Float() * 1000.0f;
					float whole = floorf(loop.simTimeFraction);
					loop.simTime += whole;
					loop.simTimeFraction -= whole;

					mod::GameTick();
					scn::CallEntityFunctions(scn::ENT_FUNC_TICK);
					mod::GamePostTick();
					pat::PostTick();
					con::Update();
					in::ClearTick();
					loop.numTicks++;
				}
			}
			else
			{
				/* FIXME: This branch runs too often on powerful CPUs, limit with a timer (or
				get rid of ticks and just do it per frame). Call to UpdateCursor is concerning
				in particular because calling SetCursorPos too often seems to make explorer lag
				for a while after the window loses focus (though for some reason I can't
				recreate this problem here, only when UpdateCursor is done outside of it). */
				if(!cursorUpdated)
				{
					wrp::UpdateCursor();
					cursorUpdated = true;
				}

				con::Update();
				in::ClearTick();
				loop.tickAccumTime = 0;
			}

			if(loop.frameAccumTime >= minFrameMSF && loop.frameWaitTime)
			{
				loop.frameAccumTime -= minFrameMSF; // FIXME: this means accum time keeps going up to hard limit if minFrameMS is 0
					// FIXME: this also causes frame rate to go beyond limit after it recovers from slowdown

				if(loop.frameAccumTime > 1010.0f)
					loop.frameAccumTime = 0.0f;

				if(wrp::showFPS.Bool())
				{
					loop.avgMSPerFrame = loop.frameWaitTime * 0.05f + loop.avgMSPerFrame * 0.95f;
					con::LogF("FPS: %.0f", 1000.0f / loop.avgMSPerFrame); // FIXME: special GUI (a per-frame log?)
				}

				loop.frameWaitTime = 0;
				loop.fraction = fixedTimeStep ? (float)loop.tickAccumTime/updateMSU : 1.0f;

				if(!cursorUpdated)
				{
					wrp::UpdateCursor();
					cursorUpdated = true;
				}

				if(play)
				{
					mod::GameFrame();
					wrp::ResetTimeStep(); // Just in case it was forced and not reset
				}

				aud::Update();
				rnd::Frame();
				gui::QuickDrawAdvance();
				SwapBuffers(wnd.hDC);
				rec::Update();
				in::ClearFrame();
				loop.numFrames++;
			}
		}
	}

	timeEndPeriod(1);
	ClipCursor(0);
	wglMakeCurrent(wnd.hDC, 0);
	wglDeleteContext(wnd.hGLContext);
	aud::CleanUp();
	con::CloseLog();
	//_CrtDumpMemoryLeaks(); //FIXME TEMP
}

/*--------------------------------------
	WinProc

FIXME: release all keys when focus is lost
--------------------------------------*/
LRESULT CALLBACK WinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_CLOSE:
		wrp::Quit();
		break;
	case WM_INPUT:
		{
			HRAWINPUT hRawInput = (HRAWINPUT)lParam;
			UINT size;

			GetRawInputData(hRawInput, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));

			if(size == 0)
			{
				con::LogF("GetRawInputData failure, size is 0");
				return 0;
			}

			BYTE* bytes = new BYTE[size]; // FIXME: don't malloc every time; keep a resizeable array or alloc once and check size after

			UINT retSize;
			if((retSize = GetRawInputData(hRawInput, RID_INPUT, bytes, &size, sizeof(RAWINPUTHEADER))) != size)
			{
				con::LogF("GetRawInputData failure, wrong size returned (%u instead of %u)", retSize, size);
				delete[] bytes;
				return 0;
			}

			RAWINPUT* raw = (RAWINPUT*)bytes;

			if(raw->header.dwType == RIM_TYPEKEYBOARD)
			{
				unsigned int keyCode; // FIXME: differentiate between left shift and right shift (and other duplicate keys) so one's release does not cancel the other
				raw->data.keyboard.VKey == VK_RETURN && raw->data.keyboard.Flags & RI_KEY_E0 ? keyCode = in::NUMPAD_ENTER : keyCode = vKeyConvert[raw->data.keyboard.VKey];
				raw->data.keyboard.Flags & RI_KEY_BREAK ? in::SetReleased(keyCode) : in::SetPressed(keyCode);
			}
			else if(raw->header.dwType == RIM_TYPEMOUSE && rawMouse)
			{
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) in::SetPressed(in::MOUSE_BUTTON_0);
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) in::SetPressed(in::MOUSE_BUTTON_1);
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) in::SetPressed(in::MOUSE_BUTTON_2);
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP) in::SetReleased(in::MOUSE_BUTTON_0);
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP) in::SetReleased(in::MOUSE_BUTTON_1);
				if(raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP) in::SetReleased(in::MOUSE_BUTTON_2);

				if(raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
				{
					if((SHORT)raw->data.mouse.usButtonData > 0)
					{
						in::SetPressed(in::MOUSE_WHEEL_UP);
						in::SetReleased(in::MOUSE_WHEEL_UP);
					}
					else if((SHORT)raw->data.mouse.usButtonData < 0)
					{
						in::SetPressed(in::MOUSE_WHEEL_DOWN);
						in::SetReleased(in::MOUSE_WHEEL_DOWN);
					}
				}

				if(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
				{
					in::SetMouse(0, raw->data.mouse.lLastX);
					in::SetMouse(1, raw->data.mouse.lLastY);
				}
				else
				{
					in::AddMouseDelta(0, raw->data.mouse.lLastX);
					in::AddMouseDelta(1, raw->data.mouse.lLastY);
				}
			}

			delete[] bytes;
		}
		break;
	case WM_LBUTTONDOWN:
		if(!rawMouse) in::SetPressed(in::MOUSE_BUTTON_0);
		break;
	case WM_LBUTTONUP:
		if(!rawMouse) in::SetReleased(in::MOUSE_BUTTON_0);
		break;
	case WM_RBUTTONDOWN:
		if(!rawMouse) in::SetPressed(in::MOUSE_BUTTON_1);
		break;
	case WM_RBUTTONUP:
		if(!rawMouse) in::SetReleased(in::MOUSE_BUTTON_1);
		break;
	case WM_MBUTTONDOWN:
		if(!rawMouse) in::SetPressed(in::MOUSE_BUTTON_2);
		break;
	case WM_MBUTTONUP:
		if(!rawMouse) in::SetReleased(in::MOUSE_BUTTON_2);
		break;
	case WM_MOUSEWHEEL:
		if(!rawMouse)
		{
			short delta = GET_WHEEL_DELTA_WPARAM(wParam);
			if(delta > 0)
			{
				in::SetPressed(in::MOUSE_WHEEL_UP);
				in::SetReleased(in::MOUSE_WHEEL_UP);
			}
			else if(delta < 0)
			{
				in::SetPressed(in::MOUSE_WHEEL_DOWN);
				in::SetReleased(in::MOUSE_WHEEL_DOWN);
			}
		}
		break;
	case WM_SETCURSOR:
		if(LOWORD(lParam) == HTCLIENT)
			SetCursor(0);
		else
			SetCursor(wnd.hCursor);
		break;
	case WM_SYSCOMMAND:
		if(wParam == SC_KEYMENU)
			return 0; // Suppress keys focusing on non-existent menu
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}