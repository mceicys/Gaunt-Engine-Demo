// hit.cpp
// Martynas Ceicys

#include <math.h>
#include <float.h>
#include <string.h>

#include "hit.h"
#include "hit_lua.h"
#include "hit_private.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../mod/mod.h"
#include "../quaternion/qua_lua.h"
#include "../script/script.h"
#include "../vector/vec_lua.h"
#include "../wrap/wrap.h"

namespace hit
{
	// HIT TEST
	void PopToNextOp(Descent& d, desc_op op);
	
	// HIT TEST LUA
	size_t LuaToEntIgnores(lua_State* l, int index);
}

/*
################################################################################################


	RESULT


################################################################################################
*/

/*--------------------------------------
	hit::Result::LuaPush

iContact, [nTimeFirst, nTimeLast, xNorm, yNorm, zNorm, [entHit]]
--------------------------------------*/
void hit::Result::LuaPush(lua_State* l)
{
	lua_pushinteger(l, contact);

	if(contact == NONE)
	{
		for(size_t i = 0; i < 6; i++)
			lua_pushnil(l);

		return;
	}

	if(contact == HIT)
		lua_pushnumber(l, timeFirst);
	else
		lua_pushnumber(l, mtvMag);

	lua_pushnumber(l, timeLast);
	contact == HIT ? vec::LuaPushVec(l, normal) : vec::LuaPushVec(l, mtvDir);
	ent ? ent->LuaPush() : lua_pushnil(l);
}

/*--------------------------------------
	hit::ResultTable

Copies result elements to the Lua table at tableIndex, starting from 1, and sets the 0th element
to numResults. Each result takes up HIT_NUM_RESULT_ELEMENTS in the table. The table is not a
sequence because some values may be nil.

Result format:
1 iContact, 2 nTimeFirst, 3 nTimeLast, 4 xNorm, 5 yNorm, 6 zNorm, 7 entHit

Everything after iContact is nil if iContact == NONE.

If iContact is HIT, nTimeFirst is the first time of hit [0, 1], and nTimeLast might be the last
time of hit, depending on what function was called (look at description). If iContact is
INTERSECT, nTimeFirst is used to store the minimum translation vector's magnitude (since the
actual first time of hit is always 0), and nTimeLast may be valid depending on what function was
called.

If iContact is HIT, v3Norm is the hit surface normal. If iContact is INTERSECT, v3Norm is the
minimum translation vector's direction.

entHit is nil if the world made contact or the function does not test entities. Otherwise, it's
the entity that was hit or the entity the minimum translation vector is based off.

FIXME: Note that the 0th element is still hashed; make it a named member?
FIXME: Are all members stored in the internal array if there are index gaps?
	FIXME: Why not just make the nils false, instead?
--------------------------------------*/
void hit::ResultTable(lua_State* l, int tableIndex, const Result* results, size_t numResults)
{
	tableIndex = lua_absindex(l, tableIndex);

	for(size_t i = 0; i < numResults; i++)
	{
		const Result& r = results[i];
		size_t resIndex = i * HIT_NUM_RESULT_ELEMENTS;
		lua_pushinteger(l, r.contact);
		lua_rawseti(l, tableIndex, resIndex + 1);

		if(r.contact == Result::NONE)
		{
			for(int j = 2; j <= 7; j++)
			{
				lua_pushnil(l);
				lua_rawseti(l, tableIndex, resIndex + j);
			}

			continue;
		}

		if(r.contact == Result::HIT)
			lua_pushnumber(l, r.timeFirst);
		else
			lua_pushnumber(l, r.mtvMag);

		lua_rawseti(l, tableIndex, resIndex + 2);
		lua_pushnumber(l, r.timeLast);
		lua_rawseti(l, tableIndex, resIndex + 3);

		for(size_t j = 0; j < 3; j++)
		{
			lua_pushnumber(l, (r.contact == Result::HIT ? r.normal : r.mtvDir)[j]);
			lua_rawseti(l, tableIndex, resIndex + 4 + j);
		}

		if(r.ent)
			r.ent->LuaPush();
		else
			lua_pushnil(l);

		lua_rawseti(l, tableIndex, resIndex + 7);
	}

	lua_pushinteger(l, numResults);
	lua_rawseti(l, tableIndex, 0);
}

/*
################################################################################################


	HIT TEST


################################################################################################
*/

/*--------------------------------------
	hit::LineTest

Sweeps a point from a to b and returns the earliest contact with an entity or the world. The
result's timeLast is only valid if an entity was hit, and it only applies to that particular
entity.

LineTest can return upon the first collision in a leaf unless an earlier descent segment was
entirely copied to both the left and right children of a node (the COPY operation of
LineDescend). The segment may hit something earlier on the other side, so that branch needs
to be tested.
--------------------------------------*/
hit::Result hit::LineTest(scn::WorldNode* root, const com::Vec3& a, const com::Vec3& b,
	const test_type type, const flg::FlagSet& ignore, const scn::Entity** entIgnores,
	size_t numEntIgnores)
{
	Result res = {Result::NONE, FLT_MAX, -FLT_MAX};
	unsigned markCode = scn::Entity::IncHitCode();

	for(size_t i = 0; i < numEntIgnores; i++)
	{
		if(entIgnores[i])
			entIgnores[i]->hitCode = markCode;
	}

	const float moveMag = (b - a).Mag();
	const float invMoveMag = moveMag ? 1.0f / moveMag : 0.0f;
	gDesc.Begin(root, a, b);

	while(gDesc.numStacked)
	{
		desc_elem& top = gDesc.s[gDesc.numStacked - 1];
		scn::WorldNode& topNode = (scn::WorldNode&)*top.node;

		if(!topNode.left) // Leaf
		{
			if(topNode.solid)
			{
				if(type & TREE)
				{
					res.ent = 0;

					if(top.hasStart)
					{
						res.contact = Result::INTERSECT;
						res.timeFirst = 0.0f;
						LeafExitVec(a, topNode, res);

						break; // Intersection; break descent
					}
					else
					{
						float tf = (top.a - a).Mag() * invMoveMag;

						if(res.timeFirst >= tf)
						{
							res.contact = Result::HIT;
							res.timeFirst = tf;
							res.normal = top.hitNormal;
						}

						// Hit solid leaf; pop stack until next COPY branch
						PopToNextOp(gDesc, COPY);
						continue;
					}
				}
			}
			else
			{
				if(type & ENTITIES)
				{
					for(const scn::obj_link<scn::Entity>* it = topNode.entLinks.o;
					it < topNode.entLinks.o + topNode.numEntLinks;
					it++)
					{
						scn::Entity& ent = *it->obj;

						if(ent.hitCode == markCode)
							continue;

						ent.hitCode = markCode;

						if(ent.gFlags.True(ignore))
							continue;

						if((type & ALT) && (ent.flags & scn::Entity::ALT_HIT_TEST))
						{
							// FIXME: hit test root's link-hull or hull before testing children?
							for(scn::Entity* chain = ent.Child(); chain; chain = chain->Child())
							{
								if(!chain->Hull())
									continue;

								if(TestLineHull(a, b, *chain->Hull(), chain->Pos(),
								chain->HullOri(), res))
									res.ent = chain;
							}
						}
						else
						{
							if(!ent.Hull())
								continue;

							if(TestLineHull(a, b, *ent.Hull(), ent.Pos(), ent.HullOri(), res))
								res.ent = &ent;
						}
					}

					if(res.contact == Result::HIT)
					{
						if(res.timeFirst < (top.b - a).Mag() * invMoveMag)
						{
							// Hit entity at a point that is within the current leaf
							// Pop stack until next COPY branch
							PopToNextOp(gDesc, COPY);
							continue;
						}
					}
					else if(res.contact == Result::INTERSECT)
						break; // Intersection; break descent
				}
			}
		}

		gDesc.LineDescend();
	}

	return res;
}

hit::Result hit::LineTest(scn::WorldNode* root, const com::Vec3& a, const com::Vec3& b,
	const test_type type, const flg::FlagSet& ignore, const scn::Entity* entIgnore)
{
	return LineTest(root, a, b, type, ignore, &entIgnore, 1);
}

/*--------------------------------------
	hit::HullTest

FIXME FIXME: bad intersections happening on side of large tower in towers.wld

Sweeps hull from a to b and returns the earliest hit with an entity or the world. The result's
timeLast is only valid if an entity was hit, and it only applies to that particular entity.

Unlike the line test, this function has to descend to further leaves despite hitting something.
Example:

# = original planes
| = offset planes
> = descent line

   \    |#    #\    |
    \   | # A # \   |
     \  |  #  #  \  |
      \ |   # #   \ |
  C    \|    ##    \|  B
        |     #     |
        |\    #     |
        | \   #     |
 >>>>>>>|>>\>>#>>>>>|>>>>>>>>
        |   \ #     |
        |    \#     |
        |     #     |
        |     #\    |

A and B are solid. C is non-solid. In this case, A is checked before B in the descent, but its
hit time is later than B's hit time. A similar thing can happen with entities, where an entity
in a further leaf may be hit before an entity in a closer leaf. Once the current descent's start
point is further than the earliest hit, then no earlier hit in that branch may occur.
--------------------------------------*/
hit::Result hit::HullTest(scn::WorldNode* root, const Hull& hull, const com::Vec3& a,
	const com::Vec3& b, const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
	const scn::Entity** entIgnores, size_t numEntIgnores)
{
	Result res = {Result::NONE, FLT_MAX, -FLT_MAX};
	unsigned markCode = scn::Entity::IncHitCode();

	for(size_t i = 0; i < numEntIgnores; i++)
	{
		if(entIgnores[i])
			entIgnores[i]->hitCode = markCode;
	}

	const float moveMag = (b - a).Mag();
	gDesc.Begin(root, a, b);

	while(gDesc.numStacked)
	{
		desc_elem& top = gDesc.s[gDesc.numStacked - 1];
		scn::WorldNode& topNode = (scn::WorldNode&)*top.node;
		float leafHitTime;

		if(!topNode.left &&
		res.timeFirst >= (leafHitTime = moveMag ? (top.a - a).Mag() / moveMag : 0.0f))
		{
			// Entered leaf at earlier time than earliest hit
			if(topNode.solid)
			{
				if(type & TREE)
				{
					// Test bevels
					uint32_t i = 0;

					for(; i < topNode.numBevels; i++)
					{
						int behind = BehindBevel(hull, top.a, top.b, ori, topNode.bevels[i],
							top.hitNormal);

						if(behind == 0)
							break;
						else if(behind == 2)
						{
							// Hit bevel, top.a has moved forward a little
							top.hasStart = false;
							leafHitTime = moveMag ? (top.a - a).Mag() / moveMag : 0.0f;

							if(res.timeFirst < leafHitTime)
								break; // Passed earliest hit; cancel
						}
					}

					if(i == topNode.numBevels)
					{
						// No bevels put the line segment on the outside
						if(!topNode.hull || (hull.Type() == BOX && ori.Identity()))
						{
							// Test is already done via descent
							res.ent = 0;

							if(top.hasStart)
							{
								res.contact = Result::INTERSECT;
								LeafExitVec(hull, a, ori, topNode, res);
								break; // Intersection; break descent
							}

							res.contact = Result::HIT;
							res.timeFirst = leafHitTime;
							res.normal = top.hitNormal;
						}
						else
						{
							// Do precise test
							if(TestHullHull(hull, a, b, ori, *topNode.hull, 0.0f,
							com::QUA_IDENTITY, res) == true)
							{
								res.ent = 0;

								if(res.contact == Result::INTERSECT)
								{
									LeafExitVec(hull, a, ori, topNode, res);
									break; // Intersection; break descent
								}
							}
						}
					}
				}
			}
			else if(type & ENTITIES)
			{
				for(const scn::obj_link<scn::Entity>* it = topNode.entLinks.o;
				it < topNode.entLinks.o + topNode.numEntLinks;
				it++)
				{
					scn::Entity& ent = *it->obj;

					if(ent.hitCode == markCode)
						continue;

					ent.hitCode = markCode;

					if(ent.gFlags.True(ignore))
						continue;

					if((type & ALT) && (ent.flags & scn::Entity::ALT_HIT_TEST))
					{
						for(scn::Entity* chain = ent.Child(); chain; chain = chain->Child())
						{
							if(!chain->Hull())
								continue;

							if(TestHullHull(hull, a, b, ori, *chain->Hull(), chain->Pos(),
							chain->HullOri(), res))
							{
								res.ent = chain;

								if(res.contact == Result::INTERSECT)
									break;
							}
						}
					}
					else
					{
						if(!ent.Hull())
							continue;

						if(TestHullHull(hull, a, b, ori, *ent.Hull(), ent.Pos(), ent.HullOri(),
						res))
							res.ent = &ent;
					}

					if(res.contact == Result::INTERSECT)
						break; // Intersection; break descent
				}
			}
		}

		gDesc.Descend(hull, ori);
	}

	return res;
}

hit::Result hit::HullTest(scn::WorldNode* root, const Hull& hull, const com::Vec3& a,
	const com::Vec3& b, const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
	const scn::Entity* entIgnore)
{
	return HullTest(root, hull, a, b, ori, type, ignore, &entIgnore, entIgnore ? 1 : 0);
}

/*--------------------------------------
	hit::PopToNextOp
--------------------------------------*/
void hit::PopToNextOp(Descent& d, desc_op op)
{
	d.numStacked--;

	while(d.numStacked && d.s[d.numStacked - 1].op != op)
		d.numStacked--;
}

/*
################################################################################################


	HIT TEST LUA


################################################################################################
*/

static com::Arr<const scn::Entity*> entIgnores(2);

/*--------------------------------------
	hit::LuaToEntIgnores
--------------------------------------*/
size_t hit::LuaToEntIgnores(lua_State* l, int index)
{
	int top = lua_gettop(l);

	if(top < index)
		return 0;

	entIgnores.Ensure(top - index + 1);
	size_t numEntIgnores = 0;

	for(int i = index; i <= top; i++)
	{
		entIgnores[numEntIgnores] = scn::Entity::LuaTo(i);

		if(entIgnores[numEntIgnores])
			numEntIgnores++;
	}

	return numEntIgnores;
}

/*--------------------------------------
LUA	hit::LineTest

IN	v3A, v3B, iTestType, fsetIgnore, [entIgnore, ...]
OUT	iContact, nTimeFirst, nTimeLast, v3Norm, entHit

iTestType can be ENTITIES, ENTITIES_ALT, ALL, ALL_ALT, or TREE.
--------------------------------------*/
int hit::LineTest(lua_State* l)
{
	com::Vec3 a = vec::CheckLuaToVec(l, 1, 2, 3);
	com::Vec3 b = vec::CheckLuaToVec(l, 4, 5, 6);
	test_type testType = (test_type)luaL_checkinteger(l, 7);
	const flg::FlagSet* ignore = flg::FlagSet::DefaultLuaTo(8);
	size_t numEntIgnores = LuaToEntIgnores(l, 9);
	LineTest(scn::WorldRoot(), a, b, testType, *ignore, entIgnores.o, numEntIgnores).LuaPush(l);
	return HIT_NUM_RESULT_ELEMENTS;
}

/*--------------------------------------
LUA	hit::HullTest

IN	hulH, v3A, v3B, q4Ori, iTestType, fsetIgnore, [entIgnore, ...]
OUT	iContact, nTimeFirst, nTimeLast, v3Norm, entHit

iTestType can be ENTITIES, ENTITIES_ALT, ALL, ALL_ALT, or TREE.
--------------------------------------*/
int hit::HullTest(lua_State* l)
{
	scr::EnsureStack(l, HIT_NUM_RESULT_ELEMENTS);
	Hull* h = Hull::CheckLuaTo(1);
	com::Vec3 a = vec::CheckLuaToVec(l, 2, 3, 4);
	com::Vec3 b = vec::CheckLuaToVec(l, 5, 6, 7);
	com::Qua ori = qua::CheckLuaToQua(l, 8, 9, 10, 11);
	test_type testType = (test_type)luaL_checkinteger(l, 12);
	const flg::FlagSet* ignore = flg::FlagSet::DefaultLuaTo(13);
	size_t numEntIgnores = LuaToEntIgnores(l, 14);

	HullTest(scn::WorldRoot(), *h, a, b, ori, testType, *ignore, entIgnores.o,
		numEntIgnores).LuaPush(l);

	return HIT_NUM_RESULT_ELEMENTS;
}

/*--------------------------------------
LUA	hit::DescentEntities

IN	tEnts, hulH, v3A, v3B, q4Ori
OUT	iNumEnts

Does a descent with hulH and fills tEnts' array with entities linked in each visited leaf.
--------------------------------------*/
int hit::DescentEntities(lua_State* l)
{
	luaL_checktype(l, 1, LUA_TTABLE);
	const Hull& h = *Hull::CheckLuaTo(2);
	com::Vec3 a = vec::CheckLuaToVec(l, 3, 4, 5);
	com::Vec3 b = vec::CheckLuaToVec(l, 6, 7, 8);
	com::Qua q = qua::CheckLuaToQua(l, 9, 10, 11, 12);
	int numEnts = 0;
	unsigned hitCode = scn::Entity::IncHitCode();
	gDesc.Begin(scn::WorldRoot(), a, b);

	while(gDesc.numStacked)
	{
		const scn::WorldNode& node = *(scn::WorldNode*)gDesc.s[gDesc.numStacked - 1].node;

		if(!node.left && !node.solid)
		{
			for(size_t i = 0; i < node.numEntLinks; i++)
			{
				const scn::Entity& ent = *node.entLinks[i].obj;

				if(ent.hitCode == hitCode)
					continue;

				ent.hitCode = hitCode;
				ent.LuaPush();
				lua_rawseti(l, 1, ++numEnts);
			}
		}

		gDesc.Descend(h, q);
	}

	lua_pushinteger(l, numEnts);
	return 1;
}

/*--------------------------------------
LUA	hit::SphereEntities

IN	tEnts, v3Pos, nRadius, [fsetIgnore]
OUT	iNumEnts
--------------------------------------*/
int hit::SphereEntities(lua_State* l)
{
	luaL_checktype(l, 1, LUA_TTABLE);
	com::Vec3 pos = vec::CheckLuaToVec(l, 2, 3, 4);
	float radius = luaL_checknumber(l, 5);
	const flg::FlagSet *ignore = flg::FlagSet::DefaultLuaTo(6);
	float sqRadius = radius * radius;
	int numEnts = 0;
	unsigned hitCode = scn::Entity::IncHitCode();
	gDesc.Begin(scn::WorldRoot(), pos, pos);

	while(gDesc.numStacked)
	{
		const scn::WorldNode& node = *(scn::WorldNode*)gDesc.s[gDesc.numStacked - 1].node;

		if(!node.left && !node.solid)
		{
			for(size_t i = 0; i < node.numEntLinks; i++)
			{
				const scn::Entity& ent = *node.entLinks[i].obj;

				if(ent.hitCode == hitCode)
					continue;

				ent.hitCode = hitCode;

				if(ent.gFlags.True(*ignore) || (ent.Pos() - pos).MagSq() > sqRadius)
					continue;

				ent.LuaPush();
				lua_rawseti(l, 1, ++numEnts);
			}
		}

		gDesc.SphereDescend(radius);
	}

	lua_pushinteger(l, numEnts);
	return 1;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	hit::Init
--------------------------------------*/
void hit::Init()
{
	gDesc.AddLock(); // Permanent

	// Lua
	luaL_Reg regs[] =
	{
		{"HullBox", HullBox},
		{"HullConvex", HullConvex},
		{"HullFrustum", HullFrustum},
		{"FindHull", FindHull},
		{"EnsureHull", EnsureHull},
		{"Descent", CreateDescent},
		{"LineTest", LineTest},
		{"HullTest", HullTest},
		{"DescentEntities", DescentEntities},
		{"SphereEntities", SphereEntities},
		{"TestHullHull", TestHullHull},
		{"TestLineHull", TestLineHull},
		{"RespondStop", RespondStop},
		{"MoveStop", MoveStop},
		{"MoveClimb", MoveClimb},
		{"MoveSlide", MoveSlide},
		{0, 0}
	};

	scr::constant consts[] =
	{
		{"NONE", Result::NONE},
		{"HIT", Result::HIT},
		{"INTERSECT", Result::INTERSECT},
		{"ENTITIES", ENTITIES},
		{"ENTITIES_ALT", ENTITIES_ALT},
		{"ALL", ALL},
		{"TREE", TREE},
		{"ALL_ALT", ALL_ALT},
		{"NUM_RESULT_ELEMENTS", HIT_NUM_RESULT_ELEMENTS},
		{"BOX", BOX},
		{"CONVEX", CONVEX},
		{"FRUSTUM", FRUSTUM},
		{0, 0}
	};

	luaL_Reg hulRegs[] =
	{
		{"__gc", HulDelete},
		{"Bounds", HulBounds},
		{"Name", HulName},
		{"Type", HulType},
		{"BoxSpan", HulBoxSpan},
		{"Span", HulSpan},
		{"BoxAverage", HulBoxAverage},
		{"Average", HulAverage},
		{"SetBox", HulSetBox},
		{"Angles", HulAngles},
		{"Dists", HulDists},
		{"SetFrustum", HulSetFrustum},
		{"DrawBoxWire", HulDrawBoxWire},
		{"DrawWire", HulDrawWire},
		{0, 0}
	};

	luaL_Reg dscRegs[] =
	{
		{"__gc", Descent::CLuaDelete},
		{"Begin", DscBegin},
		{"LineDescend", DscLineDescend},
		{"LineDescendLeaf", DscLineDescendLeaf},
		{"Descend", DscDescend},
		{"DescendLeaf", DscDescendLeaf},
		{"NumElements", DscNumElements},
		{"ElementNode", DscElementNode},
		{"ElementLine", DscElementLine},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		hulRegs,
		dscRegs,
		0
	};

	const char* prefixes[] = {
		"Hul",
		"Dsc",
		0
	};

	scr::RegisterLibrary(scr::state, "ghit", regs, consts, 0, metas, prefixes);
	Hull::RegisterMetatable(hulRegs, 0);
	Descent::RegisterMetatable(dscRegs, 0);
}

/*--------------------------------------
	hit::CleanupBegin

Removes one lock from each managed hull and deletes it if it has no additional locks and no
existing Lua userdata. Force garbage collection afterward to also clean up any unlocked managed
hulls with unreferenced userdata. Then call CleanupEnd to relock surviving managed hulls.
--------------------------------------*/
void hit::CleanupBegin()
{
	com::linker<Hull>* next = 0;

	for(com::linker<Hull>* it = hulls.f; it; it = next)
	{
		next = it->next;

		it->o->RemoveLock();

		if(!it->o->Used())
			DeleteHull(it->o);
	}
}

/*--------------------------------------
	hit::CleanupEnd
--------------------------------------*/
void hit::CleanupEnd()
{
	for(com::linker<Hull>* it = hulls.f; it; it = it->next)
		it->o->AddLock();
}