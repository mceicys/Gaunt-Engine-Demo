// flag.h
// Martynas Ceicys

#ifndef FLAG_H
#define FLAG_H

#include <stdint.h>

#include "../resource/resource.h"
#include "../script/script.h"

namespace flg
{

typedef uint32_t unit;
class FlagSet;

const FlagSet& EmptySet();

/*======================================
	flg::FlagSet

FIXME: Making this a Resource significantly increases memory expense considering the point is to
	store flags efficiently, do some sort of LightResource without locks, lists, etc?
FIXME: Scrap this code for now and just make game flags 32-bit uints, that's good enough
======================================*/
class FlagSet : public res::Resource<FlagSet>
{
public:
				FlagSet();
				FlagSet(const FlagSet& s);
				~FlagSet();

	FlagSet&	operator=(const FlagSet& s);
	FlagSet&	operator|=(size_t f);
	FlagSet&	operator|=(const FlagSet& s);
	FlagSet&	Off(size_t f);
	FlagSet&	Off(const FlagSet& s);
	FlagSet&	operator&=(const FlagSet& s);
	FlagSet&	operator^=(size_t f);
	FlagSet&	operator^=(const FlagSet& s);
	FlagSet&	Clear();

	bool		operator==(const FlagSet& s);
	bool		True() const;
	bool		True(size_t and) const;
	bool		True(const FlagSet& and) const;

	bool		EnsureNumUnits(size_t num);
	bool		EnsureNumBits(size_t num);

	void		StrToFlagSet(const char* str);
	char*		ToStr() const;
	int			LuaPushFlags(lua_State* l) const;

	// Like OptionalLuaTo but returns empty set instead of 0
	static const FlagSet* DefaultLuaTo(int index)
	{
		return lua_type(scr::state, index) <= LUA_TNIL ? &EmptySet() : CheckLuaTo(index);
	}

private:
	unit*		units;
	size_t		numUnits;
};

/*
################################################################################################
	GENERAL
################################################################################################
*/

void Init();

};

#endif