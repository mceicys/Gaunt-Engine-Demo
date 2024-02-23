// scene_world.cpp -- Static geometry
// Martynas Ceicys

#include <string.h>
#include <stdlib.h>
#include <math.h> // abs
#include <float.h> // FLT_MAX

#include "scene.h"
#include "readnode.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/tree.h"
#include "../hit/hit.h"
#include "../mod/mod.h"
#include "../path/path.h"
#include "../render/render.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

namespace scn
{
	// WORLD
	bool	LoadPlane(FILE* file, com::Plane& plnOut);
	void	MergePlane(const unsigned char* bytes, com::Plane& plnOut);
	bool	LoadPolygon(FILE* file, com::Poly& polyOut);
	bool	LoadPortalSet(FILE* file, scn::Zone* zones, uint32_t numZones, PortalSet& setOut);
	bool	LoadWorldClose(FILE* file, TriBatch* batches, ZoneExt* zoneExts, const char* err);

	// FIXME: Add Lua interface to nodes, zones, ent links, etc, and then descents.
}

/*
################################################################################################


	WORLDNODE


################################################################################################
*/

/*--------------------------------------
	scn::WorldNode::ClosestLeafTriangle

FIXME: implement uvOut
FIXME: add normal z limit parameter
--------------------------------------*/
const scn::leaf_triangle* scn::WorldNode::ClosestLeafTriangle(const com::Vec3& pos) const
{
	const leaf_triangle* best = 0;
	float bestDist = FLT_MAX;

	for(size_t i = 0; i < numTriangles; i++)
	{
		const leaf_triangle& tri = triangles[i];
		float dist;

		if(com::InteriorProjection(tri.positions, 3, tri.pln.normal, pos, 0.0f))
		{
			dist = fabs(com::PointPlaneDistance(pos, tri.pln));

			// FIXME: calculate UV
		}
		else
		{
			dist = (com::ClosestSegmentPoint(tri.positions[0], tri.positions[1], pos) - pos).MagSq();
			float seg = (com::ClosestSegmentPoint(tri.positions[1], tri.positions[2], pos) - pos).MagSq();

			if(seg < dist)
				dist = seg;

			seg = (com::ClosestSegmentPoint(tri.positions[2], tri.positions[0], pos) - pos).MagSq();

			if(seg < dist)
				dist = seg;

			// FIXME: calculate UV
		}

		if(dist < bestDist)
		{
			bestDist = dist;
			best = &tri;
		}
	}
	
	return best;
}

/*
################################################################################################


	WORLD


################################################################################################
*/

static char* worldFileName = 0;
static scn::WorldNode* nodes = 0;
static uint32_t numNodes = 0;
static size_t maxNumLevels = 0;
static scn::Zone* zones = 0;
static uint32_t numZones = 0;
static scn::PortalSet* portalSets = 0;
static uint32_t numPortalSets = 0;
static res::Ptr<rnd::Texture>* worldTextures = 0;
static uint32_t numWorldTextures = 0;
static com::Vec3 wldMin, wldMax;

/*--------------------------------------
	scn::LoadWorld

FIXME: Don't allocate null nodes, zones, etc (so numNodes is the number of elements in the
	array). An ID of 0 should either be converted to a null pointer or cause an error.

FIXME: endianness
FIXME: standard signature
FIXME: nodes should be ordered to reduce cache misses during descents
--------------------------------------*/
#define LOAD_WORLD_FAIL(err) return LoadWorldClose(file, batches, zoneExts, err)

bool scn::LoadWorld(const char* const fileName)
{
	static const size_t DEF_NUM_ENT_LINKS_ALLOC = 1;
	static const size_t DEF_NUM_BULB_LINKS_ALLOC = 1;

	FILE* file = 0;

	TriBatch* batches = 0;
	ZoneExt* zoneExts = 0;

	ClearWorld();

	if(!fileName)
		LOAD_WORLD_FAIL("No world file name given");

	size_t fileNameLength = strlen(fileName);
	worldFileName = new char[fileNameLength + 1];
	strcpy(worldFileName, fileName);
	con::LogF("Loading world '%s'", fileName);
	const char* err = 0;
	file = mod::FOpen("levels/", fileName, "rb", err);

	if(err)
		LOAD_WORLD_FAIL(err);

	// [8] HEADER
	unsigned char header[8] = {0};
	if(fread(header, sizeof(unsigned char), 8, file) != 8)
		LOAD_WORLD_FAIL("Could not read header");

	if(strncmp("GAUNWD", (char*)header, 6))
		LOAD_WORLD_FAIL("Incorrect signature");

	uint16_t version;
	com::MergeLE(header + 6, version);

	if(version != 0)
		LOAD_WORLD_FAIL("Unknown world file version");

	/*
		TEXTURES
	*/

	// [4] UINT numTextures
	fread(&numWorldTextures, sizeof(uint32_t), 1, file);

	numWorldTextures++; // +1 for default texture at index 0
	worldTextures = new res::Ptr<rnd::Texture>[numWorldTextures];

	if(!worldTextures[0].Set(rnd::EnsureTexture("default.tex")))
		LOAD_WORLD_FAIL("Could not load default texture");

	worldTextures[0]->AddLock(); // FIXME: res::Ptr already sets a lock, what's this for?

	for(uint32_t i = 1; i < numWorldTextures; i++)
	{
		// LINE fileName
		char* fileName = com::ReadLine(file, false);

		if(!fileName)
			LOAD_WORLD_FAIL("Could not read texture file name");

		if(!worldTextures[i].Set(rnd::EnsureTexture(fileName)))
		{
			con::AlertF("Could not load world texture '%s', using default.tex", fileName);
			worldTextures[i].Set(worldTextures[0]);
		}

		worldTextures[i]->AddLock();
	}

	/*
		TRIANGLE BATCHES
	*/

	uint32_t vertexCount = 0, triangleCount = 0;

	// uint32_t numBatches
	uint32_t numBatches;
	com::ReadLE(&numBatches, 1, file);

	if(numBatches)
		batches = new TriBatch[numBatches];

	for(uint32_t i = 0; i < numBatches; i++)
	{
		TriBatch& batch = batches[i];

		// uint32_t textureID
		uint32_t textureID;
		com::ReadLE(&textureID, 1, file);

		if(textureID > numWorldTextures)
			LOAD_WORLD_FAIL("Invalid texture ID in batch");

		batch.tex = textureID ? worldTextures[textureID] : 0;

		// uint32_t numVertices
		com::ReadLE(&batch.numVertices, 1, file);

		if(!batch.numVertices)
			LOAD_WORLD_FAIL("Batch has no vertices");
		
		// vertices[numVertices]
			// float pos[3]
			// float texCoords[2]
			// float normal[3]
			// float subPalette, ambient, darkness
		batch.vertices = new TriBatch::vertex[batch.numVertices];
		com::ReadLE(batch.vertices, batch.numVertices, file);
		batch.firstGlobalVertex = vertexCount;
		vertexCount += batch.numVertices;

		// uint32_t numTriangles
		com::ReadLE(&batch.numTriangles, 1, file);

		if(!batch.numTriangles)
			LOAD_WORLD_FAIL("Batch has no triangles");

		// uint32_t indices[numTriangles * 3]
		uint32_t numElements = batch.numTriangles * 3;
		batch.elements = new uint32_t[numElements];
		com::ReadLE(batch.elements, numElements, file);

		for(uint32_t j = 0; j < numElements; j++)
		{
			if(batch.elements[j] >= batch.numVertices)
				LOAD_WORLD_FAIL("Batch element >= numVertices");
		}

		batch.firstGlobalTriangle = triangleCount;
		triangleCount += batch.numTriangles;
	}

	/*
		NODES
	*/

	wldMin = FLT_MAX;
	wldMax = -FLT_MAX;

	// [4] UINT numNodes
	fread(&numNodes, sizeof(uint32_t), 1, file);

	if(!numNodes)
		LOAD_WORLD_FAIL("numNodes is 0");

	nodes = new WorldNode[numNodes];

	for(uint32_t i = 0; i < numNodes; i++)
	{
		WorldNode& n = nodes[i];
		uint32_t nodeID = i + 1;

		if(const char* err = ReadWorldNode(file, nodes, numNodes, nodeID))
			LOAD_WORLD_FAIL(err);

		// IF leaf == true
		if(!n.left && !n.right)
		{
			// IF solid == false
			if(!n.solid)
			{
				// [4] UINT numPVLs
				// FIXME: remove
				n.numPVLs = 0;
				fread(&n.numPVLs, sizeof(uint32_t), 1, file);

				if(n.numPVLs)
				{
					n.pvls = new WorldNode*[n.numPVLs];

					for(uint32_t j = 0; j < n.numPVLs; j++)
					{
						// [4] UINT nodeID
						uint32_t pvlID;
						fread(&pvlID, sizeof(uint32_t), 1, file);
						n.pvls[j] = &nodes[pvlID - 1];
					}
				}

				// uint32_t numTriangles
				com::ReadLE(&n.numTriangles, 1, file);

				if(n.numTriangles)
					n.triangles = new leaf_triangle[n.numTriangles];

				// uint32_t numTriSequences
				uint32_t numSequences;
				com::ReadLE(&numSequences, 1, file);

				uint32_t numCopied = 0;

				for(uint32_t j = 0; j < numSequences; j++)
				{
					// uint32_t batchIndex
					// uint32_t firstSequenceTriangle
					// uint32_t numSequenceTriangles
					uint32_t seq[3];
					com::ReadLE(seq, 3, file);

					if(seq[0] >= numBatches)
						LOAD_WORLD_FAIL("Invalid batch index in leaf triangle sequence");

					const TriBatch& batch = batches[seq[0]];

					if(seq[1] + seq[2] > batch.numTriangles)
						LOAD_WORLD_FAIL("Out of bounds leaf triangle sequence");

					if(numCopied + seq[2] > n.numTriangles)
						LOAD_WORLD_FAIL("Too many triangles in leaf triangle sequence");

					for(uint32_t k = 0; k < seq[2]; k++)
					{
						leaf_triangle& tri = n.triangles[numCopied];
						size_t e = (seq[1] + k) * 3;
						tri.tex = batch.tex;
						
						for(size_t l = 0; l < 3; l++)
						{
							const TriBatch::vertex& vert = batch.vertices[batch.elements[e + l]];
							tri.positions[l].Copy(vert.pos);
							tri.texCoords[l].Copy(vert.texCoords);
						}

						tri.pln = com::PolygonPlane(tri.positions, 3, false);
						numCopied++;
					}
				}

				if(numCopied != n.numTriangles)
					LOAD_WORLD_FAIL("Not enough triangles in leaf triangle sequences");
			}
			else // IF solid == true
			{
				// [4] UINT numBevels
				n.numBevels = 0;
				fread(&n.numBevels, sizeof(uint32_t), 1, file);

				if(n.numBevels)
					n.bevels = new com::Plane[n.numBevels];

				for(uint32_t j = 0; j < n.numBevels; j++)
				{
					// [12] FLT[3] normal
					// FIXME: general read plane func
					float normal[3] = {0.0f};
					fread(normal, sizeof(float), 3, file);

					// [4] FLT offset
					float offset = 0.0f;
					fread(&offset, sizeof(float), 1, file);

					for(size_t k = 0; k < 3; k++)
						n.bevels[j].normal[k] = normal[k];

					n.bevels[j].offset = offset;
				}
			}

			// Update world box
			if(n.hull)
			{
				const com::Vec3& hMin = n.hull->Min(), hMax = n.hull->Max();

				for(size_t j = 0; j < 3; j++)
				{
					if(hMin[j] < wldMin[j])
						wldMin[j] = hMin[j];
					
					if(hMax[j] > wldMax[j])
						wldMax[j] = hMax[j];
				}
			}

			n.entLinks.Init(DEF_NUM_ENT_LINKS_ALLOC);
		}
	}

	/*
		ZONES
	*/

	// [4] UINT numZones
	// FIXME: remove null zone; zoneIDs should not be used like an index, IDs are base-one
	fread(&numZones, sizeof(uint32_t), 1, file);

	if(numZones)
	{
		zones = new Zone[numZones + 1];
		zoneExts = new ZoneExt[numZones + 1];
	}

	for(uint32_t i = 0; i < numZones; i++)
	{
		Zone& z = zones[i + 1];
		ZoneExt& ze = zoneExts[i + 1];
		z.id = i + 1;

		// [1] uint8_t flags
		uint8_t flags;
		com::ReadLE(&flags, 1, file);
		z.flags = flags;

		// [4] UINT numLeaves
		z.numLeaves = 0;
		fread(&z.numLeaves, sizeof(uint32_t), 1, file);

		if(z.numLeaves)
			z.leaves = new WorldNode*[z.numLeaves];

		for(uint32_t j = 0; j < z.numLeaves; j++)
		{
			// [4] UINT nodeID
			uint32_t nodeID;
			fread(&nodeID, sizeof(uint32_t), 1, file);

			if(!nodeID || nodeID > numNodes)
				LOAD_WORLD_FAIL("Invalid leaf ID in zone");

			uint32_t nodeIdx = nodeID - 1;

			if(nodes[nodeIdx].left || nodes[nodeIdx].right)
				LOAD_WORLD_FAIL("Zone given node ID to non-leaf");

			if(nodes[nodeIdx].zone)
				LOAD_WORLD_FAIL("Leaf is in more than one zone");

			z.leaves[j] = &nodes[nodeIdx];

			nodes[nodeIdx].zone = &z;
		}

		// uint32_t firstBatchIndex
		com::ReadLE(&ze.firstBatch, 1, file);

		// uint32_t numBatches
		com::ReadLE(&ze.numBatches, 1, file);

		if(ze.firstBatch + ze.numBatches > numBatches)
			LOAD_WORLD_FAIL("Zone has out of bounds triangle-batch sequence");

		for(uint32_t j = ze.firstBatch; j < ze.firstBatch + ze.numBatches; j++)
			batches[j].zone = &z;

		// uint32_t numLinkedPortalSets
		z.portals = 0;
		z.numPortals = 0;
		com::ReadLE(&ze.numPortalsAlloc, 1, file);

		if(ze.numPortalsAlloc)
			z.portals = new PortalSet*[ze.numPortalsAlloc];
	}

	for(uint32_t i = 0; i < numBatches; i++)
	{
		if(!batches[i].zone)
			LOAD_WORLD_FAIL("Triangle batch doesn't belong to any zone");

		if(i && batches[i - 1].zone->id > batches[i].zone->id)
			LOAD_WORLD_FAIL("Triangle batches are not sorted by zone");
	}

	/*
		PORTAL SETS
	*/

	// uint32_t numPortalSets
	com::ReadLE(&numPortalSets, 1, file);
	
	if(numPortalSets)
		portalSets = new PortalSet[numPortalSets];

	for(uint32_t i = 0; i < numPortalSets; i++)
	{
		if(!LoadPortalSet(file, zones, numZones, portalSets[i]))
			LOAD_WORLD_FAIL("Invalid portal set");

		Zone *front = portalSets[i].front, *back = portalSets[i].back;

		if(front->numPortals >= zoneExts[front->id].numPortalsAlloc ||
		back->numPortals >= zoneExts[back->id].numPortalsAlloc)
			LOAD_WORLD_FAIL("Zone has unexpected number of portals");

		front->portals[front->numPortals++] = &portalSets[i];
		back->portals[back->numPortals++] = &portalSets[i];
	}

	for(uint32_t i = 0; i < numZones; i++)
	{
		if(zones[i].numPortals != zoneExts[i].numPortalsAlloc)
			LOAD_WORLD_FAIL("Zone allocated excess portal set links");
	}

	/*
		RENDER REGISTRATION
	*/

	rnd::RegisterWorld(batches, numBatches, zones, zoneExts, numZones);

	/*
		NAVIGATION MAPS
	*/

	maxNumLevels = 0;

	char* mapFileName = new char[fileNameLength + 1];
	strcpy(mapFileName, worldFileName);
	strcpy(&mapFileName[fileNameLength - 3], "map");

	const char *mapErr = 0, *path = mod::Path("levels/", mapFileName, mapErr);

	if(mapErr || (mapErr = pat::LoadMap(path, maxNumLevels)))
		con::LogF("Failed to load navigation map (%s)", mapErr);

	delete[] mapFileName;

	/*
		PREPARE PRIMARY DESCENT STACK
	*/

	if(numNodes)
	{
		size_t numLevels = com::NumTreeLevels(&nodes[1]);
		maxNumLevels = COM_MAX(maxNumLevels, numLevels);
	}

	hit::GlobalDescent().FitLargestTree();

	/*
		DONE
	*/

	LoadWorldClose(file, batches, zoneExts, 0);
	return true;
}

/*--------------------------------------
	scn::LoadPlane
--------------------------------------*/
bool scn::LoadPlane(FILE* file, com::Plane& pln)
{
	unsigned char bytes[sizeof(float) * 4];

	if(fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
		return false;

	MergePlane(bytes, pln);
	return true;
}

/*--------------------------------------
	scn::MergePlane
--------------------------------------*/
void scn::MergePlane(const unsigned char* bytes, com::Plane& pln)
{
	float normal[3] = {0.0f}, offset = 0.0f;
	com::MergeLE(bytes, normal, 3);
	com::MergeLE(bytes + 12, &offset, 1);

	for(size_t j = 0; j < 3; j++)
		pln.normal[j] = normal[j];

	pln.offset = offset;
}

/*--------------------------------------
	scn::LoadPolygon
--------------------------------------*/
bool scn::LoadPolygon(FILE* file, com::Poly& poly)
{
	uint32_t numVerts = 0;
	com::ReadLE(numVerts, file);

	if(!numVerts)
		return false;

	poly.SetVerts(0, numVerts);

	for(size_t i = 0; i < poly.numVerts; i++)
	{
		float vert[3] = {0.0f};
		uint32_t read = com::ReadLE(vert, 3, file);

		if(read != sizeof(vert))
			return false;

		poly.verts[i].Copy(vert);
	}

	return true;
}

/*--------------------------------------
	scn::LoadPortalSet
--------------------------------------*/
bool scn::LoadPortalSet(FILE* file, scn::Zone* zones, uint32_t numZones, PortalSet& set)
{
	unsigned char bytes[28];
	if(fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
		return false;

	// uint32_t frontID, backID
	uint32_t frontID, backID;
	com::MergeLE(bytes, &frontID, 1);
	com::MergeLE(bytes + 4, &backID, 1);

	// FIXME: do >= comparison if null zone is not allocated
	if(!frontID || frontID > numZones || !backID || backID > numZones || frontID == backID)
	{
		con::LogF("Invalid portal set zone IDs");
		return false;
	}

	set.front = &zones[frontID];
	set.back = &zones[backID];

	// PLANE
	com::Plane pln;
	MergePlane(bytes + 8, pln);

	// uint32_t numPolys
	com::MergeLE(bytes + 24, &set.numPolys, 1);

	if(!set.numPolys)
	{
		con::LogF("Portal set has no polygons");
		return false;
	}

	set.polys = new com::Poly[set.numPolys];

	for(uint32_t i = 0; i < set.numPolys; i++)
	{
		// POLY
		if(!LoadPolygon(file, set.polys[i]))
			return false;

		set.polys[i].pln = pln;
	}

	return true;
}

/*--------------------------------------
	scn::LoadWorldClose

err 0 means success and does not clear the world. Always returns false.
--------------------------------------*/
bool scn::LoadWorldClose(FILE* file, TriBatch* batches, ZoneExt* zoneExts, const char* err)
{
	if(file)
		fclose(file);

	if(batches)
		delete[] batches;

	if(zoneExts)
		delete[] zoneExts;

	if(err)
	{
		if(worldFileName) {
			con::LogF("Failed to load world '%s' (%s)", worldFileName, err);
			ClearWorld();
		} else {
			con::LogF("Failed to load world (%s)", err);
		}
	}

	return false;
}

/*--------------------------------------
	scn::ClearWorld

FIXME: found heap corruption when this was called during reload of horse.sav; use application verifier
--------------------------------------*/
void scn::ClearWorld()
{
	// Nodes
	if(nodes)
		delete[] nodes;

	nodes = 0;
	numNodes = 0;

	// Zones
	if(zones)
		delete[] zones;

	zones = 0;
	numZones = 0;

	// Textures
	if(worldTextures)
	{
		for(uint32_t i = 0; i < numWorldTextures; i++)
		{
			if(worldTextures[i])
				worldTextures[i]->RemoveLock();
		}

		delete[] worldTextures;
	}

	worldTextures = 0;
	numWorldTextures = 0;

	// General
	wldMin = wldMax = 0.0f;

	if(worldFileName)
		delete[] worldFileName;

	worldFileName = 0;
}

/*--------------------------------------
	scn::WorldFileName
--------------------------------------*/
const char* scn::WorldFileName() {return worldFileName;}

/*--------------------------------------
	scn::WorldRoot
--------------------------------------*/
scn::WorldNode* scn::WorldRoot() {return nodes ? &nodes[0] : 0;}

/*--------------------------------------
	scn::WorldMin
--------------------------------------*/
const com::Vec3& scn::WorldMin() {return wldMin;}

/*--------------------------------------
	scn::WorldMax
--------------------------------------*/
const com::Vec3& scn::WorldMax() {return wldMax;}

/*--------------------------------------
	scn::CheckLuaToWorldNode
--------------------------------------*/
scn::WorldNode* scn::CheckLuaToWorldNode(lua_State* l, int index)
{
	size_t i = (size_t)luaL_checkinteger(l, index);

	if(i >= numNodes)
		luaL_argerror(l, index, "Node index out of bounds");

	return &nodes[i];
}

/*--------------------------------------
	scn::CheckLuaToLeafTriangle
--------------------------------------*/
scn::leaf_triangle* scn::CheckLuaToLeafTriangle(lua_State* l, int nodeIndex, int triIndex)
{
	const WorldNode* node = CheckLuaToWorldNode(l, nodeIndex);
	int tri = luaL_checkinteger(l, triIndex);

	if(tri >= node->numTriangles)
		luaL_argerror(l, triIndex, "Leaf triangle index out of bounds");

	return &node->triangles[tri];
}

/*--------------------------------------
	scn::MaxNumTreeLevels
--------------------------------------*/
size_t scn::MaxNumTreeLevels()
{
	return maxNumLevels;
}

/*--------------------------------------
	scn::Zones
--------------------------------------*/
const scn::Zone* scn::Zones() {return zones + 1;} // FIXME: remove null zone

/*--------------------------------------
	scn::NumZones
--------------------------------------*/
uint32_t scn::NumZones() {return numZones;}

/*--------------------------------------
	scn::PortalSets
--------------------------------------*/
const scn::PortalSet* scn::PortalSets() {return portalSets;}

/*--------------------------------------
	scn::NumPortalSets
--------------------------------------*/
size_t scn::NumPortalSets() {return numPortalSets;}

/*--------------------------------------
	scn::PosToLeaf
--------------------------------------*/
const scn::Node* scn::PosToLeaf(const Node* root, const com::Vec3& pos)
{
	if(!root)
		return 0;

	const Node* curNode = root;

	while(curNode->left)
	{
		float dist = com::Dot(pos, curNode->planes->normal) - curNode->planes->offset;

		if(dist >= 0.0f)
			curNode = curNode->right;
		else
			curNode = curNode->left;
	}

	return curNode;
}

scn::Node* scn::PosToLeaf(Node* root, const com::Vec3& pos)
{
	return const_cast<Node*>(PosToLeaf(const_cast<const Node*>(root), pos));
}

/*--------------------------------------
	scn::IncZoneDrawCode
--------------------------------------*/
unsigned scn::IncZoneDrawCode()
{
	// FIXME: remove + 1 if null zone is not allocated
	COM_INC_MARK_CODE_ARRAY(drawCode, zones + 1, numZones);
}

/*--------------------------------------
	scn::NearbyTriangle

FIXME: output UV
--------------------------------------*/
const scn::leaf_triangle* scn::NearbyTriangle(const WorldNode* root, const com::Vec3& pos,
	const WorldNode*& leafOut)
{
	leafOut = (const WorldNode*)PosToLeaf(root, pos);

	if(!leafOut)
		return 0;

	return leafOut->ClosestLeafTriangle(pos);
}