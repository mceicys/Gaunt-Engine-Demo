// hit_hull_test.cpp
// Martynas Ceicys

#include "hit.h"
#include "hit_lua.h"
#include "hit_private.h"
#include "../quaternion/qua_lua.h"
#include "../vector/vec_lua.h"

namespace hit
{
	// HULL TEST
	bool	TestAABBAABB(const Hull& hullA, const com::Vec3& vel, const Hull& hullB,
			const com::Vec3& posB, Result& resInOut);
	bool	TestAABBOBB(const Hull& hullA, const com::Vec3& vel, const Hull& hullB,
			const com::Vec3& pos, const com::Qua& ori, Result& resInOut,
			const com::Qua* wldOri = 0);
	bool	TestAABBConvex(const Hull& hullA, const com::Vec3& vel, const Convex& hullB,
			const com::Vec3& pos, Result& resInOut);
	bool	TestOBBConvex(const Hull& hullA, const com::Vec3& vel, const com::Qua& ori,
			const Convex& hullB, const com::Vec3& pos, Result& resInOut,
			const com::Qua* wldOri = 0);
	bool	TestConvexConvex(const Convex& hullA, const com::Vec3& vel, const Convex& hullB,
			const com::Vec3& pos, const com::Qua& ori, Result& resInOut,
			const com::Qua* wldOri = 0);
	bool	TestPointSpan(float pVel, float min, float max, float spanPos,
			const com::Vec3& axis, const com::Vec3& vel, const Result& initRes,
			Result& resInOut, const com::Qua* wldOri = 0);
}

/*
################################################################################################


	HULL TEST


################################################################################################
*/

static const com::Vec3 BOX_AXES[3] =
{
	com::Vec3(1.0f, 0.0f, 0.0f),
	com::Vec3(0.0f, 1.0f, 0.0f),
	com::Vec3(0.0f, 0.0f, 1.0f)
};

/*--------------------------------------
	hit::TestLineHull
--------------------------------------*/
bool hit::TestLineHull(const com::Vec3& lineA, const com::Vec3& lineB, const Hull& h,
	const com::Vec3& hullPos, const com::Qua& hullOri, Result& resInOut)
{
	static Hull point(0.0f, 0.0f);
	return TestHullHull(point, lineA, lineB, com::QUA_IDENTITY, h, hullPos, hullOri, resInOut);
}

hit::Result hit::TestLineHull(const com::Vec3& lineA, const com::Vec3& lineB, const Hull& h,
	const com::Vec3& hullPos, const com::Qua& hullOri)
{
	Result res = {Result::NONE, FLT_MAX, -FLT_MAX};
	TestLineHull(lineA, lineB, h, hullPos, hullOri, res);
	return res;
}

/*--------------------------------------
	hit::TestHullHull

timeFirst and timeLast are valid upon contact, even if an intersection occured. If a hit occurs
(before the one in resInOut), returns true and changes resInOut.
--------------------------------------*/
bool hit::TestHullHull(const Hull& hullA, const com::Vec3& oldPosA, const com::Vec3& posA,
	const com::Qua& oriA, const Hull& hullB, const com::Vec3& posB, const com::Qua& oriB,
	Result& resInOut)
{
	if(hullA.Type() == BOX)
	{
		if(hullB.Type() == BOX)
		{
			if(oriA.Identity())
			{
				if(oriB.Identity())
					return TestAABBAABB(hullA, posA - oldPosA, hullB, posB - oldPosA, resInOut);
				else
					return TestAABBOBB(hullA, posA - oldPosA, hullB, posB - oldPosA, oriB,
					resInOut);
			}
			else if(oriB.Identity())
			{
				if(TestAABBOBB(hullB, oldPosA - posA, hullA, oldPosA - posB, oriA, resInOut))
				{
					if(resInOut.contact == Result::HIT)
						resInOut.normal = -resInOut.normal;
					else
						resInOut.mtvDir = -resInOut.mtvDir;

					return true;
				}
			}
			else
			{
				return TestAABBOBB(hullA,
					com::VecQua(oriA.Conjugate() * (posA - oldPosA) * oriA), hullB,
					com::VecQua(oriA.Conjugate() * (posB - oldPosA) * oriA),
					oriA.Conjugate() * oriB, resInOut, &oriA);
			}
		}
		else // A Box, B Convex
		{
			if(oriA.Identity())
			{
				if(oriB.Identity())
					return TestAABBConvex(hullA, posA - oldPosA, (Convex&)hullB, posB - oldPosA,
					resInOut);
				else
					return TestOBBConvex(hullA,
					com::VecQua(oriB.Conjugate() * (posA - oldPosA) * oriB),
					oriB.Conjugate(), (Convex&)hullB,
					com::VecQua(oriB.Conjugate() * (posB - oldPosA) * oriB), resInOut, &oriB);
			}
			else if(oriB.Identity())
				return TestOBBConvex(hullA, posA - oldPosA, oriA, (Convex&)hullB,
				posB - oldPosA, resInOut);
			else
				return TestOBBConvex(hullA,
				com::VecQua(oriB.Conjugate() * (posA - oldPosA) * oriB),
				oriB.Conjugate() * oriA, (Convex&)hullB,
				com::VecQua(oriB.Conjugate() * (posB - oldPosA) * oriB), resInOut, &oriB);
		}
	}
	else
	{
		if(hullB.Type() == BOX) // A Convex, B Box
		{
			bool hit;

			if(oriA.Identity())
			{
				if(oriB.Identity())
					hit = TestAABBConvex(hullB, oldPosA - posA, (Convex&)hullA, oldPosA - posB,
					resInOut);
				else
					hit = TestOBBConvex(hullB, oldPosA - posA, oriB, (Convex&)hullA,
					oldPosA - posB, resInOut);
			}
			else if(oriB.Identity())
				hit = TestOBBConvex(hullB,
				com::VecQua(oriA.Conjugate() * (oldPosA - posA) * oriA), oriA.Conjugate(),
				(Convex&)hullA, com::VecQua(oriA.Conjugate() * (oldPosA - posB) * oriA),
				resInOut, &oriA);
			else
				hit = TestOBBConvex(hullB,
				com::VecQua(oriA.Conjugate() * (oldPosA - posA) * oriA),
				oriA.Conjugate() * oriB, (Convex&)hullA,
				com::VecQua(oriA.Conjugate() * (oldPosA - posB) * oriA), resInOut, &oriA);

			if(hit)
			{
				if(resInOut.contact == Result::HIT)
					resInOut.normal = -resInOut.normal;
				else
					resInOut.mtvDir = -resInOut.mtvDir;

				return true;
			}
		}
		else // A Convex, B Convex
		{
			if(oriA.Identity())
				return TestConvexConvex((Convex&)hullA, posA - oldPosA, (Convex&)hullB,
				posB - oldPosA, oriB, resInOut);
			else if(oriB.Identity())
			{
				if(TestConvexConvex((Convex&)hullB, oldPosA - posA, (Convex&)hullA,
				oldPosA - posB, oriA, resInOut))
				{
					if(resInOut.contact == Result::HIT)
						resInOut.normal = -resInOut.normal;
					else
						resInOut.mtvDir = -resInOut.mtvDir;

					return true;
				}
			}
			else
				return TestConvexConvex((Convex&)hullA,
				com::VecQua(oriA.Conjugate() * (posA - oldPosA) * oriA), (Convex&)hullB,
				com::VecQua(oriA.Conjugate() * (posB - oldPosA) * oriA),
				oriA.Conjugate() * oriB, resInOut, &oriA);
		}
	}

	return false;
}

hit::Result hit::TestHullHull(const Hull& hullA, const com::Vec3& oldPosA,
	const com::Vec3& posA, const com::Qua& oriA, const Hull& hullB, const com::Vec3& posB,
	const com::Qua& oriB)
{
	Result res = {Result::NONE, FLT_MAX, -FLT_MAX};
	TestHullHull(hullA, oldPosA, posA, oriA, hullB, posB, oriB, res);
	return res;
}

/*--------------------------------------
	hit::TestAABBAABB

Swept AABB vs static AABB.
--------------------------------------*/
bool hit::TestAABBAABB(const Hull& hullA, const com::Vec3& vel, const Hull& hullB,
	const com::Vec3& posB, Result& resInOut)
{
	Result res = {Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f};
	
	if
	(
		TestPointSpan(vel.x, hullB.Min().x - hullA.Max().x, hullB.Max().x - hullA.Min().x,
		posB.x, com::Vec3(1.0f, 0.0f, 0.0f), vel, resInOut, res) == false ||

		TestPointSpan(vel.y, hullB.Min().y - hullA.Max().y, hullB.Max().y - hullA.Min().y,
		posB.y, com::Vec3(0.0f, 1.0f, 0.0f), vel, resInOut, res) == false ||

		TestPointSpan(vel.z, hullB.Min().z - hullA.Max().z, hullB.Max().z - hullA.Min().z,
		posB.z, com::Vec3(0.0f, 0.0f, 1.0f), vel, resInOut, res) == false
	)
		return false;

	resInOut = res;
	return true;
}

/*--------------------------------------
	hit::TestAABBOBB

Swept-from-origin AABB vs static OBB.

wldOri can point to the orientation of hullA before the world was rotated to align the hull's
planes with the axes. It's the rotation to set everything back to original orientation.
--------------------------------------*/
bool hit::TestAABBOBB(const Hull& hullA, const com::Vec3& vel, const Hull& hullB,
	const com::Vec3& pos, const com::Qua& ori, Result& resInOut, const com::Qua* wldOri)
{
	Result res = {Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f};
	float minA, maxA, minB, maxB;

	// Test AABB normals
	for(size_t i = 0; i < 3; i++)
	{
		hullB.BoxSpan(com::VecQua(ori.Conjugate() * BOX_AXES[i] * ori), minB, maxB);

		if(TestPointSpan(vel[i], minB - hullA.Max()[i], maxB - hullA.Min()[i], pos[i],
		BOX_AXES[i], vel, resInOut, res, wldOri) == false)
			return false;
	}

	// Test OBB normals
	com::Vec3 rotAxes[3];

	for(size_t i = 0; i < 3; i++)
	{
		rotAxes[i] = com::VecQua(ori * BOX_AXES[i] * ori.Conjugate());
		hullA.BoxSpan(rotAxes[i], minA, maxA);

		if(TestPointSpan(com::Dot(vel, rotAxes[i]), hullB.Min()[i] - maxA,
		hullB.Max()[i] - minA, com::Dot(pos, rotAxes[i]), rotAxes[i], vel, resInOut, res,
		wldOri) == false)
			return false;
	}

	// Test edge-pair cross products
	for(size_t i = 0; i < 3; i++)
	{
		for(size_t j = 0; j < 3; j++)
		{
			com::Vec3 cross = com::Cross(BOX_AXES[i], rotAxes[j]);

			if(float mag = cross.Mag())
				cross /= mag;
			else
				continue;

			hullA.BoxSpan(cross, minA, maxA);
			hullB.BoxSpan(com::VecQua(ori.Conjugate() * cross * ori), minB, maxB);

			if(TestPointSpan(com::Dot(vel, cross), minB - maxA, maxB - minA,
			com::Dot(pos, cross), cross, vel, resInOut, res, wldOri) == false)
				return false;
		}
	}

	// Contact
	// FIXME: Put into one function; caller should do this
	if(wldOri)
	{
		// Rotate normal or mtv to world coordinates
		if(res.contact == Result::HIT)
			res.normal = com::VecQua(*wldOri * res.normal * wldOri->Conjugate());
		else
			res.mtvDir = com::VecQua(*wldOri * res.mtvDir * wldOri->Conjugate());
	}

	resInOut = res;
	return true;
}

/*--------------------------------------
	hit::TestAABBConvex

Swept non-oriented AABB vs static non-oriented convex polyhedron.
--------------------------------------*/
bool hit::TestAABBConvex(const Hull& hullA, const com::Vec3& vel, const Convex& hullB,
	const com::Vec3& pos, Result& resInOut)
{
	Result res = {Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f};
	float minA, maxA, minB, maxB;

	// Test AABB normals
	for(size_t i = 0; i < 3; i++)
	{
		if(TestPointSpan(vel[i], hullB.Min()[i] - hullA.Max()[i],
		hullB.Max()[i] - hullA.Min()[i], pos[i], BOX_AXES[i], vel, resInOut, res) == false)
			return false;
	}

	// Test convex normals
	const com::Vec3* convexAxis = hullB.Axes();

	for(size_t i = 0; i < hullB.NumNormalAxes(); i++, convexAxis++)
	{
		hullA.BoxSpan(*convexAxis, minA, maxA);

		if(TestPointSpan(com::Dot(vel, *convexAxis), hullB.NormalSpans()[i * 2] - maxA,
		hullB.NormalSpans()[i * 2 + 1] - minA, com::Dot(pos, *convexAxis), *convexAxis, vel,
		resInOut, res) == false)
			return false;
	}

	const com::Vec3* edgeAxes = convexAxis;

	// Test edge-pair cross products
	for(size_t i = 0; i < 3; i++)
	{
		for(size_t j = 0; j < hullB.NumEdgeAxes(); j++, convexAxis++)
		{
			com::Vec3 cross = com::Cross(BOX_AXES[i], *convexAxis);

			if(float mag = cross.Mag())
				cross /= mag;
			else
				continue;

			hullA.BoxSpan(cross, minA, maxA);
			hullB.Span(cross, minB, maxB);

			if(TestPointSpan(com::Dot(vel, cross), minB - maxA, maxB - minA,
			com::Dot(pos, cross), cross, vel, resInOut, res) == false)
				return false;
		}

		convexAxis = edgeAxes;
	}

	// Contact
	resInOut = res;
	return true;
}

/*--------------------------------------
	hit::TestOBBConvex

Swept OBB vs static non-oriented convex polyhedron.
--------------------------------------*/
bool hit::TestOBBConvex(const Hull& hullA, const com::Vec3& vel, const com::Qua& ori,
	const Convex& hullB, const com::Vec3& pos, Result& resInOut, const com::Qua* wldOri)
{
	Result res = {Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f};
	float minA, maxA, minB, maxB;

	// Test OBB normals
	com::Vec3 rotAxes[3];

	for(size_t i = 0; i < 3; i++)
	{
		rotAxes[i] = com::VecQua(ori * BOX_AXES[i] * ori.Conjugate());
		hullB.Span(rotAxes[i], minB, maxB);

		if(TestPointSpan(com::Dot(vel, rotAxes[i]), minB - hullA.Max()[i],
		maxB - hullA.Min()[i], com::Dot(pos, rotAxes[i]), rotAxes[i], vel, resInOut, res,
		wldOri) == false)
			return false;
	}

	// Test convex normals
	const com::Vec3* convexAxis = hullB.Axes();

	for(size_t i = 0; i < hullB.NumNormalAxes(); i++, convexAxis++)
	{
		hullA.BoxSpan(com::VecQua(ori.Conjugate() * *convexAxis * ori), minA, maxA);

		if(TestPointSpan(com::Dot(vel, *convexAxis), hullB.NormalSpans()[i * 2] - maxA,
		hullB.NormalSpans()[i * 2 + 1] - minA, com::Dot(pos, *convexAxis), *convexAxis, vel,
		resInOut, res, wldOri) == false)
			return false;
	}

	const com::Vec3* edgeAxes = convexAxis;

	// Test edge-pair cross products
	for(size_t i = 0; i < 3; i++)
	{
		for(size_t j = 0; j < hullB.NumEdgeAxes(); j++, convexAxis++)
		{
			com::Vec3 cross = com::Cross(rotAxes[i], *convexAxis);

			if(float mag = cross.Mag())
				cross /= mag;
			else
				continue;

			hullA.BoxSpan(com::VecQua(ori.Conjugate() * cross * ori), minA, maxA);
			hullB.Span(cross, minB, maxB);

			if(TestPointSpan(com::Dot(vel, cross), minB - maxA, maxB - minA,
			com::Dot(pos, cross), cross, vel, resInOut, res) == false)
				return false;
		}

		convexAxis = edgeAxes;
	}

	// Contact
	if(wldOri)
	{
		// Rotate normal or mtv to world coordinates
		if(res.contact == Result::HIT)
			res.normal = com::VecQua(*wldOri * res.normal * wldOri->Conjugate());
		else
			res.mtvDir = com::VecQua(*wldOri * res.mtvDir * wldOri->Conjugate());
	}

	resInOut = res;
	return true;
}

/*--------------------------------------
	hit::TestConvexConvex

Swept non-oriented convex polyhedron vs static convex polyhedron.
--------------------------------------*/
bool hit::TestConvexConvex(const Convex& hullA, const com::Vec3& vel, const Convex& hullB,
	const com::Vec3& pos, const com::Qua& ori, Result& resInOut, const com::Qua* wldOri)
{
	Result res = {Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f};
	float minA, maxA, minB, maxB;

	// Test A normals
	const com::Vec3* axisA = hullA.Axes();

	for(size_t i = 0; i < hullA.NumNormalAxes(); i++, axisA++)
	{
		hullB.Span(com::VecQua(ori.Conjugate() * *axisA * ori), minB, maxB);

		if(TestPointSpan(com::Dot(vel, *axisA), minB - hullA.NormalSpans()[i * 2 + 1],
		maxB - hullA.NormalSpans()[i * 2], com::Dot(pos, *axisA), *axisA, vel, resInOut, res,
		wldOri) == false)
			return false;
	}

	// Test B normals
	const com::Vec3* axisB = hullB.Axes();

	for(size_t i = 0; i < hullB.NumNormalAxes(); i++, axisB++)
	{
		com::Vec3 rotAxis = com::VecQua(ori * *axisB * ori.Conjugate());
		hullA.Span(rotAxis, minA, maxA);

		if(TestPointSpan(com::Dot(vel, rotAxis), hullB.NormalSpans()[i * 2] - maxA,
		hullB.NormalSpans()[i * 2 + 1] - minA, com::Dot(pos, rotAxis), rotAxis, vel, resInOut,
		res, wldOri) == false)
			return false;
	}

	const com::Vec3* edgeAxesA = axisA;

	// Test edge-pair cross products
	for(size_t i = 0; i < hullB.NumEdgeAxes(); i++, axisB++)
	{
		com::Vec3 rotAxis = com::VecQua(ori * *axisB * ori.Conjugate());

		for(size_t j = 0; j < hullA.NumEdgeAxes(); j++, axisA++)
		{
			com::Vec3 cross = com::Cross(rotAxis, *axisA);

			if(float mag = cross.Mag())
				cross /= mag;
			else
				continue;

			hullA.Span(cross, minA, maxA);
			hullB.Span(com::VecQua(ori.Conjugate() * cross * ori), minB, maxB);

			if(TestPointSpan(com::Dot(vel, cross), minB - maxA, maxB - minA,
			com::Dot(pos, cross), cross, vel, resInOut, res) == false)
				return false;
		}

		axisA = edgeAxesA;
	}

	// Contact
	if(wldOri)
	{
		// Rotate normal or mtv to world coordinates
		if(res.contact == Result::HIT)
			res.normal = com::VecQua(*wldOri * res.normal * wldOri->Conjugate());
		else
			res.mtvDir = com::VecQua(*wldOri * res.mtvDir * wldOri->Conjugate());
	}

	resInOut = res;
	return true;
}

/*--------------------------------------
	hit::TestPointSpan

Swept 1D point vs static 1D line segment.

initRes is the earliest contact found before the current hull-pair test. resInOut is the latest
result of the current hull-pair test. If the world was rotated to align a hull with the axes,
wldOri should point to that hull's orientation.

At the beginning of a hull vs hull test, resInOut should be initialized with the following:
{Result::INTERSECT, 0.0f, 1.0f, 0.0f, 0, FLT_MAX, 0.0f}
--------------------------------------*/
bool hit::TestPointSpan(float pVel, float min, float max, float spanPos, const com::Vec3& axis,
	const com::Vec3& vel, const Result& initRes, Result& resInOut, const com::Qua* wldOri)
{
	// Changing these to >= and <= will allow hulls to touch (have 0 distance) without contact
	// Note that line tests coplanar to the touching surfaces may then go between the hulls
	#define HIT_GT >
	#define HIT_LT <

	min += spanPos;
	max += spanPos;

	float tf, tl;
	com::Vec3 normal;

	if(pVel == 0.0f)
	{
		// Line doesn't move on this axis
		// If the position is outside hull, no contact
		if(min HIT_GT 0.0f || max HIT_LT 0.0f)
			return false;

		tf = -FLT_MAX;
		tl = FLT_MAX;
	}
	else
	{
		tf = min / pVel;
		tl = max / pVel;

		if(tf > tl)
		{
			// Projected line direction is negative, times are flipped
			normal = axis;
			float temp = tf;
			tf = tl;
			tl = temp;
		}
		else
			normal = -axis;

		if(tf > initRes.timeFirst)
			return false; // Given result already hit something else earlier; cancel
		else if(tf == initRes.timeFirst &&
		fabs(com::Dot(wldOri ? com::VecQua(*wldOri * normal * wldOri->Conjugate()) : normal, // FIXME: com::VecRot
		vel)) >= fabs(com::Dot(initRes.normal, vel)))
		{
			// Hit two hulls at same time
			// Picks the best normal for sliding to prevent getting stuck on seams
			// fabs is called so normal sign does not matter here
			return false;
		}
	}

	if(resInOut.contact == Result::INTERSECT)
	{
		if(tf HIT_GT 0.0f)
			resInOut.contact = Result::HIT;
		else
		{
			// Intersection still possible, update minimum translation vector
			if(max < -min)
			{
				if(max < resInOut.mtvMag)
				{
					resInOut.mtvMag = max;
					resInOut.mtvDir = axis;
				}
			}
			else
			{
				if(-min < resInOut.mtvMag)
				{
					resInOut.mtvMag = -min;
					resInOut.mtvDir = -axis;
				}
			}
		}
	}

	if(tf >= resInOut.timeFirst)
	{
		// Hit occurs later
		resInOut.timeFirst = tf;
		resInOut.normal = normal;
	}

	if(tl < resInOut.timeLast)
		resInOut.timeLast = tl; // Exit occurs earlier

	if(resInOut.timeFirst HIT_GT resInOut.timeLast)
		return false; // Earliest exit before latest hit; no hit

	return true;
}

/*
################################################################################################


	HULL TEST LUA


################################################################################################
*/

/*--------------------------------------
LUA	hit::TestLineHull

IN	v3LineA, v3LineB, hulH, v3Pos, q4Ori
OUT	iContact, nTimeFirst, nTimeLast, v3Norm, nil
--------------------------------------*/
int hit::TestLineHull(lua_State* l)
{
	scr::EnsureStack(l, HIT_NUM_RESULT_ELEMENTS);
	Hull* h = Hull::CheckLuaTo(7);
	com::Vec3 lineA = vec::LuaToVec(l, 1, 2, 3);
	com::Vec3 lineB = vec::LuaToVec(l, 4, 5, 6);
	com::Vec3 pos = vec::LuaToVec(l, 8, 9, 10);
	com::Qua ori = qua::LuaToQua(l, 11, 12, 13, 14);

	TestLineHull(lineA, lineB, *h, pos, ori).LuaPush(l);
	return HIT_NUM_RESULT_ELEMENTS;
}

/*--------------------------------------
LUA	hit::TestHullHull

IN	hulA, v3OldPosA, v3PosA, q4OriA, hulB, v3PosB, q4OriB
OUT	iContact, nTimeFirst, nTimeLast, v3Norm, nil
--------------------------------------*/
int hit::TestHullHull(lua_State* l)
{
	scr::EnsureStack(l, HIT_NUM_RESULT_ELEMENTS);
	Hull* hA = Hull::CheckLuaTo(1);
	com::Vec3 oldPosA = vec::LuaToVec(l, 2, 3, 4);
	com::Vec3 posA = vec::LuaToVec(l, 5, 6, 7);
	com::Qua oriA = qua::LuaToQua(l, 8, 9, 10, 11);
	Hull* hB = Hull::CheckLuaTo(12);
	com::Vec3 posB = vec::LuaToVec(l, 13, 14, 15);
	com::Qua oriB = qua::LuaToQua(l, 16, 17, 18, 19);
	TestHullHull(*hA, oldPosA, posA, oriA, *hB, posB, oriB).LuaPush(l);
	return HIT_NUM_RESULT_ELEMENTS;
}