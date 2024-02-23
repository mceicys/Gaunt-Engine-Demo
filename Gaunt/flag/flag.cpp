// flag.cpp
// Martynas Ceicys

#include <stdlib.h>
#include <string.h>

#include "flag.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/math.h"
#include "../wrap/wrap.h"

#define BITS_PER_UNIT (sizeof(unit) * CHAR_BIT)

namespace flg
{
	void	FlagToBit(size_t f, size_t& indexOut, size_t& bitOut);

	// FLAGSET LUA
	int		CreateFlagSet(lua_State* l);

	// metaFlagSet
	int		FlagSetOr(lua_State* l);
	int		FlagSetOff(lua_State* l);
	int		FlagSetAnd(lua_State* l);
	int		FlagSetXor(lua_State* l);
	int		FlagSetClear(lua_State* l);
	int		FlagSetCopy(lua_State* l);
	int		FlagSetEqual(lua_State* l);
	int		FlagSetTrue(lua_State* l);
	int		FlagSetFlags(lua_State* l);
};

/*--------------------------------------
	flg::FlagToBit
--------------------------------------*/
void flg::FlagToBit(size_t f, size_t& indexOut, size_t& bitOut)
{
	indexOut = f / BITS_PER_UNIT;
	bitOut = f % BITS_PER_UNIT;
}

/*
################################################################################################


	FLAGSET


################################################################################################
*/

const char* const flg::FlagSet::METATABLE = "metaFlagSet";

/*--------------------------------------
	flg::FlagSet::FlagSet

FIXME: Don't allocate initially because many FlagSets might not be used
--------------------------------------*/
flg::FlagSet::FlagSet()
{
	numUnits = 1;
	units = (unit*)malloc(sizeof(unit) * numUnits);
	memset(units, 0, sizeof(unit) * numUnits);
}

flg::FlagSet::FlagSet(const FlagSet& s)
{
	numUnits = s.numUnits;
	units = (unit*)malloc(sizeof(unit) * numUnits);
	memset(units, 0, sizeof(unit) * numUnits);

	for(size_t i = 0; i < s.numUnits; i++)
		units[i] = s.units[i];
}

/*--------------------------------------
	flg::FlagSet::~FlagSet
--------------------------------------*/
flg::FlagSet::~FlagSet()
{
	free(units);
}

/*--------------------------------------
	flg::FlagSet::operator=
--------------------------------------*/
flg::FlagSet& flg::FlagSet::operator=(const FlagSet& s)
{
	if(EnsureNumUnits(s.numUnits))
		memcpy(units, s.units, sizeof(unit) * numUnits);
	else
	{
		memcpy(units, s.units, sizeof(unit) * s.numUnits);
		memset(units + s.numUnits, 0, sizeof(unit) * (numUnits - s.numUnits));
	}

	return *this;
}

/*--------------------------------------
	flg::FlagSet::operator|=
--------------------------------------*/
flg::FlagSet& flg::FlagSet::operator|=(size_t f)
{
	size_t index, bit;
	FlagToBit(f, index, bit);

	EnsureNumUnits(index + 1);

	units[index] |= 1U << bit;

	return *this;
}

flg::FlagSet& flg::FlagSet::operator|=(const FlagSet& s)
{
	EnsureNumUnits(s.numUnits);

	for(size_t i = 0; i < COM_MIN(numUnits, s.numUnits); i++)
		units[i] |= s.units[i];

	return *this;
}

/*--------------------------------------
	flg::FlagSet::Off
--------------------------------------*/
flg::FlagSet& flg::FlagSet::Off(size_t f)
{
	size_t index, bit;
	FlagToBit(f, index, bit);

	if(index >= numUnits)
		return *this;

	units[index] &= ~(1U << bit);

	return *this;
}

flg::FlagSet& flg::FlagSet::Off(const FlagSet& s)
{
	for(size_t i = 0; i < COM_MIN(numUnits, s.numUnits); i++)
		units[i] &= ~s.units[i];

	return *this;
}

/*--------------------------------------
	flg::FlagSet::operator&=
--------------------------------------*/
flg::FlagSet& flg::FlagSet::operator&=(const FlagSet& s)
{
	EnsureNumUnits(s.numUnits);

	for(size_t i = 0; i < COM_MIN(numUnits, s.numUnits); i++)
		units[i] &= s.units[i];

	return *this;
}

/*--------------------------------------
	flg::FlagSet::operator^=
--------------------------------------*/
flg::FlagSet& flg::FlagSet::operator^=(size_t f)
{
	size_t index, bit;
	FlagToBit(f, index, bit);

	EnsureNumUnits(index + 1);

	units[index] ^= 1U << bit;

	return *this;
}

flg::FlagSet& flg::FlagSet::operator^=(const FlagSet& s)
{
	EnsureNumUnits(s.numUnits);

	for(size_t i = 0; i < COM_MIN(numUnits, s.numUnits); i++)
		units[i] ^= s.units[i];

	return *this;
}

/*--------------------------------------
	flg::FlagSet::Clear
--------------------------------------*/
flg::FlagSet& flg::FlagSet::Clear()
{
	memset(units, 0, sizeof(unit) * numUnits);
	return *this;
}

/*--------------------------------------
	flg::FlagSet::operator==
--------------------------------------*/
bool flg::FlagSet::operator==(const FlagSet& s)
{
	if(numUnits >= s.numUnits)
	{
		for(size_t i = 0; i < s.numUnits; i++)
		{
			if(units[i] != s.units[i])
				return false;
		}

		for(size_t i = s.numUnits; i < numUnits; i++)
		{
			if(units[i])
				return false;
		}
	}
	else
	{
		for(size_t i = 0; i < numUnits; i++)
		{
			if(units[i] != s.units[i])
				return false;
		}

		for(size_t i = numUnits; i < s.numUnits; i++)
		{
			if(s.units[i])
				return false;
		}
	}

	return true;
}

/*--------------------------------------
	flg::FlagSet::True
--------------------------------------*/
bool flg::FlagSet::True() const
{
	unit u = 0U;

	for(size_t i = 0; i < numUnits; i++)
		u |= units[i];

	return (bool)u;
}

bool flg::FlagSet::True(size_t and) const
{
	size_t index, bit;
	FlagToBit(and, index, bit);

	return index < numUnits && units[index] & 1U << bit;
}

bool flg::FlagSet::True(const FlagSet& and) const
{
	unit u = 0U;

	for(size_t i = 0; i < COM_MIN(numUnits, and.numUnits); i++)
		u |= units[i] & and.units[i];

	return (bool)u;
}

/*--------------------------------------
	flg::FlagSet::EnsureNumUnits

If num > numUnits, reallocates units and returns true.
--------------------------------------*/
bool flg::FlagSet::EnsureNumUnits(size_t num)
{
	if(num > numUnits)
	{
		units = (unit*)realloc(units, sizeof(unit) * num);
		memset(units + numUnits, 0, sizeof(unit) * (num - numUnits));

		numUnits = num;
		return true;
	}

	return false;
}

/*--------------------------------------
	flg::FlagSet::EnsureNumBits
--------------------------------------*/
bool flg::FlagSet::EnsureNumBits(size_t num)
{
	return EnsureNumUnits(num / BITS_PER_UNIT + ((num % BITS_PER_UNIT) ? 1 : 0));
}

/*--------------------------------------
	flg::FlagSet::StrToFlagSet

Parses flag IDs.
--------------------------------------*/
void flg::FlagSet::StrToFlagSet(const char* str)
{
	const char* start = str;
	char* end;

	while(1)
	{
		size_t flag = strtoul(start, &end, 10);

		if(start == end)
			break;

		operator|=(flag);
		start = end;
	}
}

// Hackish way of executing code for every active flag
#define FOR_EACH_FLAG(flag) \
{ \
	for(size_t i = 0; i < numUnits; i++) \
	{ \
		unit u = units[i]; \
		\
		while(u) \
		{ \
			unit bit = u; \
			u &= u - 1; \
			bit = bit & ~u; \
			\
			flag = 0; \
			for(; bit >>= 1U; flag++); \
			flag = i * BITS_PER_UNIT + flag; \
			\
			/* code goes here */
#define FOR_EACH_FLAG_END \
		} \
	} \
}

/*--------------------------------------
	flg::FlagSet::ToStr

Returns new null-terminated string with set's flag IDs separated by spaces. Returns 0 if no
flags are active.

FIXME: save flag sets as 32-bit integer arrays
--------------------------------------*/
char* flg::FlagSet::ToStr() const
{
	size_t len = 0, flag;

	FOR_EACH_FLAG(flag)
		len += com::IntLength(flag);
		len++;
	FOR_EACH_FLAG_END

	if(!len)
		return 0;

	char* str = new char[len];
	size_t start = 0;

	FOR_EACH_FLAG(flag)
		if(start)
			start += com::SNPrintF(str + start, len - start, 0, " ");

		start += com::SNPrintF(str + start, len - start, 0, COM_UMAX_PRNT, (uintmax_t)flag);
	FOR_EACH_FLAG_END

	return str;
}

/*--------------------------------------
	flg::FlagSet::LuaPushFlags

Pushes active flags onto the stack. If lua_checkstack fails, a fatal error occurs. Returns the
number of pushed integers.
--------------------------------------*/
int flg::FlagSet::LuaPushFlags(lua_State* l) const
{
	if(!lua_checkstack(l, numUnits * BITS_PER_UNIT))
		WRP_FATAL("Cannot push flags, not enough Lua stack space");

	int numPushed = 0;
	size_t flag;

	FOR_EACH_FLAG(flag)
		lua_pushinteger(l, flag);
		numPushed++;
	FOR_EACH_FLAG_END

	return numPushed;
}

/*--------------------------------------
	flg::EmptySet
--------------------------------------*/
static flg::FlagSet empty;

const flg::FlagSet& flg::EmptySet()
{
	return empty;
}

/*
################################################################################################


	FLAGSET LUA


################################################################################################
*/

/*--------------------------------------
LUA	flg::CreateFlagSet (FlagSet)

OUT	fset
--------------------------------------*/
int flg::CreateFlagSet(lua_State* l)
{
	FlagSet* f = new FlagSet;
	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetOr (Or)

IN	fsetF, fsetG | iFlag...
OUT	fsetF
--------------------------------------*/
int flg::FlagSetOr(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		*f |= *g;
	else
	{
		for(int i = lua_gettop(l); i > 1; i--)
			*f |= lua_tointeger(l, i);
	}

	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetOff (Off)

IN	fsetF, fsetG | iFlag...
OUT	fsetF
--------------------------------------*/
int flg::FlagSetOff(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		f->Off(*g);
	else
	{
		for(int i = lua_gettop(l); i > 1; i--)
			f->Off(lua_tointeger(l, i));
	}

	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetAnd (And)

IN	fsetF, fsetG | iFlag...
OUT	fsetF
--------------------------------------*/
int flg::FlagSetAnd(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		*f &= *g;
	else
	{
		static FlagSet temp;
		temp.Clear();

		for(int i = lua_gettop(l); i > 1; i--)
			temp |= lua_tointeger(l, i);

		*f &= temp;
	}

	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetXor (Xor)

IN	fsetF, fsetG | iFlag...
OUT	fsetF
--------------------------------------*/
int flg::FlagSetXor(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		*f ^= *g;
	else
	{
		for(int i = lua_gettop(l); i > 1; i--)
			*f ^= lua_tointeger(l, i);
	}

	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetClear (Clear)

IN	fsetF
OUT	fsetF
--------------------------------------*/
int flg::FlagSetClear(lua_State* l)
{
	FlagSet::CheckLuaTo(1)->Clear().LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetCopy (Copy)

IN	fsetF, fsetG | iFlag...
OUT	fsetF
--------------------------------------*/
int flg::FlagSetCopy(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		*f = *g;
	else
	{
		f->Clear();

		for(int i = lua_gettop(l); i > 1; i--)
			*f |= lua_tointeger(l, i);
	}

	f->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetEqual (Equal)

IN	fsetF, fsetG | iFlag...
OUT	bEqual
--------------------------------------*/
int flg::FlagSetEqual(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		lua_pushboolean(l, *f == *g);
	else
	{
		static FlagSet temp;
		temp.Clear();

		for(int i = lua_gettop(l); i > 1; i--)
			temp |= lua_tointeger(l, i);

		lua_pushboolean(l, *f == temp);
	}

	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetTrue (True)

IN	fsetF, [fsetG | iFlag...]
OUT	bTrue
--------------------------------------*/
int flg::FlagSetTrue(lua_State* l)
{
	const FlagSet* f = FlagSet::CheckLuaTo(1);
	const FlagSet* g = FlagSet::LuaTo(2);

	if(g)
		lua_pushboolean(l, f->True(*g));
	else if(lua_gettop(l) == 1)
		lua_pushboolean(l, f->True());
	else
	{
		static FlagSet temp;
		temp.Clear();

		for(int i = lua_gettop(l); i > 1; i--)
			temp |= lua_tointeger(l, i);

		lua_pushboolean(l, f->True(temp));
	}

	return 1;
}

/*--------------------------------------
LUA	flg::FlagSetFlags (Flags)

IN	fsetF
OUT	iFlag...
--------------------------------------*/
int flg::FlagSetFlags(lua_State* l)
{
	FlagSet* f = FlagSet::CheckLuaTo(1);
	return f->LuaPushFlags(l);
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	flg::Init
--------------------------------------*/
void flg::Init()
{
	luaL_Reg regs[] = {
		{"FlagSet", CreateFlagSet},
		{0, 0}
	};

	luaL_Reg setRegs[] = {
		{"Or", FlagSetOr},
		{"Off", FlagSetOff},
		{"And", FlagSetAnd},
		{"Xor", FlagSetXor},
		{"Clear", FlagSetClear},
		{"Copy", FlagSetCopy},
		{"Equal", FlagSetEqual},
		{"True", FlagSetTrue},
		{"Flags", FlagSetFlags},
		{"__gc", FlagSet::CLuaDelete},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		setRegs,
		0
	};

	const char* prefixes[] = {
		"FlagSet", // FIXME: long
		0
	};

	scr::RegisterLibrary(scr::state, "gflg", regs, 0, 0, metas, prefixes);
	FlagSet::RegisterMetatable(setRegs, 0);
}