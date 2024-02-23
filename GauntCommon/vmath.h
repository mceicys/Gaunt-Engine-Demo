// vmath.h - Vector math
// Martynas Ceicys

#ifndef COM_VMATH_H
#define COM_VMATH_H

#include <stdint.h>

#include "array.h"
#include "math.h"
#include "qua.h"
#include "vec.h"

namespace com
{

/*======================================
	com::Plane
======================================*/
class Plane
{
public:
	Vec3	normal;
	fp		offset;

			Plane(const Vec3& n = (fp)0.0, fp o = (fp)0.0) : normal(n), offset(o) {}

	bool	operator==(const Plane& p) const {return normal == p.normal && offset == p.offset;}
	bool	operator!=(const Plane& p) const {return !(*this == p);}
	Plane	operator-() const {return Plane(-normal, -offset);}
	bool	Similar(const Plane& q, fp epsilon) const;
	int		SimilarSignless(const Plane& q, fp epsilon) const;
};

struct place
{
	com::Vec3 pos;
	com::Qua ori;
};

Vec3		CalculateNormal(const Vec3* verts, size_t numVerts, bool clockwise,
			int* errOut = 0);
Plane		PolygonPlane(const Vec3* v, size_t n, bool clockwise);
fp			CoplanarHeight(const Vec3& origin, const Vec2& slope, const Vec2& pos);
size_t		Quadrant(const Vec2& v);
size_t		Octant(const Vec3& v);
size_t		QuantizeVecPrimary(const Vec3& v, fp epsilon);
size_t		QuantizeVecSecondary(size_t p, const Vec3& v, fp epsilon);
size_t		QuantizeVecTertiary(size_t p, size_t s, const Vec3& v, fp epsilon);
Vec2		Rotate90(const Vec2& v, bool clockwise);
Vec3		Rotate90(const Vec3& v, bool clockwise);
Plane		EdgePlane(const Vec2& a, const Vec2& b, bool clockwise);
Plane		EdgePlane(const Vec3& a, const Vec3& b, const com::Vec3& surface, bool clockwise,
			int* errOut = 0);
Plane		PointPlane(const Vec3& n, const Vec3& pt);
fp			PointPlaneDistance(const Vec3& pt, const Plane& pln);
place		LerpedPlace(const place& a, const place& b, float f);
place		LerpedPlace(const place* places, size_t numPlaces, float frame);
place		PlaceDelta(const place& a, const place& b);
Vec3		LocalDelta(const Vec3& a, const Vec3& b, const place& spaceDelta);
bool		ClipLinePoly(const Vec3& a, const Vec3& b, const Vec3* verts, size_t numVerts,
			const Vec3& norm, bool clockwise, fp& tfOut, fp& tlOut);
Vec3		PlaneIntersectPoint(const Plane& p1, const Plane& p2, const Plane& p3,
			int* errOut = 0);
bool		PolygonBehindPlanes(const Vec3* v, size_t nv, const Plane* p, size_t np, fp eps);
bool		CutGeometry(const Vec3* v, size_t nv, const Plane& p, fp eps, Arr<Vec3>& frntOut,
			size_t& nfOut, Arr<Vec3>& backOut, size_t& nbOut);
bool		InteriorProjection(const Vec3* v, size_t nv, const Vec3& nrm, const Vec3& pt,
			fp eps);
int			RayTestPoly(const Vec3* v, size_t nv, const Vec3& nrm, const Vec3& pt,
			const Vec3& dir, fp eps, fp* distOut);

/*--------------------------------------
	com::TestLinePlane
--------------------------------------*/
fp TestLinePlane(const Vec3& lineA, const Vec3& lineB, const Vec3& normal, fp offset);
		
inline fp TestLinePlane(const Vec3& lineA, const Vec3& lineB, const Plane& pln)
{
	return TestLinePlane(lineA, lineB, pln.normal, pln.offset);
}

/*--------------------------------------
	com::Orthogonal
--------------------------------------*/
template <bool clockwise> Vec2 Orthogonal(const Vec2& u, const Vec2& v)
{
	if(clockwise)
		return com::Vec2(v.y - u.y, u.x - v.x);
	else
		return com::Vec2(u.y - v.y, v.x - u.x);
}

/*--------------------------------------
	com::ExpandVertBox
--------------------------------------*/
template <class vector3> void ExpandVertBox(const vector3& vert, Vec3& boxMinInOut,
	Vec3& boxMaxInOut)
{
	for(size_t i = 0; i < 3; i++)
	{
		if(Vec3(vert)[i] < boxMinInOut[i])
			boxMinInOut[i] = Vec3(vert)[i];

		if(Vec3(vert)[i] > boxMaxInOut[i])
			boxMaxInOut[i] = Vec3(vert)[i];
	}
}

/*--------------------------------------
	com::ExpandVertBox
--------------------------------------*/
template <class vector3> void ExpandVertBox(const vector3* verts, size_t numVerts,
	Vec3& boxMinInOut, Vec3& boxMaxInOut)
{
	for(size_t i = 0; i < numVerts; i++)
		ExpandVertBox(verts[i], boxMinInOut, boxMaxInOut);
}

/*--------------------------------------
	com::VertBox
--------------------------------------*/
template <class vector3> void VertBox(const vector3* verts, size_t numVerts,
	Vec3& boxMinOut, Vec3& boxMaxOut)
{
	boxMinOut = boxMaxOut = Vec3(verts[0]);
	ExpandVertBox(verts + 1, numVerts - 1, boxMinOut, boxMaxOut);
}

/*--------------------------------------
	com::BoxVerts

Vertices are in com::Octant order.
--------------------------------------*/
template <class vector3> void BoxVerts(const Vec3& boxMin, const Vec3& boxMax,
	vector3* vertsOut)
{
	(Vec3&)vertsOut[0] = boxMax;
	(Vec3&)vertsOut[1] = Vec3(boxMin.x, boxMax.y, boxMax.z);
	(Vec3&)vertsOut[2] = Vec3(boxMax.x, boxMin.y, boxMax.z);
	(Vec3&)vertsOut[3] = Vec3(boxMin.x, boxMin.y, boxMax.z);
	(Vec3&)vertsOut[4] = Vec3(boxMax.x, boxMax.y, boxMin.z);
	(Vec3&)vertsOut[5] = Vec3(boxMin.x, boxMax.y, boxMin.z);
	(Vec3&)vertsOut[6] = Vec3(boxMax.x, boxMin.y, boxMin.z);
	(Vec3&)vertsOut[7] = boxMin;
}

template <class vector3> void BoxVerts(const Vec3& boxMin, const Vec3& boxMax, const Vec3& x,
	const Vec3& y, const Vec3& z, vector3* vertsOut)
{
	Vec3 oBounds[6] =
	{
		x * boxMax.x,
		x * boxMin.x,
		y * boxMax.y,
		y * boxMin.y,
		z * boxMax.z,
		z * boxMin.z
	};

	(Vec3&)vertsOut[0] = oBounds[0] + oBounds[2] + oBounds[4];
	(Vec3&)vertsOut[1] = oBounds[1] + oBounds[2] + oBounds[4];
	(Vec3&)vertsOut[2] = oBounds[0] + oBounds[3] + oBounds[4];
	(Vec3&)vertsOut[3] = oBounds[1] + oBounds[3] + oBounds[4];
	(Vec3&)vertsOut[4] = oBounds[0] + oBounds[2] + oBounds[5];
	(Vec3&)vertsOut[5] = oBounds[1] + oBounds[2] + oBounds[5];
	(Vec3&)vertsOut[6] = oBounds[0] + oBounds[3] + oBounds[5];
	(Vec3&)vertsOut[7] = oBounds[1] + oBounds[3] + oBounds[5];
}

template <class vector3> void BoxVerts(const Vec3& boxMin, const Vec3& boxMax, const Qua& rot,
	vector3* vertsOut)
{
	Vec3 x = VecRot(Vec3((fp)1.0, (fp)0.0, (fp)0.0), rot).Normalized();
	Vec3 y = VecRot(Vec3((fp)0.0, (fp)1.0, (fp)0.0), rot).Normalized();
	Vec3 z = Cross(x, y);

	BoxVerts(boxMin, boxMax, x, y, z, vertsOut);
}

/*--------------------------------------
	com::Epsilon
--------------------------------------*/
fp Epsilon(const Vec2& u);
fp Epsilon(const Vec3& u);

template <class vector3> fp Epsilon(const vector3* vecs, size_t numVecs)
{
	fp maxAbs = (fp)0.0;

	for(size_t i = 0; i < numVecs; i++)
	{
		for(size_t j = 0; j < 3; j++)
			maxAbs = COM_MAX(maxAbs, fabs(Vec3(vecs[i])[j]));
	}

	return maxAbs * COM_FP_EPSILON;
}

/*--------------------------------------
	com::ProjectPointLine

Returns lerp factor of p projected onto line ab.
--------------------------------------*/
template <typename vector> fp ProjectPointLine(const vector& a, const vector& b, const vector& p)
{
	vector u = b - a;
	fp o = com::Dot(u, a);

	if(fp ob = com::Dot(u, b) - o)
		return (com::Dot(u, p) - o) / ob;
	else
		return (fp)0.0;
}

/*--------------------------------------
	com::ClosestSegmentFactor
--------------------------------------*/
template <typename vector> fp ClosestSegmentFactor(const vector& a, const vector& b,
	const vector& p)
{
	fp f = ProjectPointLine(a, b, p);

	if(f >= (fp)1.0)
		return (fp)1.0;
	else if(f <= (fp)0.0)
		return (fp)0.0;
	else
		return f;
}

/*--------------------------------------
	com::ClosestSegmentPoint

Returns closest point to p on line segment ab.
--------------------------------------*/
template <typename vector> vector ClosestSegmentPoint(const vector& a, const vector& b,
	const vector& p)
{
	return COM_LERP(a, b, ClosestSegmentFactor(a, b, p));
}

/*--------------------------------------
	com::RemoveDuplicateAxes

vecs should be unit vectors.
--------------------------------------*/
template <typename vector> void RemoveDuplicateAxes(vector* vecs, size_t& numInOut, fp epsilon)
{
	for(size_t i = 0; i < numInOut - 1; i++)
	{
		size_t j = i + 1;
		for(size_t k = j; k < numInOut; k++)
		{
			if((fp)1.0 - fabs(Dot(vecs[i], vecs[k])) >= epsilon)
				vecs[j++] = vecs[k];
		}

		numInOut = j;
	}
}

/*--------------------------------------
	com::Multiply4x4
--------------------------------------*/
template <typename t> Vec3 Multiply4x4(const Vec3& v, const t* m)
{
	Vec3 res;

	for(size_t r = 0; r < 3; r++)
	{
		size_t i = r * 4;
		res[r] = v[0] * m[i] + v[1] * m[i + 1] + v[2] * m[i + 2] + m[i + 3];
	}

	return res;
}

template <typename t> Vec3 Multiply4x4Homogeneous(const Vec3& v, const t (&m)[16])
{
	t res[4];

	for(size_t r = 0; r < 4; r++)
	{
		size_t i = r * 4;
		res[r] = v[0] * m[i] + v[1] * m[i + 1] + v[2] * m[i + 2] + m[i + 3];
	}

	for(size_t i = 0; i < 3; i++)
		res[i] /= res[3];

	return com::Vec3(res[0], res[1], res[2]);
}

/*--------------------------------------
	com::EdgePolyHit2D
--------------------------------------*/
template <typename vector> bool EdgePolyHit2D(vector a, vector b, const vector* verts,
	size_t numVerts)
{
	fp tf = -COM_FP_MAX, tl = COM_FP_MAX;

	for(size_t i = 0; i < numVerts; i++)
	{
		size_t j = (i + 1) % numVerts;
		Plane edgePln = EdgePlane(verts[i], verts[j], true);

		fp dists[2] = {
			PointPlaneDistance(a, edgePln),
			PointPlaneDistance(b, edgePln)
		};

		fp range = dists[0] - dists[1];

		if(dists[0] <= (fp)0.0)
		{
			if(dists[1] <= (fp)0.0 || range == (fp)0.0)
				continue;
			else
				tl = COM_MIN(tl, dists[0] / range);
		}
		else
		{
			if(dists[1] <= (fp)0.0)
			{
				if(range == (fp)0.0)
					continue;
				
				tf = COM_MAX(tf, dists[0] / range);
			}
			else
				return false;
		}

		if(tf > tl)
			return false;
	}

	return tf <= tl;
}

/*--------------------------------------
	com::FarthestEdgeVert

Returns index of vertex farthest from edge's infinite line. If distSqOut is not 0, sets it to
the squared distance from the line to the vertex.
--------------------------------------*/
template <typename vector> size_t FarthestEdgeVert(const vector* verts, size_t numVerts,
	size_t tail, fp* distSqOut = 0)
{
	size_t head = (tail + 1) % numVerts;
	fp mag = -COM_FP_MAX;
	size_t best = tail;

	for(size_t i = (head + 1) % numVerts; i != tail; i = (i + 1) % numVerts)
	{
		fp f = ProjectPointLine(verts[tail], verts[head], verts[i]);
		fp m = (verts[i] - COM_LERP(verts[tail], verts[head], f)).MagSq();

		if(m > mag)
		{
			mag = m;
			best = i;
		}
	}

	if(distSqOut)
		*distSqOut = mag;

	return best;
}

}

#endif