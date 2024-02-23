// console_option.cpp
// Martynas Ceicys

#include "console.h"
#include "option.h"

#include "../script/script.h"
#include "../wrap/wrap.h"

con::Option* con::Option::options[con::Option::MAX_NUM_OPTIONS];
size_t con::Option::numOptions = 0;
bool con::Option::listInitialized = false;

/*--------------------------------------
	con::Option::Option
--------------------------------------*/
con::Option::Option(const char* name, float def, OptionFunc set)
	: name(name), num(def), bln(def), func(set)
{
	if(numOptions == MAX_NUM_OPTIONS)
	{
		WRP_FATALF("Exceeded maximum number of options, %u",
			(unsigned)con::Option::MAX_NUM_OPTIONS);
	}

	if(listInitialized)
		Register();

	options[numOptions++] = this;
}

/*--------------------------------------
	con::Option::SetValue
--------------------------------------*/
void con::Option::SetValue(float f, bool log)
{
	if(func)
		func(*this, f);
	else
	{
		num = f;
		bln = f;
	}

	if(log)
		con::LogF("%s: %g", name, num);
}

/*--------------------------------------
	con::Option::ForceValue
--------------------------------------*/
void con::Option::ForceValue(float f)
{
	num = f;
	bln = f;
}

/*--------------------------------------
	con::Option::Register

Creates a command for the option.
--------------------------------------*/
void con::Option::Register(const char* cmdName)
{
	lua_pushlightuserdata(scr::state, this);
	lua_pushcclosure(scr::state, Command, 1);
	CreateCommand(cmdName ? cmdName : name);
}

/*--------------------------------------
	con::Option::LuaPushGetter
--------------------------------------*/
void con::Option::LuaPushGetter(lua_State* l) const
{
	lua_pushlightuserdata(l, (void*)this);
	lua_pushcclosure(l, Getter, 1);
}

/*--------------------------------------
	con::Option::LuaPushSetter
--------------------------------------*/
void con::Option::LuaPushSetter(lua_State* l)
{
	lua_pushlightuserdata(l, this);
	lua_pushcclosure(l, Setter, 1);
}

/*--------------------------------------
LUA	con::Option::Command

IN	[nSet]
--------------------------------------*/
int con::Option::Command(lua_State* l)
{
	Option& opt = *(Option*)lua_touserdata(l, lua_upvalueindex(1));

	if(!lua_gettop(l))
		con::LogF("%s: %g", opt.Name(), opt.Float());
	else
		opt.SetValue(luaL_checknumber(l, 1));

	return 0;
}

/*--------------------------------------
LUA	con::Option::Getter

OUT	nVal
--------------------------------------*/
int con::Option::Getter(lua_State* l)
{
	Option& opt = *(Option*)lua_touserdata(l, lua_upvalueindex(1));
	lua_pushnumber(l, opt.Float());
	return 1;
}

/*--------------------------------------
LUA	con::Option::Setter

IN	nSet
--------------------------------------*/
int con::Option::Setter(lua_State* l)
{
	Option& opt = *(Option*)lua_touserdata(l, lua_upvalueindex(1));
	opt.SetValue(luaL_checknumber(l, 1));
	return 0;
}

/*--------------------------------------
	con::Option::RegisterAll

Call once the Lua state is up.
--------------------------------------*/
void con::Option::RegisterAll()
{
	for(size_t i = 0; i < numOptions; i++)
		options[i]->Register();
}

/*--------------------------------------
	con::ReadOnly
--------------------------------------*/
void con::ReadOnly(Option& opt, float set)
{
	con::LogF("%s is read-only", opt.Name());
}

/*--------------------------------------
	con::PositiveOnly
--------------------------------------*/
void con::PositiveOnly(con::Option& opt, float set)
{
	if(set <= 0.0f)
	{
		con::AlertF("%s cannot be " COM_FLT_PRNT ", must be > 0", opt.Name(), set);
		return;
	}

	opt.ForceValue(set);
}

/*--------------------------------------
	con::PositiveIntegerOnly
--------------------------------------*/
void con::PositiveIntegerOnly(con::Option& opt, float set)
{
	int i = set;

	if(i <= 0)
	{
		con::AlertF("%s cannot be %d, must be integer > 0", opt.Name(), i);
		return;
	}

	opt.ForceValue(i);
}