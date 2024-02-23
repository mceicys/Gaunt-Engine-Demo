// hit_response.cpp
// Martynas Ceicys

#include "hit.h"
#include "hit_lua.h"
#include "../quaternion/qua_lua.h"
#include "../vector/vec_lua.h"

/*
################################################################################################


	COLLISION RESPONSE


################################################################################################
*/

/*--------------------------------------
	hit::RespondStop

Returns a position vector based on the given result from a collision test.

If no hit occurred, b is returned. If an initial intersection occurred, the minimum translation
vector added to a is returned. Otherwise, if a hit occurred, a position between a and b before
contact is returned. If trOut is not 0, it is set to the time [0, 1] of this position.

FIXME FIXME: player with cylinder hull can get stuck between two correctly spaced walls
--------------------------------------*/
com::Vec3 hit::RespondStop(const Result& r, const com::Vec3& a, const com::Vec3& b,
	float* tr)
{
	if(r.contact == Result::HIT)
	{
		float tf = r.timeFirst;
		float dist = com::Dot(a - b, r.normal);

		if(dist > 0.0f)
		{
			// FIXME: relative buffer would be good in large worlds; figure out multiplier
			//#define RESPOND_EPSILON_FACTOR 4.0f
			//tf -= COM_MAX(com::Epsilon(a), com::Epsilon(b)) * RESPOND_EPSILON_FACTOR / dist;

			tf -= HIT_ERROR_BUFFER / dist;

			if(tf < 0.0f)
				tf = 0.0f;
		}

		com::Vec3 n = COM_LERP(a, b, tf);

		if(tr)
			*tr = tf;

		return n;
	}
	else if(r.contact == Result::INTERSECT)
	{
		CON_DLOGF("INTERSECT " COM_VEC3_PRNT, a[0], a[1], a[2]);
		com::Vec3 mtv = r.mtvDir * r.mtvMag;

		// Encourage non-zero MTV coords to not round to zero when added to position
		for(size_t i = 0; i < 3; i++)
		{
			if(r.mtvDir[i] == 0.0f)
				continue;

			float m = COM_MAX(fabs(mtv[i]), COM_MAX(FLT_EPSILON, FLT_EPSILON * fabs(a[i])));
			mtv[i] = r.mtvDir[i] > 0.0f ? m : -m;
		}

		if(tr)
			*tr = 0.0f;

		return a + mtv;
	}
	else if(tr)
		*tr = 1.0f;
	
	return b;
}

/*--------------------------------------
	hit::MoveStop

Does a HullTest with specified hull, ignoring ent, from positions a to b, and calls RespondStop.
Tries again if the first response resolved an intersection.

If rOut is not 0, copies the last test's result. If tmOut is not 0, sets it to the time moved.
--------------------------------------*/
com::Vec3 hit::MoveStop(const Hull& hull, const com::Vec3& a, const com::Vec3& b,
	const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
	const scn::Entity* entIgnore, Result* rOut, float* tm)
{
	Result r = HullTest(scn::WorldRoot(), hull, a, b, ori, type, ignore, entIgnore);
	com::Vec3 pos = RespondStop(r, a, b, tm);

	if(r.contact == Result::INTERSECT)
	{
		r = HullTest(scn::WorldRoot(), hull, pos, b, ori, type, ignore, entIgnore);
		pos = RespondStop(r, pos, b, tm);
	}

	if(rOut)
		*rOut = r;

	return pos;
}

/*--------------------------------------
	hit::MoveClimb

Does a MoveStop and attempts to climb the object that was hit with three MoveStops (up, over,
down). If no progress towards the destination is made, the climb is cancelled. climbedOut is set
to true if the climb succeeds and false otherwise.

The climb is cancelled if the blocking surface's normal.z is not close to 0 or if it climbs onto
a surface with a normal.z <= floorZ.

If resultsOut is not 0, it is assumed resultsOut has at least 4 elements. The results of each
movement are copied to resultsOut.

If numResultsOut is not 0, it is set to the number of movements attempted, 4 max. If the climb
was unsuccessful, only the first move is relevant to the returned position. The number of
results will always be at least 1.

If tmiOut or tmoOut are not 0, they are set to the time of the initial move and the over move.

FIXME: depth requirement that disallows super steep staircases? Extend over test but only use
	part of it for movement
		note: MoveSlide should then check if tmo is ~0 instead of checking numResults to figure
		out whether it should use over-move hit as sliding hit
--------------------------------------*/
com::Vec3 hit::MoveClimb(const Hull& hull, const com::Vec3& a, const com::Vec3& b,
	const com::Qua& ori, float height, float floorZ, test_type type, const flg::FlagSet& ignore,
	const scn::Entity* entIgnore, bool& climbed, Result* results, size_t* numResults,
	float* tmi, float* tmo)
{
	static const float ZERO_EPSILON = 0.001f;
	height += HIT_ERROR_BUFFER; // HACK: To get over slight imprecisions in nav tree
	climbed = false;
	Result r;
	Result* fr = results ? results : &r;
	com::Vec3 pos = MoveStop(hull, a, b, ori, type, ignore, entIgnore, fr, tmi);

	if(numResults)
		*numResults = 1;

	// Attempt climb
	if(fr->contact == Result::HIT && fr->normal.z <= ZERO_EPSILON)
	{
		com::Vec3 stairNormal = fr->normal;
		com::Vec3 up = pos;
		up.z += height;
		up = MoveStop(hull, pos, up, ori, type, ignore, entIgnore, results ? results + 1 : 0);

		if(numResults)
			(*numResults)++;

		float change = up.z - pos.z;

		if(change == 0.0f)
			return pos;

		Result* or = results ? results + 2 : &r;
		com::Vec3 over(b.x, b.y, up.z);
		over = MoveStop(hull, up, over, ori, type, ignore, entIgnore, or, tmo);

		if(numResults)
			(*numResults)++;

		// FIXME: epsilon may need a multiplier; bug test for false climbs in low precision areas
		if(or->contact && com::Dot(up - over, stairNormal) <= com::Epsilon(up))
			return pos; // Didn't advance

		Result* dr = results ? results + 3 : &r;
		com::Vec3 down(over.x, over.y, over.z - change);
		down = MoveStop(hull, over, down, ori, type, ignore, entIgnore, dr);

		if(numResults)
			(*numResults)++;

		if(dr->contact == hit::Result::HIT && dr->normal.z <= floorZ)
			return pos;

		climbed = true;
		pos = down;
	}

	return pos;
}

/*--------------------------------------
	hit::MoveSlide

Moves hull from pos to pos + vel * time. Returns the new position.

vel is set to the velocity of the next move if no force is applied.

time is the time left to move. It's depleted based on how much of the movement was finished
before something was hit.

results must be able to contain two hit results. To slide on acute corner edges, results[0]
should contain the output from the last MoveSlide of the same sequence of MoveSlides (a new
"sequence" is started when caller resets time). Set contact to NONE for the first slide or if
the last hit should be ignored.

results[0]'s output value indicates the relevant obstruction. If a climb succeeds or fails due
to a horizontal obstruction, results[0] is the second, "over" horizontal move. Otherwise,
results[0] is the initial, "grounded" horizontal move. If a climb succeeds, the downword move is
also copied to results[1] and numResults is set to 2 so the caller can query the current floor.
Note that the timeFirst of any result does not correspond to the full move and is largely
useless.
--------------------------------------*/
com::Vec3 hit::MoveSlide(const Hull& hull, com::Vec3 pos, com::Vec3& vel, const com::Qua& ori,
	float& time, Result* results, size_t& numResults, float climbHeight, float floorZ,
	test_type type, const flg::FlagSet& ignore, const scn::Entity* entIgnore)
{
	const float saveTime = time;
	const Result prev = results[0];
	const com::Vec3 dest = pos + vel * time;
	float tm;

	if(climbHeight)
	{
		Result r[4];
		size_t n;
		bool climbed;
		float tmi;

		pos = MoveClimb(hull, pos, dest, ori, climbHeight, floorZ, type, ignore, entIgnore,
			climbed, r, &n, &tmi, &tm);

		if(climbed)
		{
			results[0] = r[2]; // Use over move as sliding hit
			results[1] = r[3];
			numResults = 2;
			time -= time * tmi; // Take off time for initial move
		}
		else
		{
			results[0] = r[n == 3 ? 2 : 0]; // Use over move as slider if climb failed there
			numResults = 1;
			tm = tmi;
		}
	}
	else
	{
		// Not trying to climb
		pos = MoveStop(hull, pos, dest, ori, type, ignore, entIgnore, results, &tm);
		numResults = 1;
	}

	const Result& slide = results[0];

	if(slide.contact == Result::HIT)
	{
		time -= time * tm; // Get new time

		bool acute = prev.contact == Result::HIT && saveTime == time &&
			com::Dot(slide.normal, prev.normal) < 0.0f;

		if(acute)
		{
			// No time passed since last hit and normals imply an acute corner edge
			// Project velocity onto corner edge
			com::Vec3 cross = com::Cross(slide.normal, prev.normal);

			if(float mag = cross.Mag())
			{
				cross /= mag;
				vel = cross * com::Dot(vel, cross);
			}
			else
				acute = false; // Somehow hit similar plane twice; do normal slide
		}

		if(!acute)
		{
			// Project velocity onto hit surface's plane
			vel -= slide.normal * com::Dot(vel, slide.normal);
		}
	}
	else
		time = 0.0f;

	return pos;
}

/*
################################################################################################


	COLLISION RESPONSE LUA


################################################################################################
*/

/*--------------------------------------
LUA	hit::RespondStop

IN	xA, yA, zA, xB, yB, zB, iContact, nTimeFirst, nTimeLast, xNorm, yNorm, zNorm
OUT	xPos, yPos, zPos, nTR

Responds to the given hit result.
--------------------------------------*/
int hit::RespondStop(lua_State* l)
{
	com::Vec3 a = vec::LuaToVec(l, 1, 2, 3);
	com::Vec3 b = vec::LuaToVec(l, 4, 5, 6);

	Result r;
	r.contact = lua_tointeger(l, 7);

	if(r.contact == Result::HIT)
	{
		r.timeFirst = lua_tonumber(l, 8);
		r.timeLast = lua_tonumber(l, 9);
		r.normal = vec::LuaToVec(l, 10, 11, 12);
	}
	else if(r.contact == Result::INTERSECT)
	{
		r.mtvMag = lua_tonumber(l, 8);
		r.mtvDir = vec::LuaToVec(l, 10, 11, 12);
	}

	float tr;
	vec::LuaPushVec(l, RespondStop(r, a, b, &tr));
	lua_pushnumber(l, tr);
	return 4;
}

/*--------------------------------------
LUA	hit::MoveStop

IN	hulH, xA, yA, zA, xB, yB, zB, xOri, yOri, zOri, wOri, iTestType, fsetIgnore, entIgnore,
	[tResultsOut]
OUT	xPos, yPos, zPos, nTM

If resultsOut is a table, the last response's result is copied to it.
--------------------------------------*/
int hit::MoveStop(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	com::Vec3 a = vec::LuaToVec(l, 2, 3, 4);
	com::Vec3 b = vec::LuaToVec(l, 5, 6, 7);
	com::Qua ori = qua::LuaToQua(l, 8, 9, 10, 11);
	test_type testType = (test_type)lua_tointeger(l, 12);
	const flg::FlagSet* ignore = flg::FlagSet::DefaultLuaTo(13);
	scn::Entity* entIgnore = scn::Entity::LuaTo(14);
	float tm;

	if(lua_istable(l, 15))
	{
		Result r;
		vec::LuaPushVec(l, MoveStop(*h, a, b, ori, testType, *ignore, entIgnore, &r, &tm));
		ResultTable(l, 15, &r, 1);
	}
	else
		vec::LuaPushVec(l, MoveStop(*h, a, b, ori, testType, *ignore, entIgnore, 0, &tm));

	lua_pushnumber(l, tm);
	return 4;
}

/*--------------------------------------
LUA	hit::MoveClimb

IN	hulH, xA, yA, zA, xB, yB, zB, xOri, yOri, zOri, wOri, nHeight, nFloorZ, iTestType,
	fsetIgnore, entIgnore, [tResultsOut]
OUT	xPos, yPos, zPos, bClimbed, nTMI, nTMO
--------------------------------------*/
int hit::MoveClimb(lua_State* l)
{
	scr::EnsureStack(l, 6);
	Hull* h = Hull::CheckLuaTo(1);
	com::Vec3 a = vec::LuaToVec(l, 2, 3, 4);
	com::Vec3 b = vec::LuaToVec(l, 5, 6, 7);
	com::Qua ori = qua::LuaToQua(l, 8, 9, 10, 11);
	float height = luaL_checknumber(l, 12);
	float floorZ = luaL_checknumber(l, 13);
	test_type testType = (test_type)luaL_checkinteger(l, 14);
	const flg::FlagSet* ignore = flg::FlagSet::DefaultLuaTo(15);
	scn::Entity* entIgnore = scn::Entity::LuaTo(16);
	bool climbed;
	float tmi, tmo = 0.0f;

	if(lua_istable(l, 17))
	{
		Result results[4];
		size_t numResults;

		vec::LuaPushVec(l, MoveClimb(*h, a, b, ori, height, floorZ, testType, *ignore,
			entIgnore, climbed, results, &numResults, &tmi, &tmo));

		ResultTable(l, 17, results, numResults);
	}
	else
	{
		vec::LuaPushVec(l, MoveClimb(*h, a, b, ori, height, floorZ, testType, *ignore,
			entIgnore, climbed, 0, 0, &tmi, &tmo));
	}

	lua_pushboolean(l, climbed);
	lua_pushnumber(l, tmi);
	lua_pushnumber(l, tmo);
	return 6;
}

/*--------------------------------------
LUA	hit::MoveSlide

IN	hulH, xPos, yPos, zPos, xVel, yVel, zVel, xOri, yOri, zOri, wOri, nTime, tResultsIO,
	nClimbHeight, nFloorZ, iTestType, fsetIgnore, entIgnore
OUT	xPos, yPos, zPos, xVel, yVel, zVel, nTime
--------------------------------------*/
int hit::MoveSlide(lua_State* l)
{
	scr::EnsureStack(l, 8); // Number of return values + 1 for ResultTable call
	Hull* h = Hull::CheckLuaTo(1);
	com::Vec3 pos = vec::LuaToVec(l, 2, 3, 4);
	com::Vec3 vel = vec::LuaToVec(l, 5, 6, 7);
	com::Qua ori = qua::LuaToQua(l, 8, 9, 10, 11);
	float time = luaL_checknumber(l, 12);

	// Get minimal Result needed by MoveSlide
	scr::CheckTable(l, 13);
	Result r[2];
	lua_geti(l, 13, 1);
	lua_geti(l, 13, 4);
	lua_geti(l, 13, 5);
	lua_geti(l, 13, 6);
	r[0].contact = luaL_checkinteger(l, -4);
	r[0].normal = vec::LuaToVec(l, -3, -2, -1);
	lua_pop(l, 4);

	float climbHeight = luaL_checknumber(l, 14);
	float floorZ = luaL_checknumber(l, 15);
	test_type testType = (test_type)luaL_checkinteger(l, 16);
	const flg::FlagSet* ignore = flg::FlagSet::DefaultLuaTo(17);
	scn::Entity* entIgnore = scn::Entity::LuaTo(18);
	size_t numResOut;

	vec::LuaPushVec(l, MoveSlide(*h, pos, vel, ori, time, r, numResOut, climbHeight, floorZ,
		testType, *ignore, entIgnore));

	vec::LuaPushVec(l, vel);
	lua_pushnumber(l, time);
	ResultTable(l, 13, r, numResOut);
	return 7;
}