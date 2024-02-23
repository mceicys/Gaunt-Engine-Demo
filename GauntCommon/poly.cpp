// poly.cpp
// Martynas Ceicys

#include <varargs.h>

#include "poly.h"
#include "array.h"
#include "edge.h"
#include "math.h"
#include "vec.h"

/*--------------------------------------
	com::Poly::Poly
--------------------------------------*/
com::Poly::Poly(const Poly& p) : numVerts(p.numVerts), pln(p.pln)
{
	verts.EnsureExact(numVerts);
	com::Copy(verts.o, p.verts.o, numVerts);
}

com::Poly::Poly(const com::Face& f)
{
	*this = f;
}

/*--------------------------------------
	com::Poly::ClearVerts
--------------------------------------*/
void com::Poly::ClearVerts()
{
	verts.Free();
	numVerts = 0;
}

/*--------------------------------------
	com::Poly::SetVerts

If n is 0, deletes vertices. If v is 0, changes the number of vertices.
--------------------------------------*/
void com::Poly::SetVerts(const Vec3* v, size_t n)
{
	if(n)
	{
		numVerts = n;
		verts.EnsureExact(numVerts);

		if(v)
			com::Copy(verts.o, v, numVerts);
	}
	else
		ClearVerts();
}

/*--------------------------------------
	com::Poly::AddVerts
--------------------------------------*/
void com::Poly::AddVerts(const com::Vec3* v, size_t n)
{
	verts.EnsureExact(numVerts + n);

	if(v)
		com::Copy(verts.o + numVerts, v, n);

	numVerts += n;
}

/*--------------------------------------
	com::Poly::SetElements
--------------------------------------*/
void com::Poly::SetElements(const com::Vec3* v, const size_t* e, size_t ne)
{
	if(!ne)
	{
		ClearVerts();
		return;
	}

	numVerts = ne;
	verts.EnsureExact(numVerts);

	for(size_t i = 0; i < ne; i++)
		verts[i] = v[e[i]];
}

void com::Poly::SetElements(const com::Vec3* v, size_t ne, ...)
{
	if(!ne)
	{
		ClearVerts();
		return;
	}

	numVerts = ne;
	verts.EnsureExact(numVerts);

	va_list e;
	va_start(e, ne);
	for(size_t i = 0; i < ne; i++)
		verts[i] = v[va_arg(e, int)];
	va_end(e);
}

/*--------------------------------------
	com::Poly::Flip
--------------------------------------*/
void com::Poly::Flip()
{
	pln = -pln;

	if(!numVerts)
		return;

	Reverse(verts.o, numVerts);
}

/*--------------------------------------
	com::Poly::UpdatePlane
--------------------------------------*/
void com::Poly::UpdatePlane(bool clockwise, int* err)
{
	pln.normal = CalculateNormal(verts.o, numVerts, clockwise, err);
	pln.offset = com::Dot(verts[0], pln.normal);
}

/*--------------------------------------
	com::Poly::operator=
--------------------------------------*/
com::Poly& com::Poly::operator=(const Poly& p)
{
	SetVerts(p.verts.o, p.numVerts);
	pln = p.pln;
	return *this;
}

com::Poly& com::Poly::operator=(const Face& f)
{
	pln = f.pln;
	numVerts = 0;

	const com::HalfEdge* itEdge = f.first;
	do
	{
		numVerts++;
		itEdge = itEdge->next;
	} while(itEdge != f.first);

	verts.EnsureExact(numVerts);

	size_t i = 0;
	itEdge = f.first;
	do
	{
		verts[i++] = itEdge->tail->pos;
		itEdge = itEdge->next;
	} while(itEdge != f.first);

	return *this;
}

/*--------------------------------------
	com::Poly::InteriorProjection
--------------------------------------*/
bool com::Poly::InteriorProjection(const Vec3& pt, fp eps) const
{
	return com::InteriorProjection(verts.o, numVerts, pln.normal, pt, eps);
}

/*--------------------------------------
	com::TotalNumVerts
--------------------------------------*/
size_t com::TotalNumVerts(const Poly* polys, size_t numPolys)
{
	size_t numVerts = 0;
	for(size_t i = 0; i < numPolys; i++)
		numVerts += polys[i].numVerts;
	return numVerts;
}

/*--------------------------------------
	com::CutPoly

Returns 1 if poly is in front of pln, 0 if it's split, and -1 if it's behind. If 0 is returned,
back is set to the cut portion behind the plane. poly and back can point to the same address.
poly must have at least 3 verts.
--------------------------------------*/
int com::CutPoly(const Poly& poly, const Plane& pln, Poly& back)
{
	size_t fromFrnt, toBack, fromBack, toFrnt;
	float ftbTime, btfTime;
	float nextDist = PointPlaneDistance(poly.verts[0], pln);
	int nextSide = nextDist >= 0.0f ? 1 : -1;
	int firstSide = nextSide;
	size_t numBack = 0;
	unsigned numSplits = 0;

	for(size_t i = 0; i < poly.numVerts; i++)
	{
		float dist = nextDist;
		int side = nextSide;
		size_t j = (i + 1) % poly.numVerts;
		nextDist = PointPlaneDistance(poly.verts[j], pln);
		nextSide = nextDist >= 0.0f ? 1 : -1;

		if(side == -1)
			numBack++;

		if(side == nextSide)
			continue;

		numSplits++;

		if(side == 1)
		{
			fromFrnt = i;
			toBack = j;
			ftbTime = dist / (dist - nextDist);
		}
		else
		{
			fromBack = i;
			toFrnt = j;
			btfTime = dist / (dist - nextDist);
		}

		if(numSplits == 2)
		{
			if(nextSide == -1)
				numBack += poly.numVerts - i - 1;

			break;
		}
	}

	if(numSplits < 2)
	{
		if(numBack)
			return -1;
		else
			return 1;
	}

	bool addFTB = ftbTime != 1.0f;
	bool addBTF = btfTime != 0.0f;
	size_t numClip = numBack + addFTB + addBTF;
	size_t numOrig = poly.numVerts;

	if(numClip < 3)
		return 1; // Not enough vertices to create a back; poly is in front

	Vec3 ftb = COM_LERP(poly.verts[fromFrnt], poly.verts[toBack], ftbTime);
	Vec3 btf = COM_LERP(poly.verts[fromBack], poly.verts[toFrnt], btfTime);

	if(back.numVerts < numClip)
		back.AddVerts(0, numClip - back.numVerts);
	else
		back.numVerts = numClip;

	if(firstSide == 1 || &poly != &back)
	{
		back.pln = poly.pln;
		size_t index = 0;

		if(addFTB)
			back.verts[index++] = ftb;

		for(size_t i = toBack; i != toFrnt; i = (i + 1) % numOrig)
			back.verts[index++] = poly.verts[i];

		if(addBTF)
			back.verts[index++] = btf;
	}
	else
	{
		if(toBack)
			ShiftCopy(back.verts.o, toBack, numOrig, 2 - (int)(numOrig - numBack));

		size_t addIndex = toFrnt;

		if(addBTF)
			back.verts[addIndex++] = btf;

		if(addFTB)
			back.verts[addIndex++] = ftb;
	}

	return 0;
}