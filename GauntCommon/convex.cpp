// convex.cpp
// Martynas Ceicys

#include <math.h>
#include <string.h>

#include "convex.h"
#include "convex_graphs.h"
#include "convex_sum_lookup.h"
#include "array.h"
#include "link.h"

#ifdef COM_CONVEX_SAVE_ERROR_STEPS
#include "obj.h"
#endif

#ifdef _DEBUG
#define COM_CONVEX_DEBUGGING 1
#endif

#ifdef COM_CONVEX_DEBUGGING
#include "obj.h"
#endif

#define SUM_EPSILON_FACTOR (fp)10.0

namespace com
{
	/*======================================
		com::QHullFace
	======================================*/
	class QHullFace : public Face
	{
	public:
		linker<Face>*	listItem;
		HalfEdge*		fill; // From which edge face was filled
		bool			done; // No active verts in front

		QHullFace(HalfEdge& first, list<Face>& facesInOut)
		: Face(first), fill(0), done(0)
		{
			Link(facesInOut.f, facesInOut.l, (Face*)this);
			listItem = facesInOut.l;
		}

		QHullFace(list<Face>& facesInOut)
		: fill(0), done(0)
		{
			Link(facesInOut.f, facesInOut.l, (Face*)this);
			listItem = facesInOut.l;
		}

		void			Delete(list<Face>& facesInOut);
		void			Delete(bool keepBounds, list<Face>& facesInOut);

	private:
		~QHullFace() {};
	};

	// GENERATE HULL
	enum edge_health
	{
		HEALTHY,
		DISSOLVE,
		PERPENDICULAR
	};

	const char*			ConvexHullFail(const char* err, const Vec3* verts, size_t numVerts,
						bool* actives, list<Face>& facesOut, Vertex*& vertsOut, fp epsFactor,
						bool saveSteps);
	void				DisableRedundant(const Vertex* verts, size_t numVerts, bool* actives,
						fp epsilon);
	const char*			InitialTriangles(Vertex* verts, bool* actives, size_t numVerts,
						list<Face>& facesInOut);
	size_t				FarthestVert(const Plane& pln, const Vertex* verts, const bool* actives,
						size_t numVerts, fp epsilon);
	linker<HalfEdge>*	Horizon(QHullFace*& startInOut, const Vec3& v, fp epsilon,
						list<Face>& facesInOut);
	const char*			BoundaryTriangles(linker<HalfEdge>*& lBoundIO, Vertex& v,
						list<Face>& facesIO, bool& mergedEdgeOut);
	const char*			Heal(linker<Face>* lSafeFace, fp epsilon, list<Face>& facesInOut);
	bool				DissolveRedundantVertex(HalfEdge*& edgeInOut, list<Face>& facesInOut);
	bool				DissolveEdge(HalfEdge& edge, list<Face>& facesInOut);
	bool				MergeEdge(HalfEdge& edge, list<Face>& facesIO);
	edge_health			EdgeHealth(const HalfEdge& edge, fp epsilon);
	Vertex*				FarthestEdgeVert(const HalfEdge& edge);
	const Plane&		BestPlane(const HalfEdge* edge);
	void				SaveStart(const Vec3* verts, size_t numVerts);
	void				SaveStep(int step, const Vertex* verts, size_t numVerts,
						const list<Face>& faces);

	// SEPARATING AXIS THEOREM DATA
	const char*			ReadConvexDataClose(ClimbVertex*& verticesInOut, size_t numVertices,
						Vec3*& axesInOut, const char* err);

#ifdef COM_CONVEX_DEBUGGING
	void				SaveFaceOBJ(const char* path, const Face* f);
	void				SaveFacesOBJ(const char* path, const linker<Face>* firstFace);
#endif
};

#ifdef COM_CONVEX_DEBUGGING
static unsigned debugHullCounter = 0;
static unsigned debugStepCounter = 0;
#endif

/*
################################################################################################
	
	
	QHULLFACE


################################################################################################
*/

/*--------------------------------------
	com::QHullFace::Delete
--------------------------------------*/
void com::QHullFace::Delete(list<Face>& facesInOut)
{
	Unlink(facesInOut.f, facesInOut.l, listItem);
	delete this;
}

// FIXME: template specialization
void com::QHullFace::Delete(bool keepBounds, list<Face>& facesInOut)
{
	HalfEdge* it = first;

	do
	{
		HalfEdge* next = it->next;

		if(keepBounds && it->twin && !((QHullFace*)it->twin->face)->fill)
		{
			// Don't delete boundary edges, just unlink
			it->face = 0;
			it->next = it->prev = 0;
		}
		else
		{
			if(it->tail->first == it)
			{
				if(it->twin && it->twin->next)
					it->tail->first = it->twin->next;
				else
					it->tail->first = 0;
			}

			if(it->twin)
				it->twin->twin = 0;

			delete it;
		}

		it = next;
	} while(it != first);

	Delete(facesInOut);
}

/*
################################################################################################
	
	
	GENERATE HULL


################################################################################################
*/

/*--------------------------------------
	com::AABBHull

Generates AABB. vertsOut order is determined by BoxVerts.
--------------------------------------*/
void com::AABBHull(const Vec3& boxMin, const Vec3& boxMax, Vertex*& vertsOut,
	HalfEdge*& edgesOut, Face*& facesOut)
{
	size_t numVerts, numEdges, numFaces;
	CloneGraph(ALIGNED_GRAPH, vertsOut, edgesOut, facesOut, numVerts, numEdges, numFaces);
	BoxVerts(boxMin, boxMax, vertsOut);

	for(size_t i = 0; i < 3; i++)
	{
		facesOut[i].pln.normal = 0.0f;
		facesOut[i].pln.normal[i] = 1.0f;
		facesOut[i].pln.offset = boxMax[i];
	}

	for(size_t i = 3; i < 6; i++)
	{
		facesOut[i].pln.normal = 0.0f;
		facesOut[i].pln.normal[i - 3] = -1.0f;
		facesOut[i].pln.offset = -boxMin[i - 3];
	}
}

/*--------------------------------------
	com::AABBOBBSumHull

Generates the sum of an AABB and an OBB.
--------------------------------------*/
void com::AABBOBBSumHull(const Vec3& aMin, const Vec3& aMax, const Vec3& oMin, const Vec3& oMax,
	const Qua& rot, Vertex*& vertsOut, HalfEdge*& edgesOut, Face*& facesOut,
	size_t& numVertsOut, size_t& numEdgesOut, size_t& numFacesOut)
{
	fp epsilon = COM_MAX(com::Epsilon(aMin), com::Epsilon(aMax));
	epsilon = COM_MAX(epsilon, com::Epsilon(oMin));
	epsilon = COM_MAX(epsilon, com::Epsilon(oMax));

	epsilon *= SUM_EPSILON_FACTOR;

	Vec3 f = VecRot(Vec3((fp)1.0, (fp)0.0, (fp)0.0), rot).Normalized();
	Vec3 s = VecRot(Vec3((fp)0.0, (fp)1.0, (fp)0.0), rot).Normalized();
	Vec3 u = Cross(f, s);

	const sum_plan* plan;

	size_t prm = QuantizeVecPrimary(f, epsilon);
	size_t sec = QuantizeVecSecondary(prm, s, epsilon);

	if(prm >= 18 && sec >= 6)
	{
		size_t trt = QuantizeVecTertiary(prm, sec, u, epsilon);
		plan = &SUM_PLANS_1[prm - 18][sec - 6][trt];
	}
	else
		plan = &SUM_PLANS_0[prm][sec];

	CloneGraph(*plan->graph, vertsOut, edgesOut, facesOut, numVertsOut, numEdgesOut,
		numFacesOut);

	Vec3 aVerts[8];
	Vec3 oVerts[8];

	BoxVerts(aMin, aMax, aVerts);
	BoxVerts(oMin, oMax, f, s, u, oVerts);

	for(size_t i = 0; i < numVertsOut; i++)
		vertsOut[i].pos = aVerts[plan->pairs[i].aabbVert] + oVerts[plan->pairs[i].obbVert];

	for(size_t i = 0; i < numFacesOut; i++)
		facesOut[i].UpdatePlane();
}

/*--------------------------------------
	com::ConvexHull

If 0 is returned, facesOut is set to a list of faces forming a convex hull around verts.
vertsOut is set to a new[] com::Vertex array. Otherwise, an error string is returned. Call
FreeHull to delete the faces and vertices.

Faces are counter-clockwise from the front.
--------------------------------------*/
#define CONVEX_HULL_FAIL(err) return ConvexHullFail(err, verts, numVerts, actives, facesOut, \
	vertsOut, epsFactor, !saveSteps)

const char* com::ConvexHull(const Vec3* verts, size_t numVerts, list<Face>& facesOut,
	Vertex*& vertsOut, fp epsFactor, bool saveSteps)
{
#if COM_CONVEX_DEBUGGING
	debugHullCounter++;
	debugStepCounter = 0;
#endif

	facesOut.f = facesOut.l = 0;

	vertsOut = new Vertex[numVerts];

	for(size_t i = 0; i < numVerts; i++)
		vertsOut[i].pos = verts[i];

	bool* actives = new bool[numVerts];
	memset(actives, true, sizeof(bool) * numVerts);

	fp epsilon = Epsilon(verts, numVerts) * epsFactor;

#if COM_CONVEX_SAVE_ERROR_STEPS
	int step = 0;
	if(saveSteps)
		SaveStart(verts, numVerts);
#endif

	// Construction
	DisableRedundant(vertsOut, numVerts, actives, epsilon);

	if(const char* err = InitialTriangles(vertsOut, actives, numVerts, facesOut))
		CONVEX_HULL_FAIL(err);

#if COM_CONVEX_SAVE_ERROR_STEPS
	if(saveSteps)
		SaveStep(step, vertsOut, numVerts, facesOut);
#endif

	while(1)
	{
		QHullFace* front = 0;
		size_t v;

		for(linker<Face>* it = facesOut.f; it; it = it->next)
		{
			QHullFace* face = (QHullFace*)it->o;

			if(face->done)
				continue;

			v = FarthestVert(face->pln, vertsOut, actives, numVerts, epsilon);

			if(v != numVerts)
			{
				front = face;
				break;
			}

			face->done = true;
		}

		if(!front)
			break;

		actives[v] = false;

		linker<HalfEdge>* lBound = Horizon(front, verts[v], epsilon, facesOut);

		if(!lBound)
			CONVEX_HULL_FAIL("No boundary triangles");

		linker<Face>* lSafeFace = facesOut.l;
		bool mergedEdge;

		if(const char* err = BoundaryTriangles(lBound, vertsOut[v], facesOut, mergedEdge))
			CONVEX_HULL_FAIL(err);

		if(mergedEdge) // Merged a boundary edge, can't be sure old faces are healthy
			lSafeFace = 0;

		while(lBound)
			Unlink(lBound, lBound);

		if(const char* err = Heal(lSafeFace, epsilon, facesOut))
			CONVEX_HULL_FAIL(err);

#if COM_CONVEX_DEBUGGING
		debugStepCounter++;
#endif

#if COM_CONVEX_SAVE_ERROR_STEPS
		if(saveSteps)
			SaveStep(++step, vertsOut, numVerts, facesOut);
#endif
	}

	delete[] actives;

	return 0;
}

/*--------------------------------------
	com::ConvexHullFail

FIXME: ConvexHull should fail softly by returning a copy of the last successful step along with
	the distance of the farthest vertex outside the hull. Most of the errors now are negligible
	but are still treated catastrophically, and that's interrupting NeonWorld too often.
--------------------------------------*/
const char* com::ConvexHullFail(const char* err, const Vec3* verts, size_t numVerts,
	bool* actives, list<Face>& facesOut, Vertex*& vertsOut, fp epsFactor, bool saveSteps)
{
#if COM_CONVEX_SAVE_ERROR_STEPS
	if(saveSteps)
	{
		com::list<Face> tempFaces;
		com::Vertex* tempVerts;
		ConvexHull(verts, numVerts, tempFaces, tempVerts, epsFactor, true);
		FreeHull(tempFaces, tempVerts); // Just in case it succeeded this time
	}
#endif

	delete[] actives;
	FreeHull(facesOut, vertsOut);
	return err;
}

/*--------------------------------------
	com::FreeHull
--------------------------------------*/
void com::FreeHull(list<Face>& facesInOut, Vertex*& vertsInOut)
{
	while(facesInOut.l)
		((QHullFace*)facesInOut.l->o)->Delete(false, facesInOut);

	delete[] vertsInOut;
	vertsInOut = 0;
}

void com::FreeHull(Vertex*& vertsInOut, HalfEdge*& edgesInOut, Face*& facesInOut)
{
	delete[] vertsInOut;
	delete[] edgesInOut;
	delete[] facesInOut;
}

/*--------------------------------------
	com::DisableRedundant

Disables one vertex for each pair of vertices within epsilon distance of each other.
--------------------------------------*/
void com::DisableRedundant(const Vertex* verts, size_t numVerts, bool* actives, fp epsilon)
{
	Vec3 avg = verts[0];
	for(size_t i = 1; i < numVerts; i++)
		avg += verts[i];

	avg /= (fp)numVerts;

	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		for(size_t j = i + 1; j < numVerts; j++)
		{
			if(!actives[j])
				continue;

			if((verts[i].pos - verts[j].pos).Mag() <= epsilon)
			{
				float mi = (avg - verts[i].pos).MagSq();
				float mj = (avg - verts[j].pos).MagSq();

				if(mi >= mj)
					actives[j] = false;
				else
				{
					actives[i] = false;
					break;
				}
			}
		}
	}
}

/*--------------------------------------
	com::InitialTriangles

Creates initial faces and adds them to the given list. Returns an error string on failure.
--------------------------------------*/
const char* com::InitialTriangles(Vertex* verts, bool* actives, size_t numVerts,
	list<Face>& facesInOut)
{
	size_t v[3] = {numVerts, numVerts, numVerts};

	// First vertex is farthest from average
	Vec3 avg = 0.0f;
	size_t numAccum = 0;
	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		avg += verts[i];
		numAccum++;
	}

	avg /= (fp)numAccum;

	fp mag = (fp)0.0;
	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		fp m = (avg - verts[i]).Mag();

		if(m > mag)
		{
			mag = m;
			v[0] = i;
		}
	}

	if(v[0] == numVerts)
		return "Could not get first initial vertex";

	actives[v[0]] = false;

	// Second vertex is farthest from first
	mag = (fp)0.0;
	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		fp m = (verts[v[0]].pos - verts[i].pos).Mag();

		if(m > mag)
		{
			mag = m;
			v[1] = i;
		}
	}

	if(v[1] == numVerts)
		return "Could not get second initial vertex";

	actives[v[1]] = false;

	// Third vertex is farthest from first edge's infinite line
	mag = (fp)0.0;
	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		fp f = ProjectPointLine(verts[v[0]].pos, verts[v[1]].pos, verts[i].pos);
		fp m = (verts[i].pos - COM_LERP(verts[v[0]].pos, verts[v[1]].pos, f)).Mag();

		if(m > mag)
		{
			mag = m;
			v[2] = i;
		}
	}

	if(v[2] == numVerts)
		return "Could not get third initial vertex";

	actives[v[2]] = false;

	// Fourth vertex is farthest from first triangle's plane
	Plane pln;
	Vec3 iv[3] = {verts[v[0]].pos, verts[v[1]].pos, verts[v[2]].pos};
	pln.normal = CalculateNormal(iv, 3, false);
	pln.offset = Dot(pln.normal, iv[0]);

	if(pln.normal == (fp)0.0)
		return "Initial triangle has indeterminate plane";

	mag = (fp)0.0;
	size_t top = numVerts;

	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		fp m = Dot(verts[i].pos, pln.normal) - pln.offset;

		if(fabs(m) > fabs(mag))
		{
			top = i;
			mag = m;
		}
	}

	if(top == numVerts)
		return "Could not get fourth initial vertex";

	actives[top] = false;

	if(mag == (fp)0.0)
		return "Initial tetrahedron is coplanar";

	// Flip initial triangle to face away from fourth vertex
	if(mag > (fp)0.0)
	{
		size_t temp = v[1];
		v[1] = v[2];
		v[2] = temp;
		pln = -pln;
	}

	// Edge
	HalfEdge* init = new HalfEdge;
	init->tail = &verts[v[0]];
	init->tail->first = init;
	init->head = &verts[v[1]];

	// Triangle
	new QHullFace(*init, facesInOut);
	init->NewNextToVertex(&verts[v[2]]);
	init->next->NewNextToEdge(init);
	init->face->pln = pln;

	// Tetrahedron
	HalfEdge* itEdge = init;

	do
	{
		itEdge->twin = new HalfEdge;
		itEdge->twin->twin = itEdge;
		itEdge->twin->head = itEdge->tail;
		itEdge->twin->tail = itEdge->head;
		new QHullFace(*itEdge->twin, facesInOut);
		itEdge->twin->NewNextToVertex(&verts[top]);
		itEdge->twin->next->NewNextToEdge(itEdge->twin);
		itEdge->twin->face->UpdatePlane();

		if(itEdge->twin->face->pln.normal == (fp)0.0)
			return "Initial tetrahedron triangle has indeterminate plane";

		itEdge = itEdge->next;
	} while(itEdge != init);

	itEdge = init;

	do
	{
		itEdge->twin->next->twin = itEdge->prev->twin->prev;
		itEdge->twin->prev->twin = itEdge->next->twin->next;

		itEdge = itEdge->next;
	} while(itEdge != init);

	return 0;
}

/*--------------------------------------
	com::FarthestVert

Returns numVerts if no vert is in front of pln.
--------------------------------------*/
size_t com::FarthestVert(const Plane& pln, const Vertex* verts, const bool* actives,
	size_t numVerts, fp epsilon)
{
	size_t best = numVerts;
	float bestDist = epsilon;

	for(size_t i = 0; i < numVerts; i++)
	{
		if(!actives[i])
			continue;

		float dist = Dot(pln.normal, verts[i]) - pln.offset;

		if(dist > bestDist)
		{
			best = i;
			bestDist = dist;
		}
	}

	return best;
}

/*--------------------------------------
	com::Horizon

Returns list of edges bounding the faces that see v. List last to first is clockwise. The faces
and non-boundary edges are deleted. startInOut is set to 0.
--------------------------------------*/
com::linker<com::HalfEdge>* com::Horizon(QHullFace*& startInOut, const Vec3& v, fp epsilon,
	list<Face>& facesInOut)
{
	linker<HalfEdge>* lBound = 0;

	HalfEdge* port = startInOut->first;
	startInOut->fill = port;

	bool begun = false; // First iteration complete (to avoid stopping immediately)

	while(1)
	{
		if(port == ((QHullFace*)port->face)->fill && begun)
		{
			// Reached edge we've come from before
			QHullFace* deadFace = (QHullFace*)port->face;
			
			if(port == startInOut->fill)
			{
				// Starting edge; stop
				startInOut = 0;
				port = 0;
			}
			else
				// Not starting edge; go back through
				port = port->twin;

			// Delete face we were just on
			deadFace->Delete(true, facesInOut);
		}
		else if(port->twin && !((QHullFace*)port->twin->face)->fill)
		{
			// Face on other side not filled/deleted
			// Test for front or boundary face
			Plane& adjPln = port->twin->face->pln;
			float dist = Dot(adjPln.normal, v) - adjPln.offset;

			if(dist > epsilon)
			{
				// Fill
				port = port->twin;
				((QHullFace*)port->face)->fill = port;
			}
			else
				Link(lBound, port); // Found boundary
		}

		begun = true;

		if(port)
			port = port->prev;
		else
			break;
	}

	return lBound;
}

/*--------------------------------------
	com::BoundaryTriangles

Creates triangles connecting boundary edges to v.
--------------------------------------*/
const char* com::BoundaryTriangles(linker<HalfEdge>*& lBoundIO, Vertex& v, list<Face>& facesIO,
	bool& mergedEdgeOut)
{
	linker<HalfEdge>* it = lBoundIO;

	while(it)
	{
		HalfEdge* base = it->o;
		QHullFace* face = new QHullFace(*base, facesIO);
		base->NewNextToVertex(&v);
		base->next->NewNextToEdge(base);
		base->face->UpdatePlane();

		if(it->next)
		{
			// Connect twin edges of this new triangle and last new triangle
			HalfEdge* prevBase = it->next->o;
			base->prev->twin = prevBase->next;
			prevBase->next->twin = base->prev;
		}

		if(!it->prev)
		{
			// Set final twin link
			HalfEdge* firstBase = lBoundIO->o;
			base->next->twin = firstBase->prev;
			firstBase->prev->twin = base->next;

			// And set v's first ptr
			firstBase->prev->tail->first = firstBase->prev;
			break;
		}

		it = it->prev;
	}

	// Check for degenerate triangles
	it = lBoundIO;
	mergedEdgeOut = false;

	while(it)
	{
		HalfEdge* base = it->o;
		linker<HalfEdge> *prev = it->prev;

		if(base->face->pln.normal == (fp)0.0)
		{
			// Triangle has indeterminate plane
			// Delete base since it's probably a small edge collinear with v
			bool keepTail = (base->tail->pos - v.pos).MagSq() >= (base->head->pos - v.pos).MagSq();
			
			if(!MergeEdge(keepTail ? *base : *base->twin, facesIO))
				return "Created triangle with indeterminate plane and failed to remove";

			mergedEdgeOut = true;
			com::Unlink(lBoundIO, it);
		}

		it = prev;
	}

	return 0;
}

/*--------------------------------------
	com::Heal

Merges faces that share a coplanar or concave edge and fixes errors in the process. lSafeFace
is the last face to skip healing or 0 to check all faces.

Error examples:

# = vertex
lines = edge
arrows = comments

Multiple edges are opposite one face

   #
  / \
 /  <\<<< Where we dissolved edge
#--#<<#<< Error
 \ | /
  \|/
   #

Fixed naturally by dissolving wherever possible, but specifically checked for by looking for
connected edges opposite the same face (but not their own face). This explicit check is done so
we can merge collinear edges to get rid of useless vertices. 3D example below.

     #
    / \
   /   #
  /   /|
 /   #<|<< Edge dissolve will fail due to plane check, but edge merge is fine
#   /  #
 \ /  /
  #  /
  | /
  |/
  #

The explicit check also helps in preventing bad dissolves, as using the FarthestEdgeVert to
check for concavity or coplanarity in low precision circumstances when there are redundant
vertices present can give bad results. 3D example below.

     __,--#<< FarthestEdgeVert of extremely small edge; note it's coplanar with face below
#--''    /|
 \      / |
  \    /  #
   #--#<</<< Extremely small edge that will be dissolved, merging faces that shouldn't be merged
   |    /
   |  </<< Where we dissolved an edge to remove an extremely thin face
   |  /
   | /
   #/

Dissolving redundant vertices also prevents inner edges:

#---#---#
 \   <</<< Where we dissolved edge
  \ #</< Previously redundant vertex
   \|/< Error
    #
--------------------------------------*/
const char* com::Heal(linker<Face>* lSafeFace, fp epsilon, list<Face>& facesInOut)
{
	unsigned faceIndex = 0;
	linker<Face>* start = lSafeFace ? lSafeFace->next : facesInOut.f;

	for(linker<Face>* itFace = start; itFace; itFace = itFace->next, faceIndex++)
	{
		HalfEdge* itEdge = itFace->o->first;

		while(1)
		{
			/* FIXME: Look ahead to see if the hull will degenerate if this edge is dissolved.
			Redundant vertex dissolving is based entirely on discrete logic, so there might even
			be a simple state to check for without going through the process of dissolving and
			then reverting that work. Look at why_should_this_edge_not_be_dissolved.blend and
			figure it out. */
			if(edge_health eh = EdgeHealth(*itEdge, epsilon))
			{
				if(eh == DISSOLVE)
				{
					if(!DissolveEdge(*itEdge, facesInOut))
						return "Failed to dissolve edge; hull degenerated";
				}
				else if(eh == PERPENDICULAR)
					return "Edge perpendicular to its face";
			}
			else
			{
				itEdge = itEdge->next;

				if(itEdge == itFace->o->first)
					break; // No dissolves possible; done
				else
					continue;
			}

			// Dissolved an edge
			// Merge connected edges opposite the same face to remove useless vertices
			itEdge = itFace->o->first;	
			bool merged;
			do
			{
				merged = false;
				if(itEdge->twin->face == itEdge->next->twin->face)
				{
					if(!DissolveRedundantVertex(itEdge, facesInOut))
						return "Failed to dissolve redundant vertex; hull degenerated";

					merged = true;
				}
				else
					itEdge = itEdge->next;
			} while(itEdge != itFace->o->first || merged);

			// itEdge is at itFace->o->first right now; merge cycle is restarted
		}
	}

	return 0;
}

/*--------------------------------------
	com::DissolveRedundantVertex

Dissolves edge->head if it is the tail of only edge->next and edge->twin; edgeInOut is set to
the edge with the next vertex as its head. If dissolving the vertex will turn one of the faces
into a two-vertex face, dissolves the two edges, instead; edgeInOut is set to the previous edge.
Faces must be closed.

Cancels and returns false if degeneration is detected.
--------------------------------------*/
bool com::DissolveRedundantVertex(HalfEdge*& edgeInOut, list<Face>& facesInOut)
{
	HalfEdge* edge = edgeInOut;

	// FIXME: Vertex::Dissolve should check elements specifically related to the operation
	if(!edge->face->Healthy() || !edge->twin->face->Healthy())
		return false;

	if(edge->next == edge->twin) // Two-vertex face
		return false; // edge->twin is accessed after edge->next is deleted; that won't work here

	HalfEdge* edgeNext = edge->next;
	HalfEdge* twinPrev = edge->twin->prev;

	// Ensure vertex is redundant
	Vertex& head = *edge->head;
	HalfEdge* it = head.first;

	do
	{
		if((it != edgeNext && it != edge->twin) || !it->prev || !it->prev->twin)
			return false;

		it = it->prev->twin;
	} while(it != head.first);

	// FIXME: This turns a double-sided triangle into a two-vertex face
	if(edge->face->Triangular() || twinPrev->face->Triangular())
	{
		edgeInOut = edge->prev;
		return DissolveEdge(*edge, facesInOut) && DissolveEdge(*twinPrev, facesInOut);
	}

	edge->head->first = 0; // This vertex is "removed," so set its first value to 0

	if(edge->face->first == edgeNext)
		edge->face->first = edgeNext->next;

	if(edge->twin->face->first == twinPrev)
		edge->twin->face->first = twinPrev->prev;

	if(edge->next->head->first == twinPrev)
		edge->next->head->first = edge->twin;

	edge->next = edgeNext->next;
	edgeNext->next->prev = edge;
	edge->head = edgeNext->head;
	delete edgeNext;

	edge->twin->prev = twinPrev->prev;
	twinPrev->prev->next = edge->twin;
	edge->twin->tail = twinPrev->tail;
	delete twinPrev;

	return true;
}

/*--------------------------------------
	com::DissolveEdge

Deletes edge and its twin. Merges faces if the edge is shared by different faces. Structurally,
edge.face is kept. edge's vertices' first ptrs are set to the new edge they're the tail of if
needed. true is returned if a merge occured.

Cancels and returns false if degeneration is detected.
--------------------------------------*/
bool com::DissolveEdge(HalfEdge& edge, list<Face>& facesInOut)
{
	HalfEdge& twin = *edge.twin;
	QHullFace& face = *(QHullFace*)edge.face;

	// FIXME: Edge::Dissolve should check elements specifically related to the operation
	if(!face.Healthy(2) || (twin.face != &face && !twin.face->Healthy(2)))
		return false;

	if(twin.face != &face)
	{
		// Begin face merge
		if(twin.next->next != &twin) // Don't change plane if twin face is a 2-gon
			face.pln = BestPlane(&edge);

		HalfEdge* it;
		for(it = twin.next; it != &twin; it = it->next)
			it->face = &face;

		((QHullFace*)twin.face)->Delete(facesInOut);
	}

	// Dissolve edge
	edge.prev->next = twin.next;
	twin.next->prev = edge.prev;

	if(edge.tail->first == &edge)
		edge.tail->first = twin.next;

	if(edge.next != edge.twin)
	{
		// This is only done if the edge is not an inner dead end
		edge.next->prev = twin.prev;
		twin.prev->next = edge.next;

		if(edge.head->first == edge.twin)
			edge.head->first = edge.next;
	}
	else
		edge.head->first = 0;

	// Reassign face's first edge if it's going to be deleted
	if(face.first == &edge)
		face.first = edge.prev;
	else if(face.first == &twin) // Possible when removing inner edges
		face.first = twin.next;

	// Delete shared edge
	delete &twin;
	delete &edge;
	return true;
}

/*--------------------------------------
	com::MergeEdge

Merges edge.head into edge.tail and deletes edge and any connected face that degenerates into a
2-gon. true is returned if a merge occured. Hull must be closed.

Returns false if degeneration is detected.
--------------------------------------*/
bool com::MergeEdge(HalfEdge& edge, list<Face>& facesIO)
{
	HalfEdge& twin = *edge.twin;
	Face& face = *edge.face;
	Face& twinFace = *twin.face;

	// FIXME: Edge::Merge should check elements specifically related to the operation
	if(!face.Healthy() || !twinFace.Healthy())
		return false;

	// Move references from removed vertex to kept vertex
	Vertex &kp = *edge.tail, &rm = *edge.head;
	HalfEdge* it = rm.first;

	do
	{
		it->tail = &kp;
		it->prev->head = &kp;
		it = it->prev->twin;
	} while(it != rm.first);

	rm.first = 0;

	// Remove references to edge and twin
	edge.prev->next = edge.next;
	edge.next->prev = edge.prev;
	twin.prev->next = twin.next;
	twin.next->prev = twin.prev;

	if(face.first == &edge)
		face.first = edge.prev;

	if(twinFace.first == &twin)
		twinFace.first = twin.prev;

	// Delete edge
	delete &twin;
	delete &edge;

	// Delete resulting two-edge faces by dissolving any edge and keeping its twin's face
	if(face.first->next->next == face.first)
	{
		if(!DissolveEdge(*face.first->twin, facesIO))
			return false;
	}

	if(twinFace.first->next->next == twinFace.first)
	{
		if(!DissolveEdge(*twinFace.first->twin, facesIO))
			return false;
	}

	return true;
}

/*--------------------------------------
	com::EdgeHealth

If the edge needs no healing, returns HEALTHY (0). Returns PERPENDICULAR if FarthestEdgeVert
failed. Returns DISSOLVE if the edge is concave or separates faces that both "fit into" both
faces' planes with epsilon thickness.

FIXME: The latter case is annoying. It can later result in collinear triangles being created
	(BoundaryTriangles fixes this but it could be avoided by healing earlier) and in EdgeHealth
	false-negatives if FarthestEdgeVert is given an edge that is relatively perpendicular to its
	face's plane. It's apparent that a thin triangle with a large angle is involved if the faces
	only fit into one face's plane, in which case maybe one of them should be deleted by merging
	vertices. That won't detect two thin triangles with an unhealthy edge between them, however.
--------------------------------------*/
com::edge_health com::EdgeHealth(const HalfEdge& edge, fp epsilon)
{
	HalfEdge& twin = *edge.twin;

	Vertex* v = FarthestEdgeVert(twin);

	if(!v)
		return PERPENDICULAR;

	fp distToTwinFarVert = Dot(edge.face->pln.normal, *v) - edge.face->pln.offset;

	if(distToTwinFarVert > -epsilon) // Possibly concave or approximately coplanar
	{
		v = FarthestEdgeVert(edge);

		if(!v)
			return PERPENDICULAR;

		fp distToFarVert = Dot(twin.face->pln.normal, *v) - twin.face->pln.offset;

		if(distToFarVert > -epsilon && // Concave or approximately coplanar
		!( // NOT a sharp convex edge between two approximately coplanar faces
			Dot(edge.face->pln.normal, twin.face->pln.normal) < (fp)0.0 &&
			distToFarVert < (fp)0.0 &&
			distToTwinFarVert < (fp)0.0
		))
			return DISSOLVE;
	}

	return HEALTHY;
}

/*--------------------------------------
	com::FarthestEdgeVert

Returns vert on edge's face that is farthest from edge's infinite line projected onto the face's
plane. Assumes face is closed. May return 0.

FIXME: Edge's line direction can be unreliable; can this use some other geometric principle?
--------------------------------------*/
com::Vertex* com::FarthestEdgeVert(const HalfEdge& edge)
{
	fp mag = -COM_FP_MAX;
	Vertex* best = edge.head;
	int err = 0;
	Vec3 dir = Cross(edge.tail->pos - edge.head->pos, edge.face->pln.normal).Normalized(&err);

	if(err)
		return 0; // Edge is perpendicular to its face

	float offset = Dot(dir, edge.tail->pos);

	for(const HalfEdge* it = edge.next; it != edge.prev; it = it->next)
	{
		fp m = Dot(dir, it->head->pos) - offset;

		if(m > mag)
		{
			mag = m;
			best = it->head;
		}
	}

	return best;
}

/*--------------------------------------
	com::BestPlane

Returns ref to plane that has the smallest distance to all verts in faces sharing edge.

FIXME: Increase weight of vertex distance in front of plane?
--------------------------------------*/
const com::Plane& com::BestPlane(const HalfEdge* edge)
{
	Plane* plns[2] = {&edge->face->pln, &edge->twin->face->pln};
	const HalfEdge* edges[2] = {edge, edge->twin};
	fp dists[2] = {0.0, 0.0};

	for(size_t i = 0; i < 2; i++)
	{
		HalfEdge* it = edges[i]->face->first;
		do
		{
			for(size_t j = 0; j < 2; j++)
			{
				fp d = fabs(Dot(plns[j]->normal, *it->head) - plns[j]->offset);

				if(d > dists[j])
					dists[j] = d;
			}

			it = it->next;
		} while(it != edges[i]->face->first);
	}

	return *plns[dists[1] < dists[0]];
}

#if COM_CONVEX_SAVE_ERROR_STEPS
/*--------------------------------------
	com::SaveStart
--------------------------------------*/
void com::SaveStart(const Vec3* verts, size_t numVerts)
{
	com::OBJSaveBegin("convex_start.obj");

	for(size_t i = 0; i < numVerts; i++)
	{
		com::OBJSavePolyBegin();

		for(size_t j = 0; j < 3; j++)
			com::OBJSaveVert(verts[i]);

		com::OBJSavePolyEnd();
	}

	com::OBJSaveEnd();
	COM_OUTPUT("Wrote 'convex_start.obj'\n");
}

/*--------------------------------------
	com::SaveStep
--------------------------------------*/
void com::SaveStep(int step, const Vertex* verts, size_t numVerts, const list<Face>& faces)
{
	static char path[32];
	SNPrintF(path, sizeof(path), 0, "convex_step_%d.obj", step);
	OBJSaveBegin(path);

	for(const linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
	{
		const HalfEdge* itEdge = itFace->o->first;
		OBJSavePolyBegin();

		do
		{
			OBJSaveVert(itEdge->tail->pos);
			itEdge = itEdge->next;
		} while(itEdge != itFace->o->first);

		OBJSavePolyEnd();
	}

	OBJSaveEnd();
	COM_OUTPUT_F("Wrote '%s'\n", path);
}
#endif

/*
################################################################################################
	
	
	SEPARATING AXIS THEOREM DATA


################################################################################################
*/

/*--------------------------------------
	com::ConvexVertsAxes

Gets data for separating-axis-theorem collision detection from a list of faces forming a closed
convex hull.

verticesOut is set to a new[] array of unique vertices used by the faces.

axesOut is set to a new[] array of unique normal axes and unique edge axes,
numNormalAxesOut + numEdgeAxesOut in total. Normal axes are first.

Each output vertex will store addresses of adjacent vertices only if the total number of
vertices in the polyhedron is above simpleLimit.

FIXME: Make error strings
--------------------------------------*/
const char* com::ConvexVertsAxes(const list<Face>& faces, ClimbVertex*& verticesOut,
	size_t& numVerticesOut, Vec3*& axesOut, size_t& numNormalAxesOut, size_t& numEdgeAxesOut,
	size_t simpleLimit)
{
	// Count edges
	size_t numEdges = 0;

	for(linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
	{
		HalfEdge* itEdge = itFace->o->first;

		do
		{
			numEdges++;
		} while((itEdge = itEdge->next) != itFace->o->first);
	}

	const Vertex** usedVerts = new const Vertex*[numEdges];
	numVerticesOut = 0;

	// Find used vertices
	for(linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
	{
		HalfEdge* itEdge = itFace->o->first;

		do
		{
			usedVerts[numVerticesOut++] = itEdge->head;
		} while((itEdge = itEdge->next) != itFace->o->first);
	}

	RemoveDuplicates(usedVerts, numVerticesOut);

	verticesOut = new ClimbVertex[numVerticesOut];

	for(size_t i = 0; i < numVerticesOut; i++)
	{
		verticesOut[i].pos = *usedVerts[i];
		verticesOut[i].adjacents = 0;
		verticesOut[i].numAdjacents = 0;
		verticesOut[i].testCode = 0;
	}

	if(numVerticesOut > simpleLimit)
	{
		// Find adjacent vertices
		for(linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
		{
			HalfEdge* itEdge = itFace->o->first;

			do
			{
				size_t i = 0;
				for(; itEdge->head != usedVerts[i]; i++);

				if(verticesOut[i].adjacents)
					continue;

				verticesOut[i].numAdjacents = 0;
				HalfEdge* spinEdge = itEdge;

				do
				{
					verticesOut[i].numAdjacents++;
					spinEdge = spinEdge->twin->prev;
				} while(spinEdge != itEdge);

				verticesOut[i].adjacents = new ClimbVertex*[verticesOut[i].numAdjacents];
				size_t j = 0;

				do
				{
					size_t k = 0;
					for(; spinEdge->tail != usedVerts[k]; k++);

					verticesOut[i].adjacents[j++] = &verticesOut[k];

					spinEdge = spinEdge->twin->prev;
				} while(spinEdge != itEdge);
			} while((itEdge = itEdge->next) != itFace->o->first);
		}
	}

	delete[] usedVerts;

	// Find unique normal axes
	fp epsilon = Epsilon(verticesOut, numVerticesOut);
	Vec3* normals = new Vec3[Count(faces.l)];
	size_t numNormals = 0;

	for(linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
		normals[numNormals++] = itFace->o->pln.normal;

	RemoveDuplicateAxes(normals, numNormals, epsilon);

	// Find unique edge axes
	Vec3* edges = new Vec3[numEdges];
	numEdges = 0;

	for(linker<Face>* itFace = faces.f; itFace; itFace = itFace->next)
	{
		HalfEdge* itEdge = itFace->o->first;

		do
		{
			Vec3 v = itEdge->head->pos - itEdge->tail->pos;

			if(float mag = v.Mag())
				v /= mag;
			else
				continue;

			edges[numEdges++] = v;
		} while((itEdge = itEdge->next) != itFace->o->first);
	}

	RemoveDuplicateAxes(edges, numEdges, epsilon);

	numNormalAxesOut = numNormals;
	numEdgeAxesOut = numEdges;

	// Copy vertices and axes
	axesOut = new com::Vec3[numNormalAxesOut + numEdgeAxesOut];

	for(size_t i = 0; i < numNormalAxesOut; i++)
		axesOut[i] = normals[i];

	for(size_t i = 0; i < numEdgeAxesOut; i++)
		axesOut[numNormalAxesOut + i] = edges[i];

	delete[] normals;
	delete[] edges;

	return 0;
}

/*--------------------------------------
	com::ReadConvexData

See WriteConvexData for format. All output arguments are set to zero initially. Returns error
string on failure. All allocations are done with new[].

0 vertices is not considered a failure; the function immediately returns after reading
numVertices in that case.

Vertex adjacents are only parsed if numVertices is larger than simpleLimit.
--------------------------------------*/
#define READ_CONVEX_DATA_FAIL(err) \
	ReadConvexDataClose(verticesOut, numVerticesOut, axesOut, err)

const char* com::ReadConvexData(ClimbVertex*& verticesOut, size_t& numVerticesOut,
	Vec3*& axesOut, size_t& numNormalAxesOut, size_t& numEdgeAxesOut, size_t simpleLimit,
	FILE* file)
{
	verticesOut = 0;
	axesOut = 0;
	numVerticesOut = numNormalAxesOut = numEdgeAxesOut = 0;

	static const size_t NUM_VERTS_SIZE = sizeof(uint32_t);
	static const size_t NUM_AXES_SIZE = sizeof(uint32_t) * 2;

	unsigned char headerBytes[NUM_VERTS_SIZE + NUM_AXES_SIZE];

	if(fread(headerBytes, sizeof(unsigned char), NUM_VERTS_SIZE, file) != NUM_VERTS_SIZE)
		READ_CONVEX_DATA_FAIL("Could not read number of vertices");

	uint32_t u;

	com::MergeLE(headerBytes, u);
	numVerticesOut = u;

	if(!numVerticesOut)
		return 0; // Undefined hull; done

	if(fread(headerBytes, sizeof(unsigned char), NUM_AXES_SIZE, file) != NUM_AXES_SIZE)
		READ_CONVEX_DATA_FAIL("Could not read number of axes");

	com::MergeLE(headerBytes, u);
	numNormalAxesOut = u;

	com::MergeLE(headerBytes + sizeof(uint32_t), u);
	numEdgeAxesOut = u;

	if(!numNormalAxesOut)
		READ_CONVEX_DATA_FAIL("No normal axes");

	if(!numEdgeAxesOut)
		READ_CONVEX_DATA_FAIL("No edge axes");

	verticesOut = new com::ClimbVertex[numVerticesOut];

	for(size_t i = 0; i < numVerticesOut; i++)
		verticesOut[i].adjacents = 0; // Initialized in case of failure

	for(size_t i = 0; i < numVerticesOut; i++)
	{
		com::ClimbVertex& v = verticesOut[i];
		v.testCode = 0;

		static const size_t VERT_INIT_SIZE = sizeof(float) * 3 + sizeof(uint32_t);
		unsigned char initialBytes[VERT_INIT_SIZE];

		if(fread(initialBytes, sizeof(unsigned char), VERT_INIT_SIZE, file) != VERT_INIT_SIZE)
			READ_CONVEX_DATA_FAIL("Could not read initial vertex data");

		for(size_t j = 0; j < 3; j++)
		{
			float f;
			com::MergeLE(initialBytes + sizeof(float) * j, f);
			v.pos[j] = f;
		}

		com::MergeLE(initialBytes + sizeof(float) * 3, u);
		v.numAdjacents = u;

		if(numVerticesOut <= simpleLimit)
		{
			fseek(file, sizeof(uint32_t) * v.numAdjacents, SEEK_CUR);

			v.numAdjacents = 0;
			v.adjacents = 0;

			continue;
		}

		if(!v.numAdjacents)
			READ_CONVEX_DATA_FAIL("Vertex has no adjacents");

		v.adjacents = new com::ClimbVertex*[v.numAdjacents];

		for(size_t j = 0; j < v.numAdjacents; j++)
		{
			static const size_t INDEX_SIZE = sizeof(uint32_t);
			unsigned char indexBytes[INDEX_SIZE];

			if(fread(indexBytes, sizeof(unsigned char), INDEX_SIZE, file) != INDEX_SIZE)
				READ_CONVEX_DATA_FAIL("Could not read vertex adjacent index");

			com::MergeLE(indexBytes, u);

			if(u >= numVerticesOut)
				READ_CONVEX_DATA_FAIL("Vertex has out-of-bounds adjacent index");
			
			v.adjacents[j] = verticesOut + u;
		}
	}

	size_t numAxes = numNormalAxesOut + numEdgeAxesOut;
	axesOut = new com::Vec3[numAxes];

	for(size_t i = 0; i < numAxes; i++)
	{
		static const size_t AXIS_SIZE = sizeof(float) * 3;
		unsigned char axisBytes[AXIS_SIZE];

		if(fread(axisBytes, sizeof(unsigned char), AXIS_SIZE, file) != AXIS_SIZE)
			READ_CONVEX_DATA_FAIL("Could not read axis");

		for(size_t j = 0; j < 3; j++)
		{
			float f;
			com::MergeLE(axisBytes + sizeof(float) * j, f);
			axesOut[i][j] = f;
		}
	}

	return 0;
}

/*--------------------------------------
	com::ReadConvexDataClose
--------------------------------------*/
const char* com::ReadConvexDataClose(ClimbVertex*& verticesInOut, size_t numVertices,
	Vec3*& axesInOut, const char* err)
{
	if(verticesInOut)
	{
		for(size_t i = 0; i < numVertices && verticesInOut[i].adjacents; i++)
			delete[] verticesInOut[i].adjacents;

		delete[] verticesInOut;
	}

	if(axesInOut)
		delete[] axesInOut;

	return err;
}

#ifdef COM_CONVEX_DEBUGGING

/*
################################################################################################
	
	
	COM_CONVEX_DEBUGGING


################################################################################################
*/

/*--------------------------------------
	com::SaveFaceOBJ
--------------------------------------*/
void com::SaveFaceOBJ(const char* path, const Face* f)
{
	if(!f)
		return;

	OBJSaveBegin(path);
	OBJSavePolyBegin();
	HalfEdge* he = f->first;

	do
	{
		OBJSaveVert(he->tail->pos);
		he = he->next;
	} while(he != f->first);

	OBJSavePolyEnd();
	OBJSaveEnd();
}

/*--------------------------------------
	com::SaveFacesOBJ
--------------------------------------*/
void com::SaveFacesOBJ(const char* path, const linker<Face>* firstFace)
{
	if(!firstFace)
		return;

	OBJSaveBegin(path);

	for(const linker<Face>* it = firstFace; it; it = it->next)
	{
		OBJSavePolyBegin();
		HalfEdge* he = it->o->first;

		do
		{
			OBJSaveVert(he->tail->pos);
			he = he->next;
		} while(he != it->o->first);

		OBJSavePolyEnd();
	}

	OBJSaveEnd();
}

#endif