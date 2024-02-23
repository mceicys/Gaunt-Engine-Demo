// readnode.h
// Martynas Ceicys

#ifndef READ_NODE_H
#define READ_NODE_H

#include "scene.h"

namespace scn
{
	template <typename T> const char*	ReadNode(FILE* file, T* nodes, size_t numNodes,
										uint32_t nodeID);
	template <typename T> const char*	ReadWorldNode(FILE* file, T* nodes, size_t numNodes,
										uint32_t nodeID);
}

/*--------------------------------------
	scn::ReadNode
--------------------------------------*/
template <typename T> const char* scn::ReadNode(FILE* file, T* nodes, size_t numNodes,
	uint32_t nodeID)
{
	if(!nodeID || nodeID > numNodes)
		return "Invalid node ID";

	T& node = nodes[nodeID - 1];
	node.id = nodeID;

	uint32_t childrenIDs[2];

	if(com::ReadLE(childrenIDs, 2, file) != sizeof(childrenIDs))
		return "Could not read child node IDs";

	if(childrenIDs[0] > numNodes)
		return "Invalid left node ID";

	if(childrenIDs[0])
	{
		T& left = nodes[childrenIDs[0] - 1];
		node.left = &left;
		left.parent = &node;
	}

	if(childrenIDs[1] > numNodes)
		return "Invalid right node ID";

	if(childrenIDs[1])
	{
		T& right = nodes[childrenIDs[1] - 1];
		node.right = &right;
		right.parent = &node;
	}

	if(node.left || node.right)
	{
		float splitPlane[4] = {0.0f};

		if(com::ReadLE(splitPlane, 4, file) != sizeof(splitPlane))
			return "Could not read splitting plane";

		// FIXME: wonder how much slower descents are because these are on the heap...
		node.numPlanes = 1;
		node.planes = new com::Plane[node.numPlanes];

		for(size_t k = 0; k < 3; node.planes->normal[k] = splitPlane[k], k++);
		node.planes->offset = splitPlane[3];
	}

	return 0;
}

/*--------------------------------------
	scn::ReadWorldNode

FIXME: endianness
FIXME: replace solid bool with bit flags
--------------------------------------*/
template <typename T> const char* scn::ReadWorldNode(FILE* file, T* nodes, size_t numNodes,
	uint32_t nodeID)
{
	if(const char* err = ReadNode(file, nodes, numNodes, nodeID))
		return err;

	T& node = nodes[nodeID - 1];

	if(!node.left && !node.right)
	{
		// [1] BOOL solid
		unsigned char solid = 0;
		fread(&solid, sizeof(unsigned char), 1, file);
		node.solid = solid != 0;

		// [4] UINT numPlanes
		fread(&node.numPlanes, sizeof(uint32_t), 1, file);
		if(node.numPlanes)
			node.planes = new com::Plane[node.numPlanes];

		for(uint32_t j = 0; j < node.numPlanes; j++)
		{
			// [12] FLT[3] planeNormal
			float planeNormal[3] = {0.0f};
			fread(planeNormal, sizeof(float), 3, file);

			// [4] FLT planeOffset
			float planeOffset = 0.0f;
			fread(&planeOffset, sizeof(float), 1, file);

			node.planes[j].normal = com::Vec3(planeNormal[0], planeNormal[1], planeNormal[2]);
			node.planes[j].offset = planeOffset;
		}

		// IF solid == true
		if(solid)
		{
			node.planeExits = new bool[node.numPlanes];

			for(uint32_t j = 0; j < node.numPlanes; j++)
			{
				// [1] BOOL exit
				unsigned char exit = 0;
				fread(&exit, sizeof(unsigned char), 1, file);
				node.planeExits[j] = exit != 0;
			}
		}

		// OPTIONAL CONVEX
		com::ClimbVertex* vertices;
		com::Vec3* axes;
		size_t numVertices, numNormalAxes, numEdgeAxes;

		if(const char* err = com::ReadConvexData(vertices, numVertices, axes, numNormalAxes,
		numEdgeAxes, 0, file))
			return err;

		if(vertices)
		{
			node.hull = new hit::Convex(0, vertices, numVertices, axes, numNormalAxes,
				numEdgeAxes, 1);
		}
		else if(node.numPlanes > 1)
			return "Leaf has multiple planes but no convex hull";
	}

	return 0;
}

#endif