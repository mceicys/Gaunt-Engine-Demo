// path.h -- Pathfinding system
// Martynas Ceicys

// FIXME: things like Ticket and FlightWorkMemory should be private but they're public due to bad structuring

#ifndef PATH_H
#define PATH_H

#include "path_ticket.h"
#include "../flag/flag.h"
#include "../../GauntCommon/array.h"
#include "../../GauntCommon/json.h"
#include "../../GauntCommon/math.h"
#include "../../GauntCommon/pool.h"
#include "../../GauntCommon/vec.h"
#include "../hit/hit.h"
#include "../resource/resource.h"
#include "../vector/vec_lua.h"

namespace pat
{

class FlightPortal;
class FlightNode;
class FlightWorkMemory;

enum state
{
	GO = 0,
	FAIL,
	WAIT
};

enum path_type {
	PATH_NONE,
	PATH_POINT
};

struct waypoint
{
	com::Vec3 pos;
};

/*
################################################################################################
	FLIGHT MAP
################################################################################################
*/

/*======================================
	pat::FlightMap
======================================*/
class FlightMap : public res::Resource<FlightMap>
{
public:
	float				Epsilon() const {return epsilon;}
	const hit::Hull&	Hull() const {return *hull;}
	const FlightNode*	Nodes() const {return nodes;}
	uint32_t			NumNodes() const {return numNodes;}
	uint32_t			NumPortals() const {return numPortals;}
	const FlightNode*	PosToBestLeaf(const com::Vec3& pos, com::Vec3* fixedOut = 0) const;
	FlightNode*			PosToBestLeaf(const com::Vec3& pos, com::Vec3* fixedOut = 0);
	void				Draw(int color, float time = 0.0f) const;

	static const char*	LoadFlightMaps(FILE* file, uint32_t num, size_t& maxNumLevelsIO);
	static void			ClearFlightMaps();

private:
	float				epsilon;
	res::Ptr<hit::Hull>	hull;
	FlightNode*			nodes;
	uint32_t			numNodes, numPortals;

						FlightMap() : epsilon(0.0f), hull(0), nodes(0), numNodes(0),
							numPortals(0) {AddLock(); /* Permanent lock */}
						~FlightMap();

	const char*			Load(FILE* file, size_t& maxNumLevelsIO);
};

const FlightMap* BestFlightMap(const com::Vec3& min, const com::Vec3& max);

/*
################################################################################################
	FLIGHT SEARCH
################################################################################################
*/

extern con::Option optimizeLength;

state	InstantPathToPoint(const FlightMap& map, const com::Vec3& start, const com::Vec3& dest,
		com::Arr<waypoint>& ptsOut, size_t& numPtsOut);

/*
################################################################################################
	FLIGHT NAVIGATOR

FIXME: Remove; all path searches are finished on call. If it turns out waiting is still
	required, make caller get a temporary object or code when they start a search so they can
	get the result later
################################################################################################
*/

/*======================================
	pat::FlightNavigator

Finds flight paths

FIXME: saving & loading
======================================*/
class FlightNavigator : public res::Resource<FlightNavigator>
{
public:
				FlightNavigator();
				~FlightNavigator();

	FlightMap*	Map() const {return map;}
	void		SetMap(FlightMap* f);
	bool		Developing() const {return ticket.Queued();} // Waiting or searching
	bool		Searching() const {return ticket.Memory();} // Searching
	void		ClearAttempt();
	state		FindPath(const com::Vec3& pos, const com::Vec3& dest,
				com::Arr<waypoint>& ptsOut, size_t& numPtsOut);
	void		ContinueSearch();

	void		DrawSearch(int color, float time = 0.0f) const;

	FlightMap*	LuaCheckMap(lua_State* l) const;

private:
	// Flags
	enum
	{
		HOLD_FAIL = 1
	};

	// Parameters
	FlightMap*					map;
	com::Vec3					savedPos, savedDest;
	Ticket<FlightWorkMemory>	ticket;
	uint32_t					flags;

	state	DevelopPath();
	void	ClearDevelopment();
};

/*
################################################################################################
	WORK MEMORY
################################################################################################
*/

// Work-point flags
enum {
	STOP = 1 // Search isn't continued, this is only an attempt for a winning point
};

struct flight_work_point : public waypoint
{
#if 0
	// FIXME: implement
	// Min heap node indices
	// Unused work_points use left and right as prev and next in a linked list
	size_t						left, right, up;
#endif

	const FlightNode*	leaf;
	const FlightPortal*	portal; // The exit from referenced leaf, not the entrance into leaf
	uint32_t			flags;
	float				g, f; // Cost so far, estimated final cost from start to destination
	size_t				parent;
	size_t				numPrevPts;
};

/*======================================
	pat::FlightWorkMemory

Work space for flight path search functions
======================================*/
class FlightWorkMemory
{
public:
	// Modifiable search data
	com::Vec3 dest;
	size_t numPts;
	com::Arr<flight_work_point> pts; // FIXME: global pool, see min heap FIXME comment in flight_work_point above
	flg::FlagSet closeBits; // Corresponding bit is set if a portal is closed
	const FlightNode *startLeaf, *destLeaf;
	size_t finale;

	// Search parameters; constant during a search
	const FlightMap* map;
	path_type pathType;
	com::Vec3 start, origDest;

	FlightWorkMemory() : numPts(0), startLeaf(0), destLeaf(0), finale(-1), map(0),
		pathType(PATH_NONE), visits(0)
	{
		closeBits.AddLock(); // FIXME: make a non-Resource bit set class
	}

	~FlightWorkMemory() {pts.Free();}

	void SetParams(const FlightMap& fmap, path_type pathType, const com::Vec3& start,
	const com::Vec3& dest)
	{
		Clear();
		map = &fmap;
		this->pathType = pathType;
		this->start = start;
		this->origDest = dest;

		size_t numPortals = map->NumPortals();
		pts.Ensure(numPortals);
		closeBits.EnsureNumBits(numPortals);
	}

	void Clear()
	{
		pathType = PATH_NONE;
		numPts = 0;
		closeBits.Clear();
		startLeaf = destLeaf = 0;
		finale = -1;
		map = 0;
	}

	unsigned Visits() const {return visits;}

	unsigned DecVisits()
	{
		if(visits == -1)
			return visits;

		if(visits)
			visits--;

		return visits;
	}

	void RestoreVisits() {visits = maxNumVisits.Integer();}

	void SetInfiniteVisits() {visits = -1;}

	size_t TracePath(com::Arr<waypoint>& pathOut) const;

private:
	unsigned visits;
};

/*
################################################################################################
	GENERAL
################################################################################################
*/

const char*	LoadMap(const char* fileName, size_t& maxNumLevelsOut);
void		ClearMap();
void		Init();
void		PostTick();

}

#endif