// input.h
// Martynas Ceicys

/*	FIXME: Get rid of explicit checks for key presses. All input checks should use rebindable
	actions. Create analog actions (eg. bound to MOUSE_X and stuff). This will allow for
	joystick support later on (the OS wrapper would send joystick data). Analog to digital
	conversions and vice versa can also be added later for neat threshold bindings.
*/

#ifndef INPUT_H
#define INPUT_H

#include "input_code.h"

namespace in
{

/*
################################################################################################
	ACTION
################################################################################################
*/

class Action;

Action*		EnsureAction(const char* name);
void		BindAction(Action* a, unsigned int keyCode);
Action*		FindAction(const char* name);
unsigned	ActionBoundCode(const Action* a);
const char*	ActionBoundString(const Action* a);

/*
################################################################################################
	KEY
################################################################################################
*/

void		SetPressed(unsigned keyCode);
void		SetReleased(unsigned keyCode);

bool		Pressed(unsigned keyCode);
bool		Repeated(unsigned keyCode);
bool		Down(unsigned keyCode);
bool		Released(unsigned keyCode);

bool		Pressed(const Action* a);
bool		Repeated(const Action* a);
bool		Down(const Action* a);
bool		Released(const Action* a);

bool		PressedFrame(unsigned keyCode);
bool		RepeatedFrame(unsigned keyCode);
bool		DownFrame(unsigned keyCode);
bool		ReleasedFrame(unsigned keyCode);

bool		PressedFrame(const Action* a);
bool		RepeatedFrame(const Action* a);
bool		DownFrame(const Action* a);
bool		ReleasedFrame(const Action* a);

const char*	KeyBuffer();
const char*	KeyBufferFrame();
size_t		ApplyKeyBuffer(char* destIO, size_t size, const char* buf, const char* delims = 0,
			const char* only = 0);

unsigned	KeyCode(const char* keyString);
const char*	KeyString(unsigned keyCode);

/*
################################################################################################
	POINT
################################################################################################
*/

void		SetMouse(unsigned dim, int value);
void		AddMouseDelta(unsigned dim, int value);
int			Point(unsigned int pointCode);
float		PointF(unsigned int pointCode);

/*
################################################################################################
	GENERAL
################################################################################################
*/

void		Init();
void		ClearTick();
void		ClearFrame();

}

#endif