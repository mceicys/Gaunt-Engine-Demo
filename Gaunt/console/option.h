// option.h
// Martynas Ceicys

#ifndef CON_OPTION_H
#define CON_OPTION_H

extern "C"
{
	#include "../lua/lua.h"
}

namespace con
{

class Option;

typedef void (*OptionFunc)(Option& optIO, float set);

/*======================================
	con::Option
======================================*/
class Option
{
public:
				Option(const char* name, float def, OptionFunc set = 0);
	const char*	Name() const {return name;}
	float		Float() const {return num;}
	int			Integer() const {return num;} // FIXME: store cast?
	unsigned	Unsigned() const {return num;}
	bool		Bool() const {return bln;}
	void		SetValue(float f, bool log = true);
	void		ForceValue(float f); // Only call in OptionFuncs or for read-onlys
	void		Register(const char* cmdName = 0);
	void		LuaPushGetter(lua_State* l) const;
	void		LuaPushSetter(lua_State* l);
	static void	RegisterAll();
	static Option* const*	Options() {return options;}
	static size_t			NumOptions() {return numOptions;}

private:
	const char*	name;
	OptionFunc	func;
	float		num;
	bool		bln;

				Option();
	static int	Command(lua_State* l);
	static int	Getter(lua_State* l);
	static int	Setter(lua_State* l);

	static Option* options[];
	static size_t numOptions;
	static const size_t MAX_NUM_OPTIONS = 512;
	static bool listInitialized;
};

void ReadOnly(Option& opt, float set);
void PositiveOnly(Option& opt, float set);
void PositiveIntegerOnly(Option& opt, float set);

}

#endif