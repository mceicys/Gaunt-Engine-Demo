// path_private.h
// Martynas Ceicys

#ifndef PATH_PRIVATE_H
#define PATH_PRIVATE_H

#include "path.h"
#include "../../GauntCommon/array.h"
#include "../../GauntCommon/edge.h"
#include "../hit/hit.h"
#include "../scene/scene.h"

#define PAT_WAYPOINT_CLOSED FLT_MAX

namespace pat
{

/*
################################################################################################
	FLIGHT MAP
################################################################################################
*/

// FIXME: can make these classes public but restrict member access to const funcs

/*======================================
	pat::FlightPortal
======================================*/
class FlightPortal
{
public:
	uint32_t	fakeIndex; // FIXME TEMP: using this to refer to close-bits during search even though all of a map's portals are not stored in a sequential array at the moment
	com::Vec3	avg;
	FlightNode	*leaf, *other;

	FlightPortal() : fakeIndex(0), leaf(0), other(0) {}
};

/*======================================
	pat::FlightNode
======================================*/
class FlightNode : public scn::Node
{
public:
	FlightPortal*	portals;
	uint32_t		numPortals;

	FlightNode() : portals(0), numPortals(0) {}
	~FlightNode() { if(portals) delete[] portals; }
};

extern FlightMap*	flightMaps;
extern uint32_t		numFlightMaps;

/*
################################################################################################
	FLIGHT SEARCH
################################################################################################
*/

extern FlightWorkMemory globalFlightMemory;

state	Path(FlightWorkMemory& memIO);
state	PathToPoint(FlightWorkMemory& memIO);
void	DrawSearch(const flight_work_point* pts, size_t num, int color, float time = 0.0f);

}

#endif