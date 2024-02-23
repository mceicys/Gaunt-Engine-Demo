// path_flight_navigator.cpp
// Martynas Ceicys

#include "path.h"
#include "path_lua.h"
#include "path_private.h"

const char* const pat::FlightNavigator::METATABLE = "metaFlightNavigator";

#define VALIDATE_MAP(errVal) \
	if(!map) \
	{ \
		CON_ERROR("FlightNavigator has no map"); \
		return errVal; \
	}

/*--------------------------------------
	pat::FlightNavigator::FlightNavigator
--------------------------------------*/
pat::FlightNavigator::FlightNavigator()
	: map(0), flags(0)
{
	ClearAttempt();
}

pat::FlightNavigator::~FlightNavigator()
{
}

/*--------------------------------------
	pat::FlightNavigator::SetMap
--------------------------------------*/
void pat::FlightNavigator::SetMap(FlightMap* f)
{
	if(map == f)
		return;

	ClearAttempt();
	map = f;
}

/*--------------------------------------
	pat::FlightNavigator::ClearAttempt
--------------------------------------*/
void pat::FlightNavigator::ClearAttempt()
{
	ClearDevelopment();
	savedPos = savedDest = 0.0f;
	flags &= ~HOLD_FAIL;
}

/*--------------------------------------
	pat::FlightNavigator::FindPath

Starts or continues a path search. When a search is continued, given pos and dest may not have
an effect. If GO is returned, pts and numPts are set. If FAIL is returned, no path was found. In
either case, calling FindPath again starts a new search. If WAIT is returned, call FindPath or
ContinueSearch every tick until FindPath returns something conclusive, or call ClearAttempt to
stop the search. If the search is not continued for a tick, it's automatically dropped.

FIXME: Make pos and dest optional parameters so FindPath can be used like ContinueSearch except
	it won't hold up the work-memory
--------------------------------------*/
pat::state pat::FlightNavigator::FindPath(const com::Vec3& pos, const com::Vec3& dest,
	com::Arr<waypoint>& pts, size_t& numPts)
{
	VALIDATE_MAP(FAIL);

	const FlightWorkMemory* mem = ticket.Memory();

	if(mem && mem->finale != -1)
	{
		// Got a path during ContinueSearch
		numPts = mem->TracePath(pts);
		ClearAttempt();
		return GO;
	}

	if(flags & HOLD_FAIL)
	{
		// Failed during ContinueSearch, report this to caller
		ClearAttempt();
		return FAIL;
	}

	savedPos = pos;
	savedDest = dest;

	state s = DevelopPath();

	if(s == GO)
	{
		mem = ticket.Memory();
		numPts = mem->TracePath(pts);
		ClearAttempt();
	}
	else if(s == FAIL)
		ClearAttempt();

	return s;
}

/*--------------------------------------
	pat::FlightNavigator::ContinueSearch

Like FindPath but this function uses the saved parameters from the last FindPath call and saves
results for the next FindPath call instead of returning them immediately. If FindPath has not
been called since the last ClearAttempt, this does nothing.
--------------------------------------*/
void pat::FlightNavigator::ContinueSearch()
{
	if(
		!ticket.Queued() || // Not working on a path
		(flags & HOLD_FAIL) || // Failure hasn't been cleared
		!map // Invalid inputs
	)
	{
		return;
	}

	state ret = DevelopPath();

	if(ret == FAIL)
		flags |= HOLD_FAIL;
}

/*--------------------------------------
	pat::FlightNavigator::DrawSearch
--------------------------------------*/
void pat::FlightNavigator::DrawSearch(int color, float time) const
{
	if(const FlightWorkMemory* mem = ticket.Memory())
		pat::DrawSearch(mem->pts.o, mem->numPts, color, time);
}

/*--------------------------------------
	pat::FlightNavigator::LuaCheckMap
--------------------------------------*/
pat::FlightMap* pat::FlightNavigator::LuaCheckMap(lua_State* l) const
{
	if(!map)
		luaL_error(l, "FlightNavigator has no Map");

	return map;
}

/*--------------------------------------
	pat::FlightNavigator::DevelopPath

Starts a search for a path from savedPos to savedDest, or continues a previous search. Returns
FAIL, WAIT, or GO.
--------------------------------------*/
pat::state pat::FlightNavigator::DevelopPath()
{
	ticket.Punch();

	if(ticket.Memory())
	{
		FlightWorkMemory& mem = *ticket.Memory();

		if(mem.pathType == PATH_NONE)
		{
			// Work-memory is clear, starting a new path
			mem.SetParams(*map, PATH_POINT, savedPos, savedDest);
		}

		return Path(mem);
	}
	else
		return WAIT;
}

/*--------------------------------------
	pat::FlightNavigator::ClearDevelopment
--------------------------------------*/
void pat::FlightNavigator::ClearDevelopment()
{
	ticket.Drop();
}

/*
################################################################################################


	FLIGHT NAVIGATOR LUA


################################################################################################
*/

/*--------------------------------------
LUA	pat::CreateFlightNavigator (FlightNavigator)

OUT	fnav
--------------------------------------*/
int pat::CreateFlightNavigator(lua_State* l)
{
	FlightNavigator* n = new FlightNavigator();
	n->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	pat::FNavMap (Map)

IN	fnavN
OUT	[fmapM]
--------------------------------------*/
int pat::FNavMap(lua_State* l)
{
	FlightMap* m = FlightNavigator::CheckLuaTo(1)->Map();

	if(m)
	{
		m->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	pat::FNavSetMap (SetMap)

IN	fnavN, [fmapM]
--------------------------------------*/
int pat::FNavSetMap(lua_State* l)
{
	FlightNavigator::CheckLuaTo(1)->SetMap(FlightMap::OptionalLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	pat::FNavDeveloping (Developing)

IN	fnavN
OUT	bDeveloping
--------------------------------------*/
int pat::FNavDeveloping(lua_State* l)
{
	lua_pushboolean(l, FlightNavigator::CheckLuaTo(1)->Developing());
	return 1;
}

/*--------------------------------------
LUA	pat::FNavSearching (Searching)

IN	fnavN
OUT	bSearching
--------------------------------------*/
int pat::FNavSearching(lua_State* l)
{
	lua_pushboolean(l, FlightNavigator::CheckLuaTo(1)->Searching());
	return 1;
}

/*--------------------------------------
LUA	pat::FNavClearAttempt (ClearAttempt)

IN	fnavN
--------------------------------------*/
int pat::FNavClearAttempt(lua_State* l)
{
	FlightNavigator::CheckLuaTo(1)->ClearAttempt();
	return 0;
}

/*--------------------------------------
LUA	pat::FNavFindPath (FindPath)

IN	fnavN, tPathOut, v3Pos, v3Dest, [iOffset = 0], [iExtraStride = 0]
OUT	iState, [iNumPts]

If iState is GO, tPathOut is filled with a sequence of iNumPts 3D waypoint vectors, starting
from iOffset + 1. iExtraStride number of elements are set to false after each vector.
--------------------------------------*/
int pat::FNavFindPath(lua_State* l)
{
	static com::Arr<waypoint> tempPts(32);
	static size_t numTempPts;

	const int TABLE_INDEX = 2;
	scr::CheckTable(l, TABLE_INDEX);

	state s = FlightNavigator::CheckLuaTo(1)->FindPath(vec::CheckLuaToVec(l, 3, 4, 5),
		vec::CheckLuaToVec(l, 6, 7, 8), tempPts, numTempPts);

	lua_Integer offset = luaL_optinteger(l, 9, 0);
	lua_Integer extraStride = luaL_optinteger(l, 10, 0);

	if(s == GO)
	{
		lua_Integer index = offset + 1;

		for(size_t i = 0; i < numTempPts; i++)
		{
			for(size_t j = 0; j < 3; j++)
			{
				lua_pushnumber(l, tempPts[i].pos[j]);
				lua_rawseti(l, TABLE_INDEX, index);
				index++;
			}

			for(lua_Integer j = 0; j < extraStride; j++)
			{
				lua_pushboolean(l, 0);
				lua_rawseti(l, TABLE_INDEX, index);
				index++;
			}
		}

		lua_pushinteger(l, s);
		lua_pushinteger(l, numTempPts);
		return 2;
	}
	else
	{
		lua_pushinteger(l, s);
		return 1;
	}
}

/*--------------------------------------
LUA	pat::FNavContinueSearch (ContinueSearch)

IN	fnavN
--------------------------------------*/
int pat::FNavContinueSearch(lua_State* l)
{
	FlightNavigator::CheckLuaTo(1)->ContinueSearch();
	return 0;
}

/*--------------------------------------
LUA	pat::FNavDrawSearch (DrawSearch)

IN	fnavN, iColor, [nTime = 0]
--------------------------------------*/
int pat::FNavDrawSearch(lua_State* l)
{
	FlightNavigator::CheckLuaTo(1)->DrawSearch(luaL_checkinteger(l, 2),
		luaL_optnumber(l, 3, 0.0));

	return 0;
}