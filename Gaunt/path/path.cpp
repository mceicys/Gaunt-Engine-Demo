// path.cpp
// Martynas Ceicys

#include <stdlib.h>
#include <string.h>

#include "path.h"
#include "path_lua.h"
#include "path_private.h"
#include "../console/console.h"
#include "../../GauntCommon/array.h"
#include "../../GauntCommon/io.h"
#include "../hit/hit.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	pat::LoadMap

// Header
char SIG[4] = {0x69, 0x91, 'M', 'p'}
le uint32_t version = 0
le uint32_t flags = 0

le uint32_t numGroundMaps = 0 (FIXME)
le uint32_t numFlightMaps

groundMaps[numGroundMaps]
	FIXME: Unimplemented

flightMaps[numFlightMaps]
	See FlightMap::Load()...
--------------------------------------*/
const char* LoadMapFail(FILE* file, size_t& maxNumLevelsOut, const char* err)
{
	fclose(file);
	pat::ClearMap();
	maxNumLevelsOut = 0;
	return err;
}

#define LOAD_MAP_FAIL(err) return LoadMapFail(file, maxNumLevelsOut, err)

const char* pat::LoadMap(const char* fileName, size_t& maxNumLevelsOut)
{
	maxNumLevelsOut = 0;
	ClearMap();

	if(!fileName)
		return "No navigation map file name given";

	con::LogF("Loading navigation map '%s'", fileName);
	FILE* file = fopen(fileName, "rb");

	if(!file)
		return "Failed to open navigation map file";

	unsigned char header[20];

	if(fread(header, 1, sizeof(header), file) != sizeof(header))
		LOAD_MAP_FAIL("Could not read header");

	if(strncmp((char*)header, "\x69\x91Mp", 4))
		LOAD_MAP_FAIL("Incorrect signature");

	uint32_t version, flags;
	com::MergeLE(header + 4, version);
	com::MergeLE(header + 8, flags);

	if(version != 0)
		LOAD_MAP_FAIL("Unknown version");

	uint32_t numGroundMaps, numFlightMaps;
	com::MergeLE(header + 12, numGroundMaps);
	com::MergeLE(header + 16, numFlightMaps);

	if(numGroundMaps)
		LOAD_MAP_FAIL("Ground maps are not implemented");

	if(const char* err = FlightMap::LoadFlightMaps(file, numFlightMaps, maxNumLevelsOut))
		LOAD_MAP_FAIL(err);

	fclose(file);
	return 0;
}

/*--------------------------------------
	pat::ClearMap
--------------------------------------*/
void pat::ClearMap()
{
	pat::FlightMap::ClearFlightMaps();
}

/*--------------------------------------
	pat::Init
--------------------------------------*/
void pat::Init()
{
	globalFlightMemory.SetInfiniteVisits();
	Ticket<FlightWorkMemory>::Init();

	// Lua
	luaL_Reg regs[] = {
		{"BestFlightMap", BestFlightMap},
		{"InstantFlightPathToPoint", InstantFlightPathToPoint},
		{"FlightNavigator", CreateFlightNavigator},
		{0, 0}
	};

	scr::constant consts[] = {
		{"GO", GO},
		{"FAIL", FAIL},
		{"WAIT", WAIT},
		{"PATH_NONE", PATH_NONE},
		{"PATH_POINT", PATH_POINT},
		{0, 0}
	};

	luaL_Reg fmapRegs[] = {
		{"Hull", FMapHull},
		{"Draw", FMapDraw},
		{0, 0}
	};

	luaL_Reg fnavRegs[] = {
		{"Map", FNavMap},
		{"SetMap", FNavSetMap},
		{"Developing", FNavDeveloping},
		{"Searching", FNavSearching},
		{"ClearAttempt", FNavClearAttempt},
		{"FindPath", FNavFindPath},
		{"ContinueSearch", FNavContinueSearch},
		{"DrawSearch", FNavDrawSearch},
		{"__gc", FlightNavigator::CLuaDelete},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		fmapRegs,
		fnavRegs,
		0
	};

	const char* prefixes[] = {
		"FMap",
		"FNav",
		0
	};

	scr::RegisterLibrary(scr::state, "gpat", regs, consts, 0, metas, prefixes);
	FlightMap::RegisterMetatable(fmapRegs, 0);
	FlightNavigator::RegisterMetatable(fnavRegs, 0);
}

/*--------------------------------------
	pat::PostTick
--------------------------------------*/
void pat::PostTick()
{
	Ticket<FlightWorkMemory>::PostTick();
}