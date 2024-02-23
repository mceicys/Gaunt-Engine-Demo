// input.cpp
// Martynas Ceicys

#include <string.h>

#include "input.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../../GauntCommon/math.h"
#include "../resource/resource.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

#include "input_const.h"

#define KEY_BUFFER_SIZE	128

namespace in
{
	// KEY
	void	InitKeyBuffer(char*& kbOut);
	void	ClearKeyBuffer(char* kbIO);
	void	UpdateKeyBuffer(char* kbIO, unsigned k);

	// ACTION LUA
	int	EnsureAction(lua_State* l);
	int	BindAction(lua_State* l);
	int	FindAction(lua_State* l);
	int	ActionBoundCode(lua_State* l);
	int	ActionBoundString(lua_State* l);

	// KEY LUA
	int	Pressed(lua_State* l);
	int	Repeated(lua_State* l);
	int	Down(lua_State* l);
	int	Released(lua_State* l);
	int	PressedFrame(lua_State* l);
	int RepeatedFrame(lua_State* l);
	int	DownFrame(lua_State* l);
	int	ReleasedFrame(lua_State* l);
	int	KeyBuffer(lua_State* l);
	int	KeyBufferFrame(lua_State* l);
	int	ApplyKeyBufferGeneric(lua_State* l, const char* kb);
	int	ApplyKeyBuffer(lua_State* l);
	int	ApplyKeyBufferFrame(lua_State* l);
	int	KeyCode(lua_State* l);
	int	KeyString(lua_State* l);

	// POINT LUA
	int	Mouse(lua_State* l);
	int	MouseF(lua_State* l);
	int	MouseEventDelta(lua_State* l);
	int	MouseEventDeltaF(lua_State* l);
	int	MouseTickDelta(lua_State* l);
	int	MouseTickDeltaF(lua_State* l);
}

static bool			keyPressed[in::NUM_KEYS]		= {0};
static bool			keyRepeated[in::NUM_KEYS]		= {0};
static bool			keyDown[in::NUM_KEYS]			= {0};
static bool			keyReleased[in::NUM_KEYS]		= {0};

static bool			keyPressedFrame[in::NUM_KEYS]	= {0};
static bool			keyRepeatedFrame[in::NUM_KEYS]	= {0};
static bool			keyDownFrame[in::NUM_KEYS]		= {0};
static bool			keyReleasedFrame[in::NUM_KEYS]	= {0};

static char*		keyBuffer;
static char*		keyBufferFrame;

static int			ptValue[in::NUM_POINTS]			= {0};

/*
################################################################################################


	ACTION

Actions are strings bound to keys. The game must create its actions when initialized.
################################################################################################
*/

// FIXME: make array

class in::Action : public res::Resource<in::Action>
{
public:
	char*			name;
	unsigned int	binding;
};

const char* const in::Action::METATABLE = "metaAction";

/*--------------------------------------
	in::EnsureAction

FIXME: always store in all caps
--------------------------------------*/
in::Action* in::EnsureAction(const char* name)
{
	if(Action* a = FindAction(name))
		return a;

	Action* a = new Action;
	a->name = new char[strlen(name) + 1];
	strcpy(a->name, name);
	a->binding = NULL_KEY;
	a->EnsureLink();
	return a;
}

/*--------------------------------------
	in::BindAction
--------------------------------------*/
void in::BindAction(Action* a, unsigned int k)
{
	if(k >= NUM_KEYS)
		return;

	a->binding = k;
	con::LogF("Bound %s to %s", a->name, keyString[k]);
}

/*--------------------------------------
	in::FindAction
--------------------------------------*/
in::Action* in::FindAction(const char* name)
{
	for(com::linker<Action>* it = Action::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->name, name))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	in::ActionBoundCode

FIXME: these should be methods of Action
--------------------------------------*/
unsigned in::ActionBoundCode(const Action* a)
{
	return a->binding;
}

/*--------------------------------------
	in::ActionBoundString
--------------------------------------*/
const char* in::ActionBoundString(const Action* a)
{
	return KeyString(a->binding);
}

/*
################################################################################################


	ACTION LUA


################################################################################################
*/

/*--------------------------------------
LUA	in::EnsureAction

IN	sName
OUT	act
--------------------------------------*/
int in::EnsureAction(lua_State* l)
{
	const char* name = luaL_checkstring(l, 1);
	Action* a = EnsureAction(name);
	a->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	in::BindAction

IN	actA || sName, sK
--------------------------------------*/
int in::BindAction(lua_State* l)
{
	Action* a;
	if(!(a = Action::LuaTo(1)))
	{
		const char* name = luaL_checkstring(l, 1);
		a = FindAction(name);
		if(!a)
			luaL_error(l, "no action '%s'", name);
	}
	
	const char* k = luaL_checkstring(l, 2);
	int code = KeyCode(k);
	BindAction(a, code);
	return 0;
}

/*--------------------------------------
LUA	in::FindAction

IN	sName
OUT	actA

Raises error if the action is not found.
--------------------------------------*/
int in::FindAction(lua_State* l)
{
	const char* name = luaL_checkstring(l, 1);
	Action* a = FindAction(name);
	if(!a)
		luaL_error(l, "no action '%s'", name);
	a->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	in::ActionBoundCode

IN	actA
OUT	iCode
--------------------------------------*/
int in::ActionBoundCode(lua_State* l)
{
	lua_pushinteger(l, ActionBoundCode(Action::CheckLuaTo(1)));
	return 1;
}

/*--------------------------------------
LUA	in::ActionBoundString

IN	actA
OUT	sKey
--------------------------------------*/
int in::ActionBoundString(lua_State* l)
{
	lua_pushstring(l, ActionBoundString(Action::CheckLuaTo(1)));
	return 1;
}

/*
################################################################################################


	KEY


################################################################################################
*/

// FIXME: make action funcs part of class and metatable

/*--------------------------------------
	in::SetPressed
--------------------------------------*/
void in::SetPressed(unsigned int k)
{
	if(k >= NUM_KEYS) return;

	if(!keyDown[k])
		keyPressed[k] = true;

	keyRepeated[k] = true;
	keyDown[k] = true;

	if(!keyDownFrame[k])
		keyPressedFrame[k] = true;

	keyRepeatedFrame[k] = true;
	keyDownFrame[k] = true;

	UpdateKeyBuffer(keyBuffer, k);
	UpdateKeyBuffer(keyBufferFrame, k);
}

/*--------------------------------------
	in::SetReleased
--------------------------------------*/
void in::SetReleased(unsigned int k)
{
	if(k >= NUM_KEYS) return;

	keyReleased[k] = true;
	keyDown[k] = false;

	keyReleasedFrame[k] = true;
	keyDownFrame[k] = false;
}

/*--------------------------------------
	in::Pressed
--------------------------------------*/
bool in::Pressed(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyPressed[k];
}

bool in::Pressed(const Action* a)
{
	return keyPressed[a->binding];
}

/*--------------------------------------
	in::Repeated
--------------------------------------*/
bool in::Repeated(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyRepeated[k];
}

bool in::Repeated(const Action* a)
{
	return keyRepeated[a->binding];
}

/*--------------------------------------
	in::Down
--------------------------------------*/
bool in::Down(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyDown[k] || keyPressed[k];
}

bool in::Down(const Action* a)
{
	return keyDown[a->binding] || keyPressed[a->binding];
}

/*--------------------------------------
	in::Released
--------------------------------------*/
bool in::Released(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyReleased[k];
}

bool in::Released(const Action* a)
{
	return keyReleased[a->binding];
}

/*--------------------------------------
	in::PressedFrame
--------------------------------------*/
bool in::PressedFrame(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyPressedFrame[k];
}

bool in::PressedFrame(const Action* a)
{
	return keyPressedFrame[a->binding];
}

/*--------------------------------------
	in::RepeatedFrame
--------------------------------------*/
bool in::RepeatedFrame(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyRepeatedFrame[k];
}

bool in::RepeatedFrame(const Action* a)
{
	return keyRepeatedFrame[a->binding];
}

/*--------------------------------------
	in::DownFrame
--------------------------------------*/
bool in::DownFrame(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyDownFrame[k] || keyPressedFrame[k];
}

bool in::DownFrame(const Action* a)
{
	return keyDownFrame[a->binding] || keyPressedFrame[a->binding];
}

/*--------------------------------------
	in::ReleasedFrame
--------------------------------------*/
bool in::ReleasedFrame(unsigned int k)
{
	if(k >= NUM_KEYS) return false;
	return keyReleasedFrame[k];
}

bool in::ReleasedFrame(const Action* a)
{
	return keyReleasedFrame[a->binding];
}

/*--------------------------------------
	in::KeyBuffer

Typable input (and \b and \n) since last clear, null-terminated.
--------------------------------------*/
const char* in::KeyBuffer() {return keyBuffer;}

/*--------------------------------------
	in::KeyBufferFrame
--------------------------------------*/
const char* in::KeyBufferFrame() {return keyBufferFrame;}

/*--------------------------------------
	in::ApplyKeyBuffer
--------------------------------------*/
size_t in::ApplyKeyBuffer(char* dest, size_t size, const char* buf, const char* delims,
	const char* only)
{
	size_t i = strlen(dest), j = 0;

	for(; i + 1 < size && buf[j] && (!delims || strchr(delims, buf[j]) == 0); j++)
	{
		if(buf[j] == '\b')
		{
			if(i)
				i--;

			dest[i] = 0;
		}
		else if(!only || strchr(only, buf[j]))
			dest[i++] = buf[j];
	}

	dest[i] = 0;
	return j;
}

/*--------------------------------------
	in::KeyCode
--------------------------------------*/
unsigned in::KeyCode(const char* k)
{
	if(!k) return in::NULL_KEY;

	unsigned i = 0;
	for(; i < NUM_KEYS && strcmp(keyString[i], k); i++);
	if(i == NUM_KEYS) return in::NULL_KEY;

	return i;
}

/*--------------------------------------
	in::KeyString
--------------------------------------*/
const char*	in::KeyString(unsigned k)
{
	if(k >= in::NUM_KEYS || k < 0) return keyString[0];
	return keyString[k];
}

/*--------------------------------------
	in::InitKeyBuffer
--------------------------------------*/
void in::InitKeyBuffer(char*& kb)
{
	kb = new char[KEY_BUFFER_SIZE];
	ClearKeyBuffer(kb);
}

/*--------------------------------------
	in::ClearKeyBuffer
--------------------------------------*/
void in::ClearKeyBuffer(char* kb)
{
	memset(kb, 0, KEY_BUFFER_SIZE);
}

/*--------------------------------------
	in::UpdateKeyBuffer
--------------------------------------*/
void in::UpdateKeyBuffer(char* kb, unsigned k)
{
	size_t len = strlen(kb);

	if(len < KEY_BUFFER_SIZE - 1)
	{
		char c = 0;

		if(k == BACKSPACE)
			c = '\b';
		else if(k == ENTER || k == NUMPAD_ENTER)
			c = '\n';
		else if(k >= SPACE && k <= '~')
			c = Down(SHIFT) ? keyShiftConvert[k - SPACE] : k;
		else if(k >= NUMPAD_DIVIDE && k <= in::NUMPAD_9)
			c = keyNumpadConvert[k - NUMPAD_DIVIDE];
		else
			return;

		kb[len] = c;
	}
}

/*
################################################################################################


	KEY LUA

FIXME: Action check commands should be part of the action metatable
################################################################################################
*/

/*--------------------------------------
LUA	in::Pressed

IN	actA
OUT	bPressed
--------------------------------------*/
int in::Pressed(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::Pressed(a));
	return 1;
}

/*--------------------------------------
LUA	in::Repeated

IN	actA
OUT	bRepeated
--------------------------------------*/
int in::Repeated(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::Repeated(a));
	return 1;
}

/*--------------------------------------
LUA	in::Down

IN	actA
OUT	bDown
--------------------------------------*/
int in::Down(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::Down(a));
	return 1;
}

/*--------------------------------------
LUA	in::Released

IN	actA
OUT	bReleased
--------------------------------------*/
int in::Released(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::Released(a));
	return 1;
}

/*--------------------------------------
LUA	in::PressedFrame

IN	actA
OUT	bPressedFrame
--------------------------------------*/
int in::PressedFrame(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::PressedFrame(a));
	return 1;
}

/*--------------------------------------
LUA	in::RepeatedFrame

IN	actA
OUT	bRepeatedFrame
--------------------------------------*/
int in::RepeatedFrame(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::RepeatedFrame(a));
	return 1;
}

/*--------------------------------------
LUA	in::DownFrame

IN	actA
OUT	bDownFrame
--------------------------------------*/
int in::DownFrame(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::DownFrame(a));
	return 1;
}

/*--------------------------------------
LUA	in::ReleasedFrame

IN	actA
OUT	bReleasedFrame
--------------------------------------*/
int in::ReleasedFrame(lua_State* l)
{
	Action* a = Action::CheckLuaTo(1);
	lua_pushboolean(l, in::ReleasedFrame(a));
	return 1;
}

/*--------------------------------------
LUA	in::KeyBuffer

OUT	sBuffer
--------------------------------------*/
int in::KeyBuffer(lua_State* l)
{
	lua_pushstring(l, keyBuffer);
	return 1;
}

/*--------------------------------------
LUA	in::KeyBufferFrame

OUT	sBuffer
--------------------------------------*/
int in::KeyBufferFrame(lua_State* l)
{
	lua_pushstring(l, keyBufferFrame);
	return 1;
}

/*--------------------------------------
	in::ApplyKeyBufferGeneric

IN	sOrig, [sDelims], [sOnly], [iBufOff]
OUT	sNew, iStopped
--------------------------------------*/
int in::ApplyKeyBufferGeneric(lua_State* l, const char* kb)
{
	static com::Arr<char> catBuf;
	size_t kbLen = strlen(kb);

	if(!kbLen)
	{
		// Return input string and offset unmodified
		lua_pushvalue(l, 1);
		lua_pushvalue(l, 4);
		return 2;
	}

	const char* orig = luaL_checkstring(l, 1);
	const char* delims = luaL_optstring(l, 2, 0);
	const char* only = luaL_optstring(l, 3, 0);
	unsigned bufOff = luaL_optinteger(l, 4, 0);

	if(bufOff >= KEY_BUFFER_SIZE)
		bufOff = KEY_BUFFER_SIZE - 1;

	size_t origLen = strlen(orig);
	catBuf.Ensure(origLen + kbLen - bufOff + 1);
	strcpy(catBuf.o, orig);
	bufOff += ApplyKeyBuffer(catBuf.o, catBuf.n, kb + bufOff, delims, only);
	lua_pushstring(l, catBuf.o);
	lua_pushinteger(l, bufOff);
	return 2;
}

/*--------------------------------------
LUA	in::ApplyKeyBuffer

See ApplyKeyBufferGeneric.
--------------------------------------*/
int in::ApplyKeyBuffer(lua_State* l) {return ApplyKeyBufferGeneric(l, keyBuffer);}

/*--------------------------------------
LUA	in::ApplyKeyBufferFrame

See ApplyKeyBufferGeneric.
--------------------------------------*/
int in::ApplyKeyBufferFrame(lua_State* l) {return ApplyKeyBufferGeneric(l, keyBufferFrame);}

/*--------------------------------------
LUA	in::KeyCode

IN	sKey
OUT	iCode
--------------------------------------*/
int in::KeyCode(lua_State* l)
{
	const char* key = luaL_checkstring(l, 1);
	lua_pushinteger(l, KeyCode(key));
	return 1;
}

/*--------------------------------------
LUA	in::KeyString

IN	iCode
OUT	sKey
--------------------------------------*/
int	in::KeyString(lua_State* l)
{
	lua_pushstring(l, KeyString(lua_tointeger(l, 1)));
	return 1;
}

/*
################################################################################################


	POINT


################################################################################################
*/

/*--------------------------------------
	in::SetMouse

In pixels.
--------------------------------------*/
void in::SetMouse(unsigned dim, int value)
{
	int delta = value - ptValue[MOUSE_X + dim];
	ptValue[MOUSE_EVENT_DELTA_X + dim] += delta;
	ptValue[MOUSE_TICK_DELTA_X + dim] += delta;
	ptValue[MOUSE_X + dim] = value;
}

/*--------------------------------------
	in::AddMouseDelta
--------------------------------------*/
void in::AddMouseDelta(unsigned dim, int value)
{
	unsigned limit = dim ? wrp::VideoHeight() : wrp::VideoWidth();
	ptValue[MOUSE_X + dim] = COM_MIN(COM_MAX(ptValue[MOUSE_X + dim] + value, 0), limit - 1);
	ptValue[MOUSE_EVENT_DELTA_X + dim] += value;
	ptValue[MOUSE_TICK_DELTA_X + dim] += value;
}

/*--------------------------------------
	in::Point

Returns point value in pixels.
--------------------------------------*/
int in::Point(unsigned int p)
{
	if(p >= NUM_POINTS) return 0;
	return ptValue[p];
}

/*--------------------------------------
	in::PointF

Returns normalized point value.

MOUSE_X:		-1.0 = left most column, 1.0 = right most column
MOUSE_Y:		-1.0 = top row, 1.0 = bottom row
MOUSE_DELTA_X:	-1.0 = move left a distance of video width
				 1.0 = move right a distance of video width
MOUSE_DELTA_Y:	-1.0 = move up a distance of video height
				 1.0 = move down a distance of video height
--------------------------------------*/
float in::PointF(unsigned int p)
{
	switch(p)
	{
	case MOUSE_X:
		return float(ptValue[MOUSE_X])/(wrp::VideoWidth() - 1) * 2.0f - 1.0f;
	case MOUSE_Y:
		return float(ptValue[MOUSE_Y])/(wrp::VideoHeight() - 1) * 2.0f - 1.0f;
	case MOUSE_EVENT_DELTA_X:
		return float(ptValue[MOUSE_EVENT_DELTA_X])/wrp::VideoWidth();
	case MOUSE_EVENT_DELTA_Y:
		return float(ptValue[MOUSE_EVENT_DELTA_Y])/wrp::VideoHeight();
	case MOUSE_TICK_DELTA_X:
		return float(ptValue[MOUSE_TICK_DELTA_X])/wrp::VideoWidth();
	case MOUSE_TICK_DELTA_Y:
		return float(ptValue[MOUSE_TICK_DELTA_Y])/wrp::VideoHeight();
	}

	return 0.0f;
}

/*
################################################################################################


	POINT LUA


################################################################################################
*/

/*--------------------------------------
LUA	in::Mouse

OUT	xPixel, yPixel
--------------------------------------*/
int in::Mouse(lua_State* l)
{
	lua_pushinteger(l, ptValue[MOUSE_X]);
	lua_pushinteger(l, ptValue[MOUSE_Y]);
	return 2;
}

/*--------------------------------------
LUA	in::MouseF

OUT	xCoord, yCoord

Returns normalized mouse coordinates [-1.0, 1.0]
--------------------------------------*/
int in::MouseF(lua_State* l)
{
	lua_pushnumber(l, PointF(MOUSE_X));
	lua_pushnumber(l, PointF(MOUSE_Y));
	return 2;
}

/*--------------------------------------
LUA	in::MouseEventDelta

OUT	xPixel, yPixel
--------------------------------------*/
int in::MouseEventDelta(lua_State* l)
{
	lua_pushinteger(l, ptValue[MOUSE_EVENT_DELTA_X]);
	lua_pushinteger(l, ptValue[MOUSE_EVENT_DELTA_Y]);
	return 2;
}

/*--------------------------------------
LUA	in::MouseEventDeltaF

OUT	xCoord, yCoord

Returns normalized mouse coordinates [-1.0, 1.0].
--------------------------------------*/
int in::MouseEventDeltaF(lua_State* l)
{
	lua_pushnumber(l, PointF(MOUSE_EVENT_DELTA_X));
	lua_pushnumber(l, PointF(MOUSE_EVENT_DELTA_Y));
	return 2;
}

/*--------------------------------------
LUA	in::MouseTickDelta

OUT	xPixel, yPixel
--------------------------------------*/
int in::MouseTickDelta(lua_State* l)
{
	lua_pushinteger(l, ptValue[MOUSE_TICK_DELTA_X]);
	lua_pushinteger(l, ptValue[MOUSE_TICK_DELTA_Y]);
	return 2;
}

/*--------------------------------------
LUA	in::MouseTickDeltaF

OUT	xCoord, yCoord
--------------------------------------*/
int in::MouseTickDeltaF(lua_State* l)
{
	lua_pushnumber(l, PointF(MOUSE_TICK_DELTA_X));
	lua_pushnumber(l, PointF(MOUSE_TICK_DELTA_Y));
	return 2;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	in::Init
--------------------------------------*/
void in::Init()
{
	InitKeyBuffer(keyBuffer);
	InitKeyBuffer(keyBufferFrame);

	luaL_Reg regs[] =
	{
		{"EnsureAction", in::EnsureAction},
		{"BindAction", in::BindAction},
		{"FindAction", in::FindAction},
		{"ActionBoundCode", in::ActionBoundCode},
		{"ActionBoundString", in::ActionBoundString},
		{"Pressed", in::Pressed},
		{"Repeated", in::Repeated},
		{"Down", in::Down},
		{"Released", in::Released},
		{"PressedFrame", in::PressedFrame},
		{"RepeatedFrame", in::RepeatedFrame},
		{"DownFrame", in::DownFrame},
		{"ReleasedFrame", in::ReleasedFrame},
		{"KeyBuffer", in::KeyBuffer},
		{"KeyBufferFrame", in::KeyBufferFrame},
		{"ApplyKeyBuffer", in::ApplyKeyBuffer},
		{"ApplyKeyBufferFrame", in::ApplyKeyBufferFrame},
		{"KeyCode", in::KeyCode},
		{"KeyString", in::KeyString},
		{"Mouse", in::Mouse},
		{"MouseF", in::MouseF},
		{"MouseEventDelta", in::MouseEventDelta},
		{"MouseEventDeltaF", in::MouseEventDeltaF},
		{"MouseTickDelta", in::MouseTickDelta},
		{"MouseTickDeltaF", in::MouseTickDeltaF},
		{0, 0}
	};

	scr::RegisterLibrary(scr::state, "gkey", regs, 0, 0, 0, 0);
	Action::RegisterMetatable(0, 0);

	lua_pushcfunction(scr::state, BindAction); con::CreateCommand("bind");
}

/*--------------------------------------
	in::ClearTick

FIXME: all inputs should store state since last-tick, last-frame, and last-event (tick/frame)
--------------------------------------*/
void in::ClearTick()
{
	memset(keyPressed, 0, sizeof(bool) * NUM_KEYS);
	memset(keyRepeated, 0, sizeof(bool) * NUM_KEYS);
	memset(keyReleased, 0, sizeof(bool) * NUM_KEYS);

	ClearKeyBuffer(keyBuffer);

	ptValue[MOUSE_EVENT_DELTA_X] = ptValue[MOUSE_EVENT_DELTA_Y] = 0;
	ptValue[MOUSE_TICK_DELTA_X] = ptValue[MOUSE_TICK_DELTA_Y] = 0;
}

/*--------------------------------------
	in::ClearFrame
--------------------------------------*/
void in::ClearFrame()
{
	memset(keyPressedFrame, 0, sizeof(bool) * NUM_KEYS);
	memset(keyRepeatedFrame, 0, sizeof(bool) * NUM_KEYS);
	memset(keyReleasedFrame, 0, sizeof(bool) * NUM_KEYS);

	ClearKeyBuffer(keyBufferFrame);

	ptValue[MOUSE_EVENT_DELTA_X] = ptValue[MOUSE_EVENT_DELTA_Y] = 0;
}