// convex.h
// Martynas Ceicys

#ifndef COM_CONVEX_H
#define COM_CONVEX_H

#include <stdio.h>

#include "edge.h"
#include "io.h"
#include "link.h"
#include "math.h"
#include "qua.h"
#include "vec.h"

namespace com
{

/*
################################################################################################
	GENERATE HULL
################################################################################################
*/

void		AABBHull(const Vec3& boxMin, const Vec3& boxMax, Vertex*& vertsOut,
			HalfEdge*& edgesOut, Face*& facesOut);
void		AABBOBBSumHull(const Vec3& aMin, const Vec3& aMax, const Vec3& oMin,
			const Vec3& oMax, const Qua& rot, Vertex*& vertsOut, HalfEdge*& edgesOut,
			Face*& facesOut, size_t& numVertsOut, size_t& numEdgesOut, size_t& numFacesOut);
const char*	ConvexHull(const Vec3* verts, size_t numVerts, list<Face>& facesOut,
			Vertex*& vertsOut, fp epsFactor = (fp)1.0, bool saveSteps = false);
void		FreeHull(list<Face>& facesInOut, Vertex*& vertsInOut);
void		FreeHull(Vertex*& vertsInOut, HalfEdge*& edgesInOut, Face*& facesInOut);

/*
################################################################################################
	SEPARATING AXIS THEOREM DATA
################################################################################################
*/

/* FIXME TEMP: ClimbVertex shouldn't be needed with new sequential half-edge structure, unless
climbing becomes a lot slower when getting adjacents using half-edge traversal */
class ClimbVertex
{
public:
	Vec3			pos;
	ClimbVertex**	adjacents;
	size_t			numAdjacents;
	size_t			testCode;

					operator Vec3&() {return pos;}
					operator const Vec3&() const {return pos;}
};

const char*	ConvexVertsAxes(const list<Face>& faces, ClimbVertex*& verticesOut,
			size_t& numVerticesOut, Vec3*& axesOut, size_t& numNormalAxesOut,
			size_t& numEdgeAxesOut, size_t simpleLimit);

const char*	ReadConvexData(ClimbVertex*& verticesOut, size_t& numVerticesOut, Vec3*& axesOut,
			size_t& numNormalAxesOut, size_t& numEdgeAxesOut, size_t simpleLimit, FILE* file);

/*--------------------------------------
	com::WriteConvexData

le uint32_t	numVertices

if numVertices > 0
	le uint32_t	numNormalAxes
	le uint32_t	numEdgeAxes
	vertices[numVertices]
		le vec3_t	pos
		le uint32_t	numAdjacents
		le uint32_t	adjacentIndices[numAdjacents]
	le vec3_t	axes[numNormalAxes + numEdgeAxes]

Returns error string on failure. All floating point values are converted to wfp before writing.
--------------------------------------*/
template <typename wfp> const char* WriteConvexData(const ClimbVertex* vertices,
	size_t numVertices, const Vec3* axes, size_t numNormalAxes, size_t numEdgeAxes, FILE* file)
{
	uint32_t nums[3] = {numVertices, numNormalAxes, numEdgeAxes};

	if(!numVertices)
	{
		if(com::WriteLE(nums, 1, file) != 1)
			return "Could not write number of vertices";

		return 0;
	}

	if(com::WriteLE(nums, 3, file) != 3)
		return "Could not write number of data";

	for(size_t i = 0; i < numVertices; i++)
	{
		wfp v[3] = {vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z};

		if(com::WriteLE(v, 3, file) != 3)
			return "Could not write vertex position";

		uint32_t numAdjacents = vertices[i].numAdjacents;

		if(com::WriteLE(&numAdjacents, 1, file) != 1)
			return "Could not write vertex number of adjacents";

		for(size_t j = 0; j < vertices[i].numAdjacents; j++)
		{
			uint32_t index = vertices[i].adjacents[j] - vertices;

			if(com::WriteLE(&index, 1, file) != 1)
				return "Could not write vertex adjacent index";
		}
	}

	for(size_t i = 0; i < numNormalAxes + numEdgeAxes; i++)
	{
		wfp a[3] = {axes[i].x, axes[i].y, axes[i].z};

		if(com::WriteLE(a, 3, file) != 3)
			return "Could not write axis";
	}

	return 0;
}

}

#endif