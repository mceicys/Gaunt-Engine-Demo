// edge.cpp
// Martynas Ceicys

#include "edge.h"

/*
################################################################################################
	
	
	CLONE


################################################################################################
*/

/*--------------------------------------
	com::CloneGraph

FIXME TEMP
--------------------------------------*/
void com::CloneGraph(const graph_template& t, Vertex*& vertsOut, HalfEdge*& edgesOut,
	Face*& facesOut, size_t& numVertsOut, size_t& numEdgesOut, size_t& numFacesOut)
{
	vertsOut = new Vertex[t.numVerts];
	edgesOut = new HalfEdge[t.numEdges];
	facesOut = new Face[t.numFaces];

	for(size_t i = 0; i < t.numVerts; i++)
		vertsOut[i].first = edgesOut + t.verts[i];

	for(size_t i = 0; i < t.numEdges; i++)
	{
		HalfEdge& e = edgesOut[i];

		e.face = facesOut + t.edges[i].face;
		e.twin = edgesOut + t.edges[i].twin;
		e.prev = edgesOut + t.edges[i].prev;
		e.next = edgesOut + t.edges[i].next;
		e.tail = vertsOut + t.edges[i].tail;
		e.head = vertsOut + t.edges[i].head;
	}

	for(size_t i = 0; i < t.numFaces; i++)
		facesOut[i].first = edgesOut + t.faces[i];

	numVertsOut = t.numVerts;
	numEdgesOut = t.numEdges;
	numFacesOut = t.numFaces;
}