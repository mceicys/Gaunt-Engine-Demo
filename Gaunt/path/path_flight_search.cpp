// path_flight_search.cpp
// Martynas Ceicys

#include "path.h"
#include "path_lua.h"
#include "path_private.h"
#include "../render/render.h"

namespace pat
{
	con::Option optimizeLength("pat_optimize_length", 300.0f);
	FlightWorkMemory globalFlightMemory;

	bool				StartSearchSession(FlightWorkMemory& memIO, const FlightNode*& curOut,
						size_t& selectIndexOut, flight_work_point*& selectOut, state& sOut);
	bool				FinishIteration(FlightWorkMemory& memIO, const FlightNode*& curOut,
						size_t& selectIndexOut, flight_work_point*& selectOut, state& sOut);
	size_t				SelectStaticPoint(FlightWorkMemory& memIO, const FlightNode*& curOut);
	size_t				SelectPoint(FlightWorkMemory& memIO, const FlightNode*& curOut);
	void				ContinueStaticSearch(FlightWorkMemory& memIO, const FlightNode* cur,
						flight_work_point*& selectIO);
	bool				CloseBoundary(FlightWorkMemory& memIO, const flight_work_point* select);
	float				GScore(const FlightWorkMemory& mem, const flight_work_point& pt,
						const flight_work_point& parent);
	float				FScore(const com::Vec3& dest, const flight_work_point& pt);
	flight_work_point*	CreatePortalPoint(FlightWorkMemory& memIO, flight_work_point*& selectIO,
						const FlightPortal& portal);
}

/*--------------------------------------
	pat::InstantPathToPoint

Uses global work-memory that has no visit limit.

FIXME TEMP: Testing if performance is good enough with instant paths so script doesn't have to deal with waiting
--------------------------------------*/
pat::state pat::InstantPathToPoint(const FlightMap& map, const com::Vec3& start,
	const com::Vec3& dest, com::Arr<waypoint>& pts, size_t& numPts)
{
	globalFlightMemory.SetParams(map, PATH_POINT, start, dest);
	state s = Path(globalFlightMemory);

	if(s == GO)
		numPts = globalFlightMemory.TracePath(pts);

	globalFlightMemory.Clear();
	return s;
}

/*--------------------------------------
	pat::Path

Finds a path based on mem's setup.
--------------------------------------*/
pat::state pat::Path(FlightWorkMemory& mem)
{
	switch(mem.pathType)
	{
		case PATH_POINT: return PathToPoint(mem);
		default: return FAIL;
	}
}

/*--------------------------------------
	pat::PathToPoint

FIXME: can change destination mid-search if new dest leaf has no waypoints yet
	...and if it does, can try connecting lowest g waypoint to new destination

FIXME: binary heap, pool waypoints
--------------------------------------*/
pat::state pat::PathToPoint(FlightWorkMemory& mem)
{
	const FlightNode* cur;
	size_t selectIndex;
	flight_work_point* select;
	state s;

	if(!StartSearchSession(mem, cur, selectIndex, select, s))
		return s;

	// Search for destination
	while(1)
	{
		mem.DecVisits();

		if(select)
		{
			if(cur == mem.destLeaf)
			{
				// Waypoint is in destination's leaf
				if(select->pos == mem.dest)
				{
					// Waypoint is exact destination, found winning path
					mem.finale = selectIndex;
					return GO;
				}

				if((select->flags & STOP) == 0)
				{
					// Create new point at destination
					if(mem.pts.Ensure(mem.numPts + 1))
						select = mem.pts.o + selectIndex;

					flight_work_point& pt = mem.pts[mem.numPts++];
					pt.pos = mem.dest;
					pt.leaf = mem.destLeaf;
					pt.portal = 0;
					pt.flags = STOP;
					pt.parent = selectIndex;
					pt.g = GScore(mem, pt, *select);
					pt.f = pt.g;
					pt.numPrevPts = select->numPrevPts + 1;
				}
			}

			ContinueStaticSearch(mem, cur, select);
		}

		if(!FinishIteration(mem, cur, selectIndex, select, s))
			return s;
	}
}

/*--------------------------------------
	pat::DrawSearch
--------------------------------------*/
void pat::DrawSearch(const flight_work_point* pts, size_t num, int color, float time)
{
	for(size_t i = 0; i < num; i++)
	{
		const flight_work_point& child = pts[i];

		if(!child.numPrevPts)
			continue;

		const flight_work_point& parent = pts[child.parent];
		rnd::DrawLine(parent.pos, child.pos, color, time);
	}
}

/*--------------------------------------
	pat::StartSearchSession
--------------------------------------*/
bool pat::StartSearchSession(FlightWorkMemory& mem, const FlightNode*& cur, size_t& selectIndex,
	flight_work_point*& select, state& s)
{
	// Check mutable state
	if(!mem.map)
	{
		CON_ERROR("FlightWorkMemory has no assigned map");
		s = FAIL;
		return false;
	}

	if(!mem.Visits())
	{
		s = WAIT;
		return false;
	}

	if(mem.numPts) // Continuing search
	{
		if((selectIndex = SelectPoint(mem, cur)) == -1)
		{
			s = FAIL;
			return false;
		}

		select = mem.pts.o + selectIndex;
	}
	else // New search
	{
		mem.startLeaf = mem.map->PosToBestLeaf(mem.start);

		if(!mem.startLeaf || (mem.startLeaf->flags & scn::Node::SOLID))
		{
			s = FAIL;
			return false;
		}

		mem.destLeaf = mem.map->PosToBestLeaf(mem.origDest, &mem.dest);

		if(!mem.destLeaf || (mem.destLeaf->flags & scn::Node::SOLID))
		{
			s = FAIL;
			return false;
		}

		cur = mem.startLeaf;

		// Create first waypoint
		mem.pts.Ensure(1);
		mem.numPts++;
		selectIndex = 0;
		select = mem.pts.o;
		select->pos = mem.start;
		select->leaf = mem.startLeaf;
		select->portal = 0;
		select->flags = 0;
		select->g = 0.0f;
		select->f = PAT_WAYPOINT_CLOSED;
		select->parent = -1;
		select->numPrevPts = 0;
	}

	return true;
}

/*--------------------------------------
	pat::FinishIteration
--------------------------------------*/
bool pat::FinishIteration(FlightWorkMemory& mem, const FlightNode*& cur, size_t& selectIndex,
	flight_work_point*& select, state& s)
{
	// Check for interrupt
	if(!mem.Visits())
	{
		s = WAIT;
		return false;
	}

	// Next waypoint
	if((selectIndex = SelectPoint(mem, cur)) == -1)
	{
		s = FAIL;
		return false;
	}

	select = mem.pts.o + selectIndex;
	return true;
}

/*--------------------------------------
	pat::SelectStaticPoint

Returns index of flight_work_point with lowest f or -1 if all are closed. Selected point is
closed. cur is set to the next leaf to search.

FIXME: binary heap
--------------------------------------*/
size_t pat::SelectStaticPoint(FlightWorkMemory& mem, const FlightNode*& cur)
{
	size_t index;
	flight_work_point* select;

	while(1)
	{
		index = -1;
		float bestF = FLT_MAX;

		for(size_t i = 0; i < mem.numPts; i++)
		{
			if(mem.pts[i].f < bestF)
			{
				index = i;
				bestF = mem.pts[i].f;
			}
		}

		if(index == -1)
			return -1;

		select = mem.pts.o + index;
		select->f = PAT_WAYPOINT_CLOSED;

		if(select->portal)
		{
			if(mem.closeBits.True(select->portal->fakeIndex))
				continue; // Portal has been selected before

			cur = select->portal->other;
		}
		else
			cur = select->leaf;

		break;
	}

	return index;
}

/*--------------------------------------
	pat::SelectPoint
--------------------------------------*/
size_t pat::SelectPoint(FlightWorkMemory& mem, const FlightNode*& cur)
{
	return SelectStaticPoint(mem, cur);
}

/*--------------------------------------
	pat::ContinueStaticSearch
--------------------------------------*/
void pat::ContinueStaticSearch(FlightWorkMemory& mem, const FlightNode* cur,
	flight_work_point*& select)
{
	CloseBoundary(mem, select); // Don't try to go thru this portal again

	// Create waypoints at portals
	for(size_t i = 0; i < cur->numPortals; i++)
		CreatePortalPoint(mem, select, cur->portals[i]);
}

/*--------------------------------------
	pat::CloseBoundary

Closes select's portal.
--------------------------------------*/
bool pat::CloseBoundary(FlightWorkMemory& mem, const flight_work_point* select)
{
	if(select->portal)
	{
		mem.closeBits |= select->portal->fakeIndex;

		// FIXME TEMP: portals need to be shared or point to their coportal so it's closed on both sides without this linear search
		for(uint32_t i = 0; i < select->portal->other->numPortals; i++)
		{
			if(select->portal->other->portals[i].other == select->leaf)
				mem.closeBits |= select->portal->other->portals[i].fakeIndex;
		}

		return true;
	}

	return false;
}

/*--------------------------------------
	pat::GScore
--------------------------------------*/
float pat::GScore(const FlightWorkMemory& mem, const flight_work_point& pt,
	const flight_work_point& parent)
{
	return parent.g + (pt.pos - parent.pos).Mag();
}

/*--------------------------------------
	pat::FScore

pt.pos and pt.g must already be calculated.
--------------------------------------*/
float pat::FScore(const com::Vec3& dest, const flight_work_point& pt)
{
	return pt.g + (dest - pt.pos).Mag();
}

/*--------------------------------------
	pat::CreatePortalPoint

Returns the created flight_work_point, or 0 on failure. select's address may be modified if
mem.pts is reallocated.
--------------------------------------*/
pat::flight_work_point* pat::CreatePortalPoint(FlightWorkMemory& mem,
	flight_work_point*& select, const FlightPortal& portal)
{
	if(mem.closeBits.True(portal.fakeIndex))
		return 0; // Closed

	size_t selectIndex = select - mem.pts.o;

	if(mem.pts.Ensure(mem.numPts + 1))
		select = mem.pts.o + selectIndex;

	flight_work_point& pt = mem.pts[mem.numPts++];
	pt.pos = portal.avg; // FIXME: make portals polygons and place point decently close to parent
	pt.leaf = portal.leaf;
	pt.portal = &portal;
	pt.flags = 0;
	pt.parent = select - mem.pts.o;
	pt.g = GScore(mem, pt, *select);
	pt.f = FScore(mem.dest, pt);
	pt.numPrevPts = select->numPrevPts + 1;
	return &pt;
}

/*
################################################################################################


	FLIGHT WORK MEMORY


################################################################################################
*/

/*--------------------------------------
	pat::FlightWorkMemory::TracePath
--------------------------------------*/
size_t pat::FlightWorkMemory::TracePath(com::Arr<waypoint>& path) const
{
	if(finale == -1)
		return 0;

	const flight_work_point* end = &pts[finale];
	size_t index = end->numPrevPts;
	size_t numPathPts = end->numPrevPts + 1;
	path.Ensure(numPathPts);

	for(const flight_work_point* orig = end; ; orig = &pts[orig->parent], index--)
	{
		waypoint& copy = path[index];

		copy.pos = orig->pos;

		if(!index)
			break;
	}

	// FIXME TEMP: Get rid of small segments if possible; required while the nav tree is a mess
	size_t numRem = numPathPts;

	if(numPathPts >= 2)
	{
		float lim = optimizeLength.Float();
		lim *= lim;

		for(size_t di = 1, si = 1; ; di++, si++)
		{
			path[di] = path[si];

			if(si + 1 >= numPathPts)
				break;

			waypoint& cur = path[di];
			waypoint& prev = path[di - 1];
			waypoint& next = path[si + 1];

			if((cur.pos - prev.pos).MagSq() > lim && (next.pos - cur.pos).MagSq() > lim)
			{
				// Segments are long enough
				continue;
			}

			hit::Descent& desc = hit::GlobalDescent();
			desc.Begin(const_cast<FlightNode*>(&map->Nodes()[0]), prev.pos, next.pos);

			while(desc.numStacked)
			{
				scn::Node& node = *desc.s[desc.numStacked - 1].node;

				if(node.flags & node.SOLID)
					break;

				desc.LineDescend();
			}

			if(!desc.numStacked)
			{
				// Didn't hit any solids, skip cur
				di--;
				numRem--;
			}
		}
	}

	return numRem;
}

/*
################################################################################################


	FLIGHT SEARCH LUA


################################################################################################
*/

/*--------------------------------------
LUA	pat::InstantFlightPathToPoint

IN	fmapM, v3Pos, v3Dest, tPathOut, [bSkipStart = false], [iOffset = 0], [iExtraStride = 0]
OUT	iState, [iNumPts]
--------------------------------------*/
int pat::InstantFlightPathToPoint(lua_State* l)
{
	static com::Arr<waypoint> tempPts(32); // FIXME: trace immediately into a table so this isn't necessary
	static size_t numTempPts;

	const int TABLE_INDEX = 8;
	scr::CheckTable(l, TABLE_INDEX);

	state s = InstantPathToPoint(*FlightMap::CheckLuaTo(1), vec::CheckLuaToVec(l, 2, 3, 4),
		vec::CheckLuaToVec(l, 5, 6, 7), tempPts, numTempPts);

	bool skipStart = lua_toboolean(l, 9) == 1;
	lua_Integer offset = luaL_optinteger(l, 10, 0);
	lua_Integer extraStride = luaL_optinteger(l, 11, 0);

	if(s == GO)
	{
		// FIXME: general func since FlightNavigator does this too
		lua_Integer index = offset + 1;
		size_t start = skipStart ? 1 : 0;

		for(size_t i = start; i < numTempPts; i++)
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
		lua_pushinteger(l, numTempPts - start);
		return 2;
	}
	else
	{
		lua_pushinteger(l, s);
		return 1;
	}
}