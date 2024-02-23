// vmath.cpp
// Martynas Ceicys

#include "vmath.h"

/*--------------------------------------
	com::Plane::Similar
--------------------------------------*/
bool com::Plane::Similar(const Plane& q, fp epsilon) const
{
	return (normal - q.normal).InEps(epsilon) && fabs(offset - q.offset) <= epsilon;
}

/*--------------------------------------
	com::Plane::SimilarSignless
--------------------------------------*/
int com::Plane::SimilarSignless(const Plane& q, fp epsilon) const
{
	if(this->Similar(q, epsilon))
		return 1;
	else if(this->Similar(-q, epsilon))
		return -1;

	return 0;
}

/*--------------------------------------
	com::TestLinePlane

Returns time of hit [0, 1].
Returns -1.0 if the line did not hit.
Returns -2.0 if the line is exactly coplanar.
--------------------------------------*/
com::fp com::TestLinePlane(const Vec3& lineA, const Vec3& lineB, const Vec3& normal, fp offset)
{
	fp projBToA = Dot(lineA - lineB, normal);

	if(projBToA == (fp)0.0)
		return (fp)-2.0;

	fp projA = Dot(lineA, normal) - offset;
	fp time = projA/projBToA;

	return time >= (fp)0.0 && time <= (fp)1.0 ? time : (fp)-1.0;
}

/*--------------------------------------
	com::CalculateNormal

Gets the cross product of a pair of edge vectors formed by verts (must form a convex polygon
overall). Clockwise specifies the front face vertex order.

If the normal is indeterminate, returns a zero vector and sets err to non-zero if given.
--------------------------------------*/
com::Vec3 com::CalculateNormal(const Vec3* verts, size_t numVerts, bool clockwise, int* err)
{
	if(numVerts < 3)
	{
		if(err)
			*err = 1;

		return (fp)0.0;
	}

	// Get decent line segment pair (cross product mag >= 1.0, or highest cross product mag)
	Vec3 normal;
	fp maxMagSq = (fp)0.0;
	for(size_t i = 0; i < numVerts; i++)
	{
		size_t j = (i + 1) % numVerts;
		size_t k = (i + 2) % numVerts;

		Vec3 v0 = verts[j] - verts[i];
		Vec3 v1 = verts[k] - verts[j];

		Vec3 tempNormal = clockwise ? Cross(v1, v0) : Cross(v0, v1);
		fp magSq = tempNormal.MagSq();

		if(magSq > maxMagSq)
		{
			maxMagSq = magSq;
			normal = tempNormal;

			if(magSq >= (fp)1.0)
				break;
		}
	}

	fp maxMag = sqrt(maxMagSq);

	if(maxMag == (fp)0.0)
	{
		if(err)
			*err = 1;

		return (fp)0.0;
	}

	normal /= maxMag;
	return normal;
}

/*--------------------------------------
	com::PolygonPlane
--------------------------------------*/
com::Plane com::PolygonPlane(const Vec3* v, size_t n, bool clockwise)
{
	Vec3 norm = CalculateNormal(v, n, clockwise);
	float offset = Dot(norm, v[0]);
	return Plane(norm, offset);
}

/*--------------------------------------
	com::CoplanarHeight

Returns z required for pos to be coplanar. origin is normal * offset. Slope is delta z over
delta x or y; calculated with (-normal.x / normal.z, -normal.y / normal.z).
--------------------------------------*/
com::fp com::CoplanarHeight(const Vec3& origin, const Vec2& slope, const Vec2& pos)
{
	return origin.z + slope.x * (pos.x - origin.x) + slope.y * (pos.y - origin.y);
}

/*--------------------------------------
	com::Quadrant
--------------------------------------*/
size_t com::Quadrant(const Vec2& v)
{
	return Quadrant(v.x, v.y);
}

/*--------------------------------------
	com::Octant
--------------------------------------*/
size_t com::Octant(const Vec3& v)
{
	return Octant(v.x, v.y, v.z);
}

/*--------------------------------------
	com::QuantizeVecPrimary

Returns discrete quantity of primary reference vector.

0-5: lines
	0-2: positive, axis order
	3-5: negative, axis order
6-17: quads
	6-9: yz (roll), start from y+ z+, quadrant order
	10-13: xz (pitch), start from x+ z+, quadrant order
	14-17: xy (yaw), start from x+ y+, quadrant order
18-25: octs
	Start from x+ y+ z+, octant order
--------------------------------------*/
size_t com::QuantizeVecPrimary(const Vec3& v, fp epsilon)
{
	for(size_t i = 0; i < 3; i++)
	{
		if(v[i] >= (fp)1.0 - epsilon)
			return i;
		else if(v[i] <= epsilon - (fp)1.0)
			return i + 3;
	}

	for(size_t i = 0; i < 3; i++)
	{
		if(fabs(v[i]) <= epsilon)
			return Quadrant(v[SecAxis(i)], v[TerAxis(i)]) + i * 4 + 6;
	}

	return Octant(v) + 18;
}

/*--------------------------------------
	com::QuantizeVecSecondary

Returns discrete quantity of reference vector based on primary vector's discrete quantity.

A different quantization for the secondary vector is used to reduce the size of lookup tables
and to logically prevent theoretically impossible outputs (e.g. forward and up vectors pointing
at the same octant) that might arise from floating point errors otherwise.

If p is line (0-5)
	0-3: lines
		0-1: positive, subordinate axis order
		2-3: negative, subordinate axis order
	4-7: quads
		Start from sec+ ter+, qaudrant order

If p is quad (6-17)
	axis perpendicular to quad (primary axis) = (p - 6) / 4
	0-1: lines (perpendicular axis)
		0: positive
		1: negative
	2-3: quads
		2: flipped on secondary axis
		3: flipped on tertiary axis
	4-7: octs
		4: flip pQuad's secondary axis, make perpendicular positive
		5: flip secondary axis, perpendicular negative
		6: flip tertiary axis, perpendicular postive
		7: flip tertiary axis, perpendicular negative

If p is oct (18-25)
	0-5: quads
		0: yz, flip pOct's y
		1: yz, flip pOct's z
		2: xz, flip pOct's x
		3: xz, flip pOct's z
		4: xy, flip pOct's x
		5: xy, flip pOct's y
	6-11: octs
		XOR pOct (which is p - 18) with (s - 5) to get sOct
		6: flip pOct's x
		7: flip pOct's y
		8: flip pOct's x and y
		9: flip pOct's z
		10: flip pOct's x and z
		11: flip pOct's y and z
--------------------------------------*/
size_t com::QuantizeVecSecondary(size_t p, const Vec3& v, fp epsilon)
{
	if(p < 6) // Line
	{
		size_t pLineAxis = p % 3;
		size_t subAxes[] = {SecAxis(pLineAxis), TerAxis(pLineAxis)};

		for(size_t i = 0; i < 2; i++)
		{
			size_t c = subAxes[i];

			if(v[c] >= (fp)1.0 - epsilon)
				return i;
			else if(v[c] <= epsilon - (fp)1.0)
				return i + 2;
		}

		return Quadrant(v[subAxes[0]], v[subAxes[1]]) + 4;
	}
	else if(p < 18) // Quadrant
	{
		size_t pPerpAxis = (p - 6) / 4;

		if(v[pPerpAxis] >= (fp)1.0 - epsilon)
			return 0;
		else if(v[pPerpAxis] <= epsilon - (fp)1.0)
			return 1;
		
		size_t pQuad = (p - 6) % 4;
		bool sameSecAxisSign = (v[SecAxis(pPerpAxis)] < (fp)0.0) == (pQuad & 1);

		if(fabs(v[pPerpAxis]) <= epsilon)
		{
			// Secondary vector is in a quad
			if(sameSecAxisSign)
				return 3; // sQuad sign differs from pQuad on tertiary axis
			else
				return 2; // sQuad sign differs from pQuad on secondary axis
		}

		// Secondary vector is in an octant
		size_t o = 0;

		if(v[pPerpAxis] < (fp)0.0)
			o ^= 1; // Quad's perpendicular axis is negative in octant

		if(sameSecAxisSign)
			o ^= 2; // Octant and quad sign differs on tertiary axis rather than secondary

		return o + 4;
	}
	else // Octant
	{
		size_t pOct = p - 18;

		for(size_t i = 0; i < 3; i++)
		{
			if(fabs(v[i]) > epsilon)
				continue;

			size_t q = i * 2;
			size_t sec = SecAxis(i);

			if((v[sec] >= (fp)0.0) == ((pOct & (sec + 1)) != 0))
				return q;
			else
				return q + 1;
		}

		size_t o = 0;

		for(size_t i = 0; i < 3; i++)
		{
			if((v[i] >= (fp)0.0) == ((pOct & (1 << i)) != 0))
				o ^= 1 << i; // sOct sign differs from pOct on axis i

			if(o >= 3)
				break; // o's bits have been flipped twice
		}

		if(o == 0)
		{
			// Never flipped due to precision errors; flip on closest axis
			size_t minComp = 0;
			fp absMin = fabs(v[0]);

			for(size_t i = 1; i < 3; i++)
			{
				fp a = fabs(v[i]);

				if(a < absMin)
				{
					minComp = i;
					absMin = a;
				}
			}

			o ^= 1 << minComp;
		}

		return o + 5;
	}
}

/*--------------------------------------
	com::QuantizeVecTertiary

Returns discrete quantity of reference vector based on primary and secondary vectors' discrete
quantities. This quantity is only relevant if the primary and secondary are pointing into
octants.

comp = the single component of which p's and s's signs are uniquely equal or uniquely different
	i.e. if x signs differ while y and z signs are equal, comp is x
0: comp of vector is zero
1: comp of vector is positive
2: comp of vector is negative
--------------------------------------*/
size_t com::QuantizeVecTertiary(size_t p, size_t s, const Vec3& v, fp epsilon)
{
	size_t pOct = p - 18;
	size_t sOct = pOct ^ (s - 5);

	size_t comp = pOct ^ sOct;

	if(comp & (comp - 1))
		comp = comp ^ 7; // Vectors have one sign equality rather than one sign difference

	comp >>= 1; // Convert to axis index

	if(v[comp] > epsilon)
		return 1;
	else if(v[comp] < -epsilon)
		return 2;
	else
		return 0;
}

/*--------------------------------------
	com::Rotate90
--------------------------------------*/
com::Vec2 com::Rotate90(const Vec2& v, bool clockwise)
{
	if(clockwise)
		return Vec2(v.y, -v.x);
	else
		return Vec2(-v.y, v.x);
}

com::Vec3 com::Rotate90(const Vec3& v, bool clockwise)
{
	if(clockwise)
		return Vec3(v.y, -v.x, v.z);
	else
		return Vec3(-v.y, v.x, v.z);
}

/*--------------------------------------
	com::EdgePlane

Faces outward.
--------------------------------------*/
com::Plane com::EdgePlane(const Vec2& a, const Vec2& b, bool clockwise)
{
	Plane pln;
	pln.normal = Rotate90(b - a, clockwise);
	pln.normal.z = (fp)0.0;
	pln.normal = pln.normal.Normalized();
	pln.offset = Dot(pln.normal, Vec3(a.x, a.y, 0.0));
	return pln;
}

com::Plane com::EdgePlane(const Vec3& a, const Vec3& b, const com::Vec3& surface,
	bool clockwise, int* errOut)
{
	Plane pln;
	pln.normal = com::Cross(clockwise ? (a - b) : (b - a), surface).Normalized(errOut);
	pln.offset = Dot(pln.normal, a);
	return pln;
}

/*--------------------------------------
	com::PointPlane
--------------------------------------*/
com::Plane com::PointPlane(const Vec3& n, const Vec3& pt)
{
	return Plane(n, Dot(n, pt));
}

/*--------------------------------------
	com::PointPlaneDistance
--------------------------------------*/
com::fp com::PointPlaneDistance(const Vec3& pt, const Plane& pln)
{
	return Dot(pt, pln.normal) - pln.offset;
}

/*--------------------------------------
	com::LerpedPlace
--------------------------------------*/
com::place com::LerpedPlace(const place& a, const place& b, float f)
{
	place p = {
		COM_LERP(a.pos, b.pos, f),
		com::Slerp(a.ori, b.ori, f).Normalized()
	};

	return p;
}

com::place com::LerpedPlace(const place* places, size_t numPlaces, float frame)
{
	place p1 = {com::Vec3(0.0f), com::QUA_IDENTITY};
	place p2 = {com::Vec3(0.0f), com::QUA_IDENTITY};

	int a = (int)floor(frame);
	int b = (int)ceil(frame);
	float f = frame - (float)a;

	if((size_t)a < numPlaces)
		p1 = places[a];

	if(a == b)
		return p1;

	if((size_t)b < numPlaces)
		p2 = places[b];

	p1.pos = COM_LERP(p1.pos, p2.pos, f);
	p1.ori = com::Slerp(p1.ori, p2.ori, f).Normalized();
	return p1;
}

/*--------------------------------------
	com::PlaceDelta
--------------------------------------*/
com::place com::PlaceDelta(const place& a, const place& b)
{
	place delta = {
		com::VecRotInv(b.pos - a.pos, a.ori),
		com::Delta(a.ori, b.ori)
	};

	return delta;
}

/*--------------------------------------
	com::LocalDelta

a and b are in their own local spaces which differ by spaceDelta. Returns vector from a to b in
the former's local space.
--------------------------------------*/
com::Vec3 com::LocalDelta(const Vec3& a, const Vec3& b, const place& spaceDelta)
{
	com::Vec3 localB = com::VecRot(b, spaceDelta.ori) + spaceDelta.pos;
	return localB - a;
}

/*--------------------------------------
	com::ClipLinePoly

Line ab is projected onto the polygon. tfOut is set to time (-inf, inf) the line enters the
polygon, and tlOut to time before it exits. Returns false if the line is outside the polygon.
Even if false is returned, tf and tl form a valid clip of the infinite line if tf <= tl.
--------------------------------------*/
bool com::ClipLinePoly(const Vec3& a, const Vec3& b, const Vec3* verts, size_t numVerts,
	const Vec3& norm, bool clockwise, fp& tfOut, fp& tlOut)
{
	tfOut = -COM_FP_MAX;
	tlOut = COM_FP_MAX;
	size_t skipped = 0;
	bool outside = false;

	for(size_t i = 0; i < numVerts; i++)
	{
		size_t j = (i + 1) % numVerts;
		int err = 0;
		com::Plane pln = EdgePlane(verts[i], verts[j], norm, clockwise, &err);

		if(err)
		{
			skipped++;
			continue;
		}

		fp dists[2] = {
			com::Dot(pln.normal, a) - pln.offset,
			com::Dot(pln.normal, b) - pln.offset
		};

		fp total = dists[0] - dists[1];

		if(!total)
		{
			if(dists[0] > (fp)0.0)
				outside = true;

			skipped++;
			continue;
		}

		fp t = dists[0] / total;

		if(total > (fp)0.0)
		{
			if(t > tfOut)
				tfOut = t;
		}
		else
		{
			if(t < tlOut)
				tlOut = t;
		}
	}

	if(skipped == numVerts)
		return !outside;
	else
		return tfOut <= tlOut && tfOut <= (fp)1.0 && tlOut >= (fp)0.0 && !outside;
}

/*--------------------------------------
	com::PlaneIntersectPoint
--------------------------------------*/
com::Vec3 com::PlaneIntersectPoint(const Plane& p1, const Plane& p2, const Plane& p3,
	int* errOut)
{
	const Vec3 &n1 = p1.normal, &n2 = p2.normal, &n3 = p3.normal;

	#define DET33(a1, b1, c1, a2, b2, c2, a3, b3, c3) \
		((a1) * ((b2) * (c3) - (c2) * (b3)) - \
		(b1) * ((a2) * (c3) - (c2) * (a3)) + \
		(c1) * ((a2) * (b3) - (b2) * (a3)))

	fp det = DET33(n1.x, n1.y, n1.z, n2.x, n2.y, n2.z, n3.x, n3.y, n3.z);

	if(!det)
	{
		*errOut = 1;
		return (fp)0.0;
	}

	fp d1 = p1.offset, d2 = p2.offset, d3 = p3.offset;
	fp xp = DET33(d1, n1.y, n1.z, d2, n2.y, n2.z, d3, n3.y, n3.z);
	fp yp = DET33(n1.x, d1, n1.z, n2.x, d2, n2.z, n3.x, d3, n3.z);
	fp zp = DET33(n1.x, n1.y, d1, n2.x, n2.y, d2, n3.x, n3.y, d3);

	return Vec3(xp / det, yp / det, zp / det);
}

/*--------------------------------------
	com::PolygonBehindPlanes

Returns false if polygon is entirely in front of at least one plane. Positive eps means true is
more likely.
--------------------------------------*/
bool com::PolygonBehindPlanes(const Vec3* v, size_t nv, const Plane* p, size_t np, fp eps)
{
	for(size_t ip = 0; ip < np; ip++)
	{
		size_t iv = 0;
		for(; iv < nv; iv++)
		{
			if(PointPlaneDistance(v[iv], p[ip]) < eps)
				break;
		}

		if(iv == nv)
			return false;
	}

	return true;
}

/*--------------------------------------
	com::CutGeometry

Cuts a line segment or convex polygon formed by verts v with plane p. frnt and back store the
split-section verts, and nf and nb are set to the number of verts respectively.

Returns true if the geometry was split into two sections of the same type of geometry (polygon-
to-polygons, line-to-lines).
--------------------------------------*/
bool com::CutGeometry(const Vec3* v, size_t nv, const Plane& p, fp eps, Arr<Vec3>& frnt,
	size_t& nf, Arr<Vec3>& back, size_t& nb)
{
	nf = 0, nb = 0;

	// Split
	if(nv >= 3)
	{
		fp nextDist = Dot(v[0], p.normal) - p.offset;
		int nextSide = nextDist >= (fp)0.0 ? 1 : -1;

		for(size_t i = 0; i < nv; i++)
		{
			frnt.Ensure(nf + 2);
			back.Ensure(nb + 2);
			fp dist = nextDist;
			int side = nextSide;

			if(side == 1)
				frnt[nf++] = v[i];
			else
				back[nb++] = v[i];

			size_t j = (i + 1) % nv;
			nextDist = com::Dot(v[j], p.normal) - p.offset;
			nextSide = nextDist >= 0.0 ? 1 : -1;

			if(side == nextSide)
				continue;

			// Crossed the splitting plane
			if(abs(dist) <= eps)
			{
				// Start vertex is coplanar, duplicate it on the other side
				if(nextSide == 1)
					frnt[nf++] = v[i];
				else
					back[nb++] = v[i];
			}
			else if(abs(nextDist) <= eps)
			{
				// End vertex is coplanar, duplicate it on both sides
				frnt[nf++] = v[j];
				back[nb++] = v[j];
			}
			else
			{
				// No involved vertices are on the plane, make a new vertex for both sides
				fp t = dist/(dist - nextDist);
				t = COM_MIN(COM_MAX(t, (fp)0.0), (fp)1.0); // Just in case
				frnt[nf++] = COM_LERP(v[i], v[j], t);
				back[nb++] = COM_LERP(v[i], v[j], t);
			}
		}

		// Fix duplicates since vertices are copied liberally
		RemoveDuplicates(frnt.o, nf);
		RemoveDuplicates(back.o, nb);

		// Don't output non-polygons
		if(nf < 3)
			nf = 0;

		if(nb < 3)
			nb = 0;
	}
	else if(nv == 2)
	{
		fp t = TestLinePlane(v[0], v[1], p);

		if(t < (fp)0.0)
			return false;

		nf = nb = 2;
		size_t fi = Dot(v[1], p.normal) - p.offset >= 0.0;
		Vec3 midVert = COM_LERP(v[0], v[1], t);
		frnt[0] = midVert;
		frnt[1] = v[fi];
		back[0] = v[!fi];
		back[1] = midVert;
	}

	return nf && nb;
}

/*--------------------------------------
	com::InteriorProjection

Returns true if pt projected onto polygon's plane is inside the polygon. Positive epsilon is
more lenient, returning true for more points.
--------------------------------------*/
bool com::InteriorProjection(const Vec3* v, size_t nv, const Vec3& nrm, const Vec3& pt, fp eps)
{
	for(size_t i = 0; i < nv; i++)
	{
		size_t j = (i + 1) % nv;
		int err = 0;
		com::Plane inPln = com::EdgePlane(v[i], v[j], nrm, false, &err);

		if(err)
			continue;

		if(com::Dot(inPln.normal, pt) - inPln.offset > eps)
			return false;
	}

	return true;
}

/*--------------------------------------
	com::RayTestPoly

Returns 0 if the polygon was not hit, 1 if the front was hit, -1 if the back was hit, and 2 if
the ray brushes the polygon. If distOut is given, it's set to how far the ray travelled. Polygon
must be convex and coplanar.
--------------------------------------*/
int com::RayTestPoly(const Vec3* v, size_t nv, const Vec3& nrm, const Vec3& pt, const Vec3& dir,
	fp eps, fp* distOut)
{
	if(distOut)
		*distOut = COM_FP_MAX;

	fp div = Dot(nrm, dir);
	fp offset = Dot(nrm, pt) - Dot(nrm, v[0]);

	if(!div)
	{
		// Ray is parallel
		if(offset)
			return 0; // pt is not coplanar, no hit

		// Ray is coplanar, check if it brushes polygon
		fp tf, tl;
		ClipLinePoly(pt, pt + dir, v, nv, nrm, false, tf, tl);

		if(tf >= (fp)0.0 && tf <= tl)
		{
			if(distOut)
				*distOut = tf;

			return 2;
		}

		return 0;
	}
	else if(div > (fp)0.0)
	{
		// Ray going in normal's direction
		if(offset > (fp)0.0)
			return 0; // Ray starts in front of polygon, no hit
	}
	else
	{
		// Ray going opposite of normal
		if(offset < (fp)0.0)
			return 0; // Ray starts behind polygon, no hit
	}

	// Ray hits plane
	fp dist = offset / -div;

	if(!com::InteriorProjection(v, nv, nrm, pt + dir * dist, eps))
		return 0;

	if(distOut)
		*distOut = dist;

	return div <= (fp)0.0 ? 1 : -1;
}

/*--------------------------------------
	com::Epsilon
--------------------------------------*/
com::fp com::Epsilon(const Vec2& u)
{
	return COM_MAX(fabs(u.x), fabs(u.y)) * COM_FP_EPSILON;
}

com::fp com::Epsilon(const Vec3& u)
{
	return COM_MAX(COM_MAX(fabs(u.x), fabs(u.y)), fabs(u.z)) * COM_FP_EPSILON;
}