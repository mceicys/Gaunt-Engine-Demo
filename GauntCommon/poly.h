// poly.h
// Martynas Ceicys

#ifndef COM_POLY_H
#define COM_POLY_H

#include "array.h"
#include "vmath.h"

namespace com
{

class Face;

/*======================================
	com::Poly
======================================*/
class Poly
{
public:
	Arr<Vec3>	verts;
	size_t		numVerts;
	Plane		pln;

	Poly() : numVerts(0) {}
	Poly(const Poly& p);
	Poly(const Face& f);
	~Poly() {verts.Free();}
	void ClearVerts();
	void SetVerts(const Vec3* v, size_t n);
	void AddVerts(const Vec3* v, size_t n);
	void SetElements(const Vec3* v, const size_t* e, size_t ne);
	void SetElements(const Vec3* v, size_t ne, ...);
	void Flip();
	void UpdatePlane(bool clockwise, int* errOut = 0);
	Poly& operator=(const Poly& p);
	Poly& operator=(const Face& f);
	bool InteriorProjection(const Vec3& pt, fp eps) const;
};

size_t TotalNumVerts(const Poly* polys, size_t numPolys);
int CutPoly(const Poly& poly, const Plane& pln, Poly& backIO);

}

#endif