// path_flight_map.cpp
// Martynas Ceicys

#include "path.h"
#include "path_lua.h"
#include "path_private.h"
#include "../../GauntCommon/tree.h"
#include "../render/render.h"
#include "../scene/readnode.h"

namespace pat
{
	FlightMap*	flightMaps = 0;
	uint32_t	numFlightMaps = 0;

	// DESCENT MIN HEAP
	struct loser_node
	{
		const FlightNode *node;
		float dist;
		com::Vec3 pos;
	};

	void		InsertLoser(com::Arr<loser_node>& losersIO, size_t& numLosersIO,
				const FlightNode* node, float dist, const com::Vec3& pos);
	loser_node	PopLoser(com::Arr<loser_node>& losersIO, size_t& numLosersIO);
}

const char* const pat::FlightMap::METATABLE = "metaFlightMap";

/*--------------------------------------
	pat::FlightMap::PosToBestLeaf

FIXME: move to scn, generalize for scn::Node
--------------------------------------*/
const pat::FlightNode* pat::FlightMap::PosToBestLeaf(const com::Vec3& pos,
	com::Vec3* fixed) const
{
	if(!nodes)
		return 0;

	static com::Arr<loser_node> losers(32);
	size_t numLosers = 0;

	const FlightNode* cur = nodes;
	com::Vec3 curPos = pos;

	while(1)
	{
		while(!cur->left)
		{
			// Got to a leaf
			if(cur->flags & cur->SOLID)
			{
				// Solid leaf
				if(!numLosers)
				{
					// Whole tree is solid
					cur = 0;
					break;
				}

				// Cross to losing side of closest splitting plane
				loser_node other = PopLoser(losers, numLosers);
				cur = other.node;
				com::Plane& splitter = *other.node->parent->planes;

				// Restore position at the time of this descent and project it onto plane
				curPos = other.pos + splitter.normal *
					(splitter.offset - com::Dot(other.pos, splitter.normal));
			}
			else
				break; // Non-solid leaf, done
		}

		if(!cur->left)
			break; // Done

		float dist = com::Dot(pos, cur->planes->normal) - cur->planes->offset;
		const FlightNode *loser;

		if(dist >= 0.0f)
		{
			loser = (const FlightNode*)cur->left;
			cur = (const FlightNode*)cur->right;
		}
		else
		{
			loser = (const FlightNode*)cur->right;
			cur = (const FlightNode*)cur->left;
		}

		InsertLoser(losers, numLosers, loser, fabs(dist), curPos);
	}

	if(fixed)
		*fixed = curPos;

	return cur;
}

pat::FlightNode* pat::FlightMap::PosToBestLeaf(const com::Vec3& pos, com::Vec3* fixed)
{
	return const_cast<FlightNode*>(const_cast<const FlightMap*>(this)->PosToBestLeaf(pos, fixed));
}

/*--------------------------------------
	pat::FlightMap::Draw
--------------------------------------*/
void pat::FlightMap::Draw(int color, float time) const
{
	for(FlightNode* n = nodes; n; n = com::TraverseTree(n))
	{
		if(!n->numPortals)
			continue;

		for(uint32_t i = 0; i < n->numPortals; i++)
		{
			for(uint32_t j = i + 1; j < n->numPortals; j++)
				rnd::DrawLine(n->portals[i].avg, n->portals[j].avg, color, time);
		}
	}
}

/*--------------------------------------
	pat::FlightMap::~FlightMap
--------------------------------------*/
pat::FlightMap::~FlightMap()
{
	if(hull)
		hull->RemoveLock();

	if(nodes)
		delete[] nodes;
}

/*--------------------------------------
	pat::FlightMap::Load

le float	aabb[3][2]
le uint32_t	numNodes

flightNavNodes[numNodes]
	le uint32_t leftNodeID, rightNodeID

	if leftNodeID || rightNodeID
		PLANE splittingPlane
	else
		le uint32_t leafFlags
			SOLID = 1

		if (leafFlags & SOLID) == 0
			le uint32_t numPortals
			portals[numPortals]
				le float avg[3]
				le uint32_t otherNodeID
--------------------------------------*/
const char* pat::FlightMap::Load(FILE* file, size_t& maxNumLevelsIO)
{
	float aabb[6];

	if(com::ReadLE(aabb, 6, file) != sizeof(aabb))
		return "Could not read aabb";

	hull.Set(new hit::Hull(com::Vec3(aabb[0], aabb[1], aabb[2]),
		com::Vec3(aabb[3], aabb[4], aabb[5])));

	hull->AddLock();

	if(com::ReadLE(numNodes, file) != sizeof(numNodes))
		return "Could not read numNodes";

	if(numNodes)
		nodes = new FlightNode[numNodes];

	numPortals = 0;

	for(uint32_t i = 0; i < numNodes; i++)
	{
		FlightNode& node = nodes[i];
		uint32_t nodeID = i + 1;

		if(const char* err = scn::ReadNode(file, nodes, numNodes, nodeID))
			return err;

		if(!node.left && !node.right)
		{
			// FIXME: make leaf flags standard, load them in scn::ReadNode
			if(com::ReadLE(node.flags, file) != sizeof(node.flags))
				return "Could not read leafFlags";

			if((node.flags & node.SOLID) == 0)
			{
				if(com::ReadLE(node.numPortals, file) != sizeof(node.numPortals))
					return "Could not read numPortals";

				if(node.numPortals)
				{
					node.portals = new FlightPortal[node.numPortals];

					for(uint32_t portIndex = 0; portIndex < node.numPortals; portIndex++)
					{
						FlightPortal& portal = node.portals[portIndex];
						portal.fakeIndex = numPortals++; // FIXME: don't accumulate count once portals are shared by leaves instead of being dup'd
						portal.leaf = &node;
						unsigned char portalBytes[16];

						if(fread(portalBytes, 1, sizeof(portalBytes), file) !=
						sizeof(portalBytes))
							return "Could not read portal";

						float avg[3];
						com::MergeLE(portalBytes, avg, 3);
						portal.avg.Copy(avg);

						uint32_t otherNodeID;
						com::MergeLE(portalBytes + 12, otherNodeID);

						if(otherNodeID == 0 || otherNodeID > numNodes)
							return "Invalid otherNodeID";

						portal.other = &nodes[otherNodeID - 1];
					}
				}
			}
		}
	}

	if(nodes)
		maxNumLevelsIO = com::Max(maxNumLevelsIO, com::NumTreeLevels(&nodes[0]));

	return 0;
}

/*--------------------------------------
	pat::FlightMap::LoadFlightMaps
--------------------------------------*/
const char* pat::FlightMap::LoadFlightMaps(FILE* file, uint32_t num, size_t& maxNumLevelsIO)
{
	ClearFlightMaps();

	if(!num)
		return 0;

	numFlightMaps = num;
	flightMaps = new FlightMap[numFlightMaps];

	for(uint32_t i = 0; i < numFlightMaps; i++)
	{
		if(const char* err = flightMaps[i].Load(file, maxNumLevelsIO))
			return err;
	}

	return 0;
}

/*--------------------------------------
	pat::FlightMap::ClearFlightMaps
--------------------------------------*/
void pat::FlightMap::ClearFlightMaps()
{
	if(flightMaps)
		delete[] flightMaps;

	flightMaps = 0;
	numFlightMaps = 0;
}

/*--------------------------------------
	pat::BestFlightMap

FIXME: prefer map hulls that envelop the given AABB fully to avoid unusable paths
--------------------------------------*/
const pat::FlightMap* pat::BestFlightMap(const com::Vec3& min, const com::Vec3& max)
{
	FlightMap* best = 0;
	float bestMinMag = FLT_MAX, bestMaxMag = FLT_MAX;

	for(uint32_t i = 0; i < numFlightMaps; i++)
	{
		FlightMap& map = flightMaps[i];
		const hit::Hull& hull = map.Hull();
		float minMag = (hull.Min() - min).MagSq();
		
		if(minMag < bestMinMag)
		{
			float maxMag = (hull.Max() - max).MagSq();

			if(maxMag < bestMaxMag)
			{
				best = &map;
				bestMinMag = minMag;
				bestMaxMag = maxMag;
			}
		}
	}

	return best;
}

/*
################################################################################################


	FLIGHT MAP LUA


################################################################################################
*/

/*--------------------------------------
LUA	pat::BestFlightMap

IN	v3Min, v3Max
OUT	[fmapF]
--------------------------------------*/
int pat::BestFlightMap(lua_State* l)
{
	const FlightMap* f = BestFlightMap(vec::CheckLuaToVec(l, 1, 2, 3),
		vec::CheckLuaToVec(l, 4, 5, 6));

	if(f)
	{
		f->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	pat::FMapHull (Hull)

IN	fmapF
OUT	hulH
--------------------------------------*/
int pat::FMapHull(lua_State* l)
{
	FlightMap::CheckLuaTo(1)->Hull().LuaPush();
	return 1;
}

/*--------------------------------------
LUA	pat::FMapDraw (Draw)

IN	fmapF, iColor, [nTime = 0]
--------------------------------------*/
int pat::FMapDraw(lua_State* l)
{
	FlightMap::CheckLuaTo(1)->Draw(luaL_checkinteger(l, 2), luaL_optnumber(l, 3, 0.0));
	return 0;
}

/*
################################################################################################
	
	
	DESCENT MIN HEAP


################################################################################################
*/

/*--------------------------------------
	pat::InsertLoser
--------------------------------------*/
void pat::InsertLoser(com::Arr<loser_node>& losers, size_t& numLosers,
	const FlightNode* node, float dist, const com::Vec3& pos)
{
	size_t index = numLosers;
	losers.Ensure(++numLosers);
	loser_node& add = losers[index];
	add.node = node;
	add.dist = dist;
	add.pos = pos;

	while(index)
	{
		size_t parent = (index - 1) / 2;

		if(losers[parent].dist > losers[index].dist)
		{
			com::Swap(losers[parent], losers[index]);
			index = parent;
		}
		else
			break;
	}
}

/*--------------------------------------
	pat::PopLoser
--------------------------------------*/
pat::loser_node pat::PopLoser(com::Arr<loser_node>& losers, size_t& numLosers)
{
	loser_node pop = losers[0];
	losers[0] = losers[--numLosers];
	size_t index = 0;

	while(1)
	{
		size_t swap = index;
		size_t left = index * 2 + 1;
		size_t right = index * 2 + 2;

		if(left < numLosers && losers[index].dist > losers[left].dist)
			swap = left;

		if(right < numLosers && losers[swap].dist > losers[right].dist)
			swap = right;

		if(swap == index)
			break;

		com::Swap(losers[index], losers[swap]);
		index = swap;
	}

	return pop;
}