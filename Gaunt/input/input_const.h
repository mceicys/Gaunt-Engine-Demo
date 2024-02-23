// input_const.h -- key names, shift conversion
// Martynas Ceicys

#ifndef INPUT_NAMES_H
#define INPUT_NAMES_H

#include "input_code.h"

static const char* const ns = "NULL";

static const char* const keyString[in::NUM_KEYS] =
{
	ns, ns, ns, ns, ns, ns, ns, ns, //0 - 7
	"BACKSPACE", //8
	"TAB", //9
	"ENTER", //10
	ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, ns, //11 - 26
	"ESCAPE", //27
	ns, ns, ns, ns, //28 - 31
	"SPACE", //32
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	"'", //39
	ns,
	ns,
	ns,
	ns,
	",", //44
	"-",
	".",
	"/",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9", //57
	ns,
	";", //59
	ns,
	"=", //61
	ns,
	ns,
	ns,
	ns, //65 'A'
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	ns,
	"[", //91
	"\\",
	"]",
	ns,
	ns,
	"`", //96
	"a", //97
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p",
	"q",
	"r",
	"s",
	"t",
	"u",
	"v",
	"w",
	"x",
	"y",
	"z",
	ns, //123
	ns,
	ns,
	ns,
	ns, //127
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12", //139
	"PRINT_SCREEN",
	"SCROLL_LOCK",
	"PAUSE", //142
	"INSERT",
	"DELETE",
	"HOME",
	"END",
	"PAGE_UP",
	"PAGE_DOWN", //148
	"NUMPAD_LOCK",
	"NUMPAD_ENTER",
	"NUMPAD_/",
	"NUMPAD_*",
	"NUMPAD_-",
	"NUMPAD_+",
	"NUMPAD_.",
	"NUMPAD_0",
	"NUMPAD_1",
	"NUMPAD_2",
	"NUMPAD_3",
	"NUMPAD_4",
	"NUMPAD_5",
	"NUMPAD_6",
	"NUMPAD_7",
	"NUMPAD_8",
	"NUMPAD_9", //165
	"CAPS_LOCK",
	"SHIFT",
	"CTRL",
	"ALT", //169
	"UP_ARROW",
	"RIGHT_ARROW",
	"DOWN_ARROW",
	"LEFT_ARROW", //173
	"MOUSE_BUTTON_0",
	"MOUSE_BUTTON_1",
	"MOUSE_BUTTON_2",
	"MOUSE_WHEEL_UP",
	"MOUSE_WHEEL_DOWN", //178
	"EXTRA_0",
	"EXTRA_1",
	"EXTRA_2",
	"EXTRA_3",
	"EXTRA_4",
	"EXTRA_5",
	"EXTRA_6",
	"EXTRA_7",
	"EXTRA_8",
	"EXTRA_9",
	"EXTRA_10",
	"EXTRA_11",
	"EXTRA_12",
	"EXTRA_13",
	"EXTRA_14",
	"EXTRA_15",
	"EXTRA_16",
	"EXTRA_17",
	"EXTRA_18",
	"EXTRA_19",
	"EXTRA_20",
	"EXTRA_21",
	"EXTRA_22",
	"EXTRA_23",
	"EXTRA_24",
	"EXTRA_25",
	"EXTRA_26",
	"EXTRA_27",
	"EXTRA_28",
	"EXTRA_29",
	"EXTRA_30",
	"EXTRA_31",
	"EXTRA_32",
	"EXTRA_33",
	"EXTRA_34",
	"EXTRA_35",
	"EXTRA_36",
	"EXTRA_37",
	"EXTRA_38",
	"EXTRA_39",
	"EXTRA_40",
	"EXTRA_41",
	"EXTRA_42",
	"EXTRA_43",
	"EXTRA_44",
	"EXTRA_45",
	"EXTRA_46",
	"EXTRA_47",
	"EXTRA_48",
	"EXTRA_49",
	"EXTRA_50",
	"EXTRA_51",
	"EXTRA_52",
	"EXTRA_53",
	"EXTRA_54",
	"EXTRA_55",
	"EXTRA_56",
	"EXTRA_57",
	"EXTRA_58",
	"EXTRA_59",
	"EXTRA_60",
	"EXTRA_61",
	"EXTRA_62",
	"EXTRA_63" //242
};

// keyShiftConvert[# - SPACE]
static const char keyShiftConvert[] =
{
	" !\"#$%&\"()*+<_>?)!@#$%^&*(::<+>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}^_~"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~"
};

// keyNumpadConvert[# - NUMPAD_DIVIDE]
static const char keyNumpadConvert[] =
{
	"/*-+.0123456789"
};

#endif