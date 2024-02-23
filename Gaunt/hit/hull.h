// hull.h
// Martynas Ceicys

#ifndef HULL_H
#define HULL_H

#include "../../GauntCommon/convex.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/vec.h"
#include "../resource/resource.h"

#define HIT_SIMPLE_VERT_LIMIT 9

namespace hit
{

enum
{
	BOX,
	CONVEX, // All subsequent types are also convex
	FRUSTUM
};

/*======================================
	hit::Hull
======================================*/
class Hull : public res::Resource<Hull>
{
public:
						Hull(const com::Vec3& boxMin, const com::Vec3& boxMax) : name(0),
						type(BOX), boxMin(boxMin), boxMax(boxMax), res::Resource<Hull>(0) {}

						// Managed hull constructor
						Hull(const char* name, const com::Vec3& boxMin, const com::Vec3& boxMax,
						unsigned numLocks) : name(com::NewStringCopy(name)), type(BOX),
						boxMin(boxMin), boxMax(boxMax), res::Resource<Hull>(numLocks) {}

						~Hull() {if(name) delete[] name;}

	const com::Vec3&	Min() const {return boxMin;}
	const com::Vec3&	Max() const {return boxMax;}
	const char*			Name() const {return name;}
	char				Type() const {return type;}
	void				BoxSpan(const com::Vec3& axis, float& minOut, float& maxOut) const;
	void				SetBox(const com::Vec3& min, const com::Vec3& max);
	com::Vec3			BoxAverage() const {return (boxMin + boxMax) * 0.5f;}
	void				DrawBoxWire(const com::Vec3& p, const com::Qua& o, int color,
						float time = 0.0f) const;

protected:
	com::Vec3			boxMin, boxMax;
	const char*			name;
	char				type;

						Hull() {}
						Hull(const Hull&) {}

						Hull(char type, const com::Vec3& boxMin, const com::Vec3& boxMax)
						: name(0), type(type), boxMin(boxMin), boxMax(boxMax),
						res::Resource<Hull>(0) {}

						Hull(const char* name, char type, const com::Vec3& boxMin,
						const com::Vec3& boxMax, unsigned numLocks)
						: name(com::NewStringCopy(name)), type(type), boxMin(boxMin),
						boxMax(boxMax), res::Resource<Hull>(numLocks) {}
};

/*======================================
	hit::Convex

FIXME: Arrange axes to find negatives faster
	Early variety by finding the next most different axis
	Prefer something closer to xy for the first two? Assuming game is usually doing ~2D checks
======================================*/
class Convex : public Hull
{
public:
							Convex(const com::Vec3* verts, size_t numVerts);

							Convex(const char* name, com::ClimbVertex* vertices,
							size_t numVertices, com::Vec3* axes, size_t numNormalAxes,
							size_t numEdgeAxes, unsigned numLocks);

							~Convex();

	void					Span(const com::Vec3& axis, float& minOut, float& maxOut) const;
	size_t					FarVertices(const com::Vec3& axis,
							com::Arr<com::ClimbVertex*>& farOut) const; // FIXME: don't need this, can remove
	const com::ClimbVertex*	Vertices() const {return vertices;}
	const com::Vec3*		Axes() const {return axes;}
	size_t					NumVertices() const {return numVertices;}
	size_t					NumNormalAxes() const {return numNormalAxes;}
	size_t					NumEdgeAxes() const {return numEdgeAxes;}
	const float*			NormalSpans() const {return normalSpans;}
	com::Vec3				Average() const;
	void					DrawWire(const com::Vec3& p, const com::Qua& o, int color,
							float time = 0.0f) const;

protected:
	com::ClimbVertex*		vertices;
	com::Vec3*				axes;
	size_t					numVertices, numNormalAxes, numEdgeAxes;
	float*					normalSpans; // numNormalAxes * 2, min max
	mutable size_t			markCode;

							Convex(char type) : Hull(type, 0.0f, 0.0f), markCode(0) {}
							Convex(const Convex&);

	void					CreateNormalSpans();
	void					CalcNormalSpans();
	void					DefaultHull();
	size_t					IncMarkCode() const;
};

/*======================================
	hit::Frustum
======================================*/
class Frustum : public Convex
{
public:
				Frustum(float horiAng = 0.0f, float vertAng = 0.0f, float nearDist = 0.0f,
				float farDist = 1.0f);
	float		HoriAng() const {return ha;}
	float		VertAng() const {return va;}
	void		SetAngles(float h, float v);
	float		NearDist() const {return nd;}
	void		SetNearDist(float f);
	float		FarDist() const {return fd;}
	void		SetFarDist(float f);
	void		Set(float horiAng, float vertAng, float nearDist, float farDist);
	com::Vec3	MinSphere(float& radiusOut, float n, float f) const;
	com::Vec3	MinSphere(float& radiusOut) const;

	static Frustum* CheckLuaToFrustum(int index);

private:
	float		ha, va, nd, fd;

				Frustum(const Frustum&);
	void		CalcFrustum();
};

}

#endif