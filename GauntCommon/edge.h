// edge.h
// Martynas Ceicys

/*
FIXME
Not ready for general use

* Solid but flexible interface
	* Must be flexible for:
		* Cutting faces adjacent to other faces
		* All the things ConvexHull does
			* Dissolve vertex
				* Should repair degenerated two-edge faces immediately
			* Dissolve edge
				* Should recursively repair inner edges immediately
				* Make sure to cancel if disconnected rings would be created
		* Should have general heal functionality
			* e.g.
				* Dissolve coplanar/concave edges (with assumption that geometry is convex)
				* Turn small edges into vertices
				* etc.
			* Useful for other operations, like creating areas
* A way to traverse all things (faces, edges / half-edges, vertices) once would be nice
	* Make required enveloping structure which keeps track of all elements in pooled lists
		* All elements will have to point to other elements via index
			* This also makes cloning simple and fast
		* ConvexHull will have to extend faces with a separate pool aligned to the face pool
* Get rid of need for ClimbVertex; have everything available in half-edge structure
	* The only thing that would need to be added is the testCode
	* The testCode could also be used for flood filling (which quickhull requires); a generic
	testCode for each element isn't a bad idea
		* Or use a bit array
* Enveloping structure is called a Graph
* All operation functions detect and fix or prevent degeneration relevant to operands
	* e.g. Vertex::Dissolve fixes potential two-half-edge face
	* Vertex::Dissolve cancels if it will set a pointer to something about to be deleted
* General functions to detect degeneracy
	* Every face must have at least three half-edges in a loop
	* Every half-edge must have two distinct vertices
	* Provide functions to detect related attributes (e.g. closed)
* Rename these files to graph.h and graph.cpp
*/

#ifndef COM_EDGE_H
#define COM_EDGE_H

#include "vmath.h"
#include "vec.h"

namespace com
{

/*======================================
	com::TempVertex
======================================*/
template <class HE> class TempVertex
{
public:
	Vec3	pos;
	HE*		first; // The vertex is the tail of this edge

			TempVertex() : first(0) {}
			operator Vec3&() {return pos;}
			operator const Vec3&() const {return pos;}
};

/*======================================
	com::TempHalfEdge
======================================*/
template <class V, class HE, class F> class TempHalfEdge
{
public:
	F*	face;

	HE*	twin;

	HE*	prev;
	HE*	next;

	V*	tail;
	V*	head;

	/* FIXME: NewNext and NewTwin assume HalfEdge has certain vertices, so maybe HalfEdges
	without both vertices should not be allowed to exist */
		TempHalfEdge() : face(0), twin(0), prev(0), next(0), tail(0), head(0) {}

	/*--------------------------------------
		com::TempHalfEdge::NewNext
	--------------------------------------*/
	HE* TempHalfEdge::NewNext()
	{
		next = new HE;
		next->prev = (HE*)this;
		next->tail = head;
		next->face = face;

		if(!head->first)
			head->first = next;

		return next;
	};

	HE* TempHalfEdge::NewNextToVertex(V* v)
	{
		NewNext();
		next->head = v;

		return next;
	}

	HE* TempHalfEdge::NewNextToEdge(HE* connect)
	{
		NewNext();
		next->next = connect;
		next->head = connect->tail;
		connect->prev = next;

		return next;
	}

	/*--------------------------------------
		com::TempHalfEdge::NewTwin
	--------------------------------------*/
	HE* TempHalfEdge::NewTwin()
	{
		twin = new HE;
		twin->twin = (HE*)this;

		twin->tail = head;
		twin->head = tail;

		if(!head->first)
			head->first = twin;

		return twin;
	}
};

/*======================================
	com::TempFace
======================================*/
template <class HE, class F> class TempFace
{
public:
	HE*		first;
	Plane	pln;

			TempFace() : first(0), pln((fp)0.0, (fp)0.0) {}
			TempFace(HE& first) : first(&first), pln((fp)0.0, (fp)0.0)
			{
				first.face = (F*)this;
			}

	/*--------------------------------------
		com::TempFace::UpdatePlane
	--------------------------------------*/
	void TempFace::UpdatePlane()
	{
		Vec3 v[3] = {first->tail->pos, first->head->pos, first->next->head->pos};
		pln.normal = CalculateNormal(v, 3, false);
		pln.offset = Dot(pln.normal, v[0]);
	}

	/*--------------------------------------
		com::TempFace::Healthy
	--------------------------------------*/
	bool TempFace::Healthy(size_t minNumEdges = 0) const
	{
		if(!first)
			return false;

		size_t numEdges = 0;
		HE* it = first;
		do
		{
			numEdges++;

			if(!it->next || !it->tail || !it->head || it->head == it->tail ||
			it->next->prev != it || it->next->tail != it->head)
				return false;

			it = it->next;
		} while(it != first);

		return numEdges >= minNumEdges;
	}

	/*--------------------------------------
		com::TempFace::Triangular
	--------------------------------------*/
	bool TempFace::Triangular() const
	{
		return first->next->next->next == first;
	}

	/*--------------------------------------
		com::TempFace::NumVerts
	--------------------------------------*/
	unsigned TempFace::NumVerts() const
	{
		unsigned count = 0;
		HE* it = first;

		do
		{
			count++;
		} while((it = it->next) && it != first);

		return count;
	}
};

// Can't get inherited constructors to work, add this to derived face classes
#define COM_TEMP_FACE_INHERIT_CONSTRUCTORS(HE, F) \
	F() : TempFace() {} \
	F(HE& first) : TempFace(first) {}

// Basic derived classes
class Vertex;
class HalfEdge;
class Face;

class Vertex : public TempVertex<HalfEdge> {};
class HalfEdge : public TempHalfEdge<Vertex, HalfEdge, Face> {};

class Face : public TempFace<HalfEdge, Face>
{
public:
	COM_TEMP_FACE_INHERIT_CONSTRUCTORS(HalfEdge, Face)
};

/*
################################################################################################
	CLONE

FIXME TEMP: Temporary structure and function for storing and recreating closed half-edge graphs
from indices. Once modifiable half-edge structures have an enveloping Graph class, a common
Clone function will be implemented there.
################################################################################################
*/

struct graph_template
{
	struct e
	{
		size_t face, twin, prev, next, tail, head;
	};

	size_t numVerts, numEdges, numFaces;

	const size_t*	verts; // Value is first-edge index
	const e*		edges;
	const size_t*	faces; // Value is first-edge index
};

void CloneGraph(const graph_template& t, Vertex*& vertsOut, HalfEdge*& edgesOut,
	Face*& facesOut, size_t& numVertsOut, size_t& numEdgesOut, size_t& numFacesOut);

}

#endif