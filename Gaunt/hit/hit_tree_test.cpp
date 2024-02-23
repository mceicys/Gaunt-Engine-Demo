// hit_tree_test.cpp
// Martynas Ceicys

#include "hit.h"
#include "hit_lua.h"
#include "hit_private.h"
#include "../quaternion/qua_lua.h"
#include "../vector/vec_lua.h"

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace hit
{
	Descent gDesc; // This stack is reallocated after a world load to support any loaded tree

#if 0
	template <desc_op op> void LineDescendCopy(desc_elem* s, size_t& numStacked,
		const desc_elem& cur);
	template <int startRight, bool checkDist> void LineDescendSplit(desc_elem* s,
		size_t& numStacked, const float (&dists)[2], const desc_elem& cur);
#endif
	void ThickPlaneSplit(Descent& d, const desc_elem& cur, const float (&offsets)[2]);
	template <bool passRight, bool passFirst> void DoHullOp(int op, Descent& d,
		const desc_elem& cur, const float (&dists)[2][2]);
}

/*
################################################################################################


	DESCENT


################################################################################################
*/

const char* const res::Resource<hit::Descent>::METATABLE = "metaDescent";

/*--------------------------------------
	hit::Descent::Descent
--------------------------------------*/
hit::Descent::Descent() : numStacked(0)
{
	FitLargestTree();
}

/*--------------------------------------
	hit::Descent::~Descent
--------------------------------------*/
hit::Descent::~Descent()
{
	s.Free();
}

/*--------------------------------------
	hit::Descent::CheckLuaToDescElem
--------------------------------------*/
hit::desc_elem* hit::Descent::CheckLuaToDescElem(lua_State* l, int index)
{
	size_t i = (size_t)luaL_checkinteger(l, index);

	if(i >= numStacked)
		luaL_argerror(l, index, "Descent stack index out of bounds");

	return s.o + i;
}

/*--------------------------------------
	hit::Descent::CheckLuaToNotEmpty
--------------------------------------*/
hit::Descent* hit::Descent::CheckLuaToNotEmpty(int index)
{
	Descent* d = CheckLuaTo(index);

	if(!d->numStacked)
		luaL_argerror(scr::state, index, "Descent stack is empty");

	return d;
}

/*--------------------------------------
	hit::Descent::FitLargestTree
--------------------------------------*/
void hit::Descent::FitLargestTree()
{
	size_t maxNumLevels = scn::MaxNumTreeLevels();

	if(maxNumLevels)
		s.EnsureResize(maxNumLevels);
	else
		s.Free();
}

/*--------------------------------------
	hit::Descent::Begin

Sets the stack array to an initial state, starting from root.
--------------------------------------*/
size_t hit::Descent::Begin(scn::Node* root, const com::Vec3& a, const com::Vec3& b)
{
	if(!root)
		return numStacked = 0;

	s.Ensure(1);
	s[0].node = root;
	s[0].a = a;
	s[0].b = b;
	s[0].op = ROOT;
	s[0].hasStart = true;
	return numStacked = 1;
}

/*--------------------------------------
	hit::Descent::LineDescend

Similar to Descend, except this function does not create two planes at different offsets to
emulate a swept volume. A similar result can be acquired using a 0 volume hull, but it's slower.
--------------------------------------*/
size_t hit::Descent::LineDescend()
{
	// Save and pop
	numStacked--;
	desc_elem cur = s[numStacked];

	if(!cur.node->left)
		return numStacked; // Leaf; can't descend

	s.Ensure(numStacked + 2);

	float dists[2] = {
		com::Dot(cur.node->planes->normal, cur.a) - cur.node->planes->offset,
		com::Dot(cur.node->planes->normal, cur.b) - cur.node->planes->offset
	};

	/*
		Segment operations
				{dists[0] cmp 0, dists[1] cmp 0}
		LEFT	{<, <}
		COPY	{=, =}
		RIGHT	{>, >}
		SPLIT	{>, =} {<, =} {=, >} {=, <} {>, <} {<, >}
	*/

	desc_op op;

	if(dists[0] < 0.0f)
	{
		if(dists[1] < 0.0f)
			op = LEFT; // {<, <}
		else
			op = SPLIT; // {<, =} {<, >}
	}
	else if(dists[0] > 0.0f)
	{
		if(dists[1] > 0.0f)
			op = RIGHT; // {>, >}
		else
			op = SPLIT; // {>, =} {>, <}
	}
	else
	{
		if(dists[1] == 0.0f)
			op = COPY; // {=, =}
		else
			op = SPLIT; // {=, >} {=, <}
	}

	if(op == SPLIT)
	{
		// Start segment added to the stack last so the earliest hit is found first
		desc_elem& end = s[numStacked++];
		desc_elem& start = s[numStacked++];

		com::Vec3 cut;

		if(!dists[0])
			cut = cur.a;
		else if(!dists[1])
			cut = cur.b;
		else
			cut = COM_LERP(cur.a, cur.b, dists[0] / (dists[0] - dists[1]));

		start.a = cur.a;
		start.b = end.a = cut;
		end.b = cur.b;

		start.hitNormal = cur.hitNormal;

		if(dists[0] >= dists[1])
		{
			start.node = cur.node->right;
			end.node = cur.node->left;

			end.hitNormal = cur.node->planes->normal;
		}
		else
		{
			start.node = cur.node->left;
			end.node = cur.node->right;

			// If a solid is in front of plane, the hit surface's normal is negative
			end.hitNormal = -cur.node->planes->normal;
		}

		start.op = end.op = SPLIT;

		start.hasStart = cur.hasStart;
		end.hasStart = false;
	}
	else
	{
		if(op <= COPY)
		{
			desc_elem& left = s[numStacked++];
			left.node = cur.node->left;
			left.a = cur.a;
			left.b = cur.b;
			left.hitNormal = cur.hitNormal;
			left.op = op;
			left.hasStart = cur.hasStart;
		}

		if(op >= COPY)
		{
			desc_elem& right = s[numStacked++];
			right.node = cur.node->right;
			right.a = cur.a;
			right.b = cur.b;
			right.hitNormal = cur.hitNormal;
			right.op = op;
			right.hasStart = cur.hasStart;
		}
	}

	return numStacked;
}

/*--------------------------------------
	hit::Descent::Descend

Pops one from the stack and sends the line segments further down the tree. Normally, this should
be called at the end of a while(numStacked) loop.
--------------------------------------*/
size_t hit::Descent::Descend(const Hull& h, const com::Qua& ori)
{
	desc_elem cur = s[--numStacked]; // Save and pop

	if(!cur.node->left)
		return numStacked; // Leaf; can't descend

	s.Ensure(numStacked + 2);
	float min, max;
	Span(h, ori, cur.node->planes->normal, min, max);

	float thickOffsets[2] =
	{
		cur.node->planes->offset - max,
		cur.node->planes->offset - min
	};

	ThickPlaneSplit(*this, cur, thickOffsets);
	return numStacked;
}

/*--------------------------------------
	hit::Descent::SphereDescend
--------------------------------------*/
size_t hit::Descent::SphereDescend(float radius)
{
	desc_elem cur = s[--numStacked];

	if(!cur.node->left)
		return numStacked;

	s.Ensure(numStacked + 2);

	float thickOffsets[2] =
	{
		cur.node->planes->offset - radius,
		cur.node->planes->offset + radius
	};

	ThickPlaneSplit(*this, cur, thickOffsets);
	return numStacked;
}

// FIXME: Profile to see if templated way is faster
#if 0
/*--------------------------------------
	hit::LineDescendCopy
--------------------------------------*/
template <hit::desc_op op> FORCE_INLINE void hit::LineDescendCopy(desc_elem* s, size_t& numStacked,
	const desc_elem& cur)
{
	if(op <= COPY)
	{
		desc_elem& left = s[numStacked++];
		left.node = cur.node->left;
		left.a = cur.a;
		left.b = cur.b;
		left.hitNormal = cur.hitNormal;
		left.op = op;
		left.hasStart = cur.hasStart;
	}

	if(op >= COPY)
	{
		desc_elem& right = s[numStacked++];
		right.node = cur.node->right;
		right.a = cur.a;
		right.b = cur.b;
		right.hitNormal = cur.hitNormal;
		right.op = op;
		right.hasStart = cur.hasStart;
	}
}

/*--------------------------------------
	hit::LineDescendSplit
--------------------------------------*/
template <int startRight, bool checkDist> FORCE_INLINE void hit::LineDescendSplit(
	desc_elem* s, size_t& numStacked, const float (&dists)[2], const desc_elem& cur)
{
	// Start segment added to the stack last so the earliest hit is found first
	desc_elem& end = s[numStacked++];
	desc_elem& start = s[numStacked++];
	com::Vec3 cut;

	if(checkDist && dists[1] == 0.0f)
		cut = cur.b;
	else
		cut = COM_LERP(cur.a, cur.b, dists[0] / (dists[0] - dists[1]));

	start.a = cur.a;
	start.b = end.a = cut;
	end.b = cur.b;
	start.hitNormal = cur.hitNormal;

	if(startRight == 1 || (startRight == 0 && dists[0] >= dists[1]))
	{
		start.node = cur.node->right;
		end.node = cur.node->left;
		end.hitNormal = cur.node->planes->normal;
	}
	else
	{
		start.node = cur.node->left;
		end.node = cur.node->right;

		// If a solid is in front of plane, the hit surface's normal is negative
		end.hitNormal = -cur.node->planes->normal;
	}

	start.op = end.op = SPLIT;
	start.hasStart = cur.hasStart;
	end.hasStart = false;
}
#endif

/*--------------------------------------
	hit::ThickPlaneSplit
--------------------------------------*/
void hit::ThickPlaneSplit(Descent& d, const desc_elem& cur, const float (&offsets)[2])
{
	float projs[2] =
	{
		com::Dot(cur.node->planes->normal, cur.a),
		com::Dot(cur.node->planes->normal, cur.b)
	};

	float dists[2][2] =
	{
		{
			projs[0] - offsets[0],
			projs[1] - offsets[0]
		},
		{
			projs[0] - offsets[1],
			projs[1] - offsets[1]
		}
	};

	desc_op ops[2];

	// Right node
	if(dists[0][0] >= 0.0f)
	{
		if(dists[0][1] >= 0.0f)
			ops[0] = COPY;
		else
			ops[0] = CLIP;
	}
	else
	{
		if(dists[0][1] >= 0.0f)
			ops[0] = CLIP;
		else
			ops[0] = DISCARD;
	}

	// Left node
	if(dists[1][0] > 0.0f)
	{
		if(dists[1][1] > 0.0f)
			ops[1] = DISCARD;
		else
			ops[1] = CLIP;
	}
	else
	{
		if(dists[1][1] > 0.0f)
			ops[1] = CLIP;
		else
			ops[1] = COPY;
	}
	
	// The halfspace the infinite line ends in is pushed first
	// If coplanar, left side is pushed first
	if(dists[0][0] >= dists[0][1])
	{
		DoHullOp<false, true>(ops[1], d, cur, dists);
		DoHullOp<true, false>(ops[0], d, cur, dists);
	}
	else
	{
		DoHullOp<true, true>(ops[0], d, cur, dists);
		DoHullOp<false, false>(ops[1], d, cur, dists);
	}
}

/*--------------------------------------
	hit::DoHullOp
--------------------------------------*/
template <bool passRight, bool passFirst> FORCE_INLINE void hit::DoHullOp(int op, Descent& d,
	const desc_elem& cur, const float (&dists)[2][2])
{
	if(op == COPY)
	{
		desc_elem& e = d.s[d.numStacked++];
		e.node = passRight ? cur.node->right : cur.node->left;
		e.a = cur.a;
		e.b = cur.b;
		e.hitNormal = cur.hitNormal;
		e.op = COPY;
		e.hasStart = cur.hasStart;
	}
	else if(op == CLIP)
	{
		desc_elem& e = d.s[d.numStacked++];

		com::Vec3 cut;
		
		if(passRight)
		{
			e.node = cur.node->right;
			cut = COM_LERP(cur.a, cur.b, dists[0][0] / (dists[0][0] - dists[0][1]));
			e.hitNormal = passFirst ? -cur.node->planes->normal : cur.hitNormal;
		}
		else
		{
			e.node = cur.node->left;
			cut = COM_LERP(cur.a, cur.b, dists[1][0] / (dists[1][0] - dists[1][1]));
			e.hitNormal = passFirst ? cur.node->planes->normal : cur.hitNormal;
		}
		
		e.op = CLIP;

		if(passFirst)
		{
			// Pushed first; side has end point
			e.a = cut;
			e.b = cur.b;
			e.hasStart = false;
		}
		else
		{
			// Pushed last; side has start point
			e.a = cur.a;
			e.b = cut;
			e.hasStart = cur.hasStart;
		}
	}
}

/*--------------------------------------
	hit::GlobalDescent
--------------------------------------*/
hit::Descent& hit::GlobalDescent()
{
	return gDesc;
}

/*
################################################################################################


	DESCENT LUA


################################################################################################
*/

/*--------------------------------------
LUA	hit::CreateDescent (Descent)

OUT	dsc
--------------------------------------*/
int hit::CreateDescent(lua_State* l)
{
	Descent* d = new Descent();
	d->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	hit::DscBegin (Begin)

IN	dscD, iRoot, v3A, v3B
OUT	iNumElements
--------------------------------------*/
int hit::DscBegin(lua_State* l)
{
	lua_pushinteger(l, Descent::CheckLuaTo(1)->Begin(scn::CheckLuaToWorldNode(l, 2),
		vec::LuaToVec(l, 3, 4, 5), vec::LuaToVec(l, 6, 7, 8)));

	return 1;
}

/*--------------------------------------
LUA	hit::DscLineDescend (LineDescend)

IN	dscD
OUT	iNumElements
--------------------------------------*/
int hit::DscLineDescend(lua_State* l)
{
	lua_pushinteger(l, Descent::CheckLuaToNotEmpty(1)->LineDescend());
	return 1;
}

/*--------------------------------------
LUA	hit::DscLineDescendLeaf (LineDescendLeaf)

IN	dscD
OUT	iNumElements
--------------------------------------*/
int hit::DscLineDescendLeaf(lua_State* l)
{
	Descent& d = *Descent::CheckLuaToNotEmpty(1);

	do
	{
		d.LineDescend();
	} while(d.numStacked && d.s[d.numStacked - 1].node->left);

	lua_pushinteger(l, d.numStacked);
	return 1;
}

/*--------------------------------------
LUA	hit::DscDescend (Descend)

IN	dscD, hulH, q4Ori
OUT	iNumElements
--------------------------------------*/
int hit::DscDescend(lua_State* l)
{
	lua_pushinteger(l, Descent::CheckLuaToNotEmpty(1)->Descend(*Hull::CheckLuaTo(2),
		qua::LuaToQua(l, 3, 4, 5, 6)));

	return 1;
}

/*--------------------------------------
LUA	hit::DscDescendLeaf (DescendLeaf)

IN	dscD
OUT	iNumElements
--------------------------------------*/
int hit::DscDescendLeaf(lua_State* l)
{
	Descent& d = *Descent::CheckLuaToNotEmpty(1);
	Hull& h = *Hull::CheckLuaTo(2);
	com::Qua ori = qua::LuaToQua(l, 3, 4, 5, 6);

	do
	{
		d.Descend(h, ori);
	} while(d.numStacked && d.s[d.numStacked - 1].node->left);

	lua_pushinteger(l, d.numStacked);
	return 1;
}

/*--------------------------------------
LUA	hit::DscNumElements (NumElements)

IN	dscD
OUT	iNumElements
--------------------------------------*/
int hit::DscNumElements(lua_State* l)
{
	lua_pushinteger(l, Descent::CheckLuaTo(1)->numStacked);
	return 1;
}

/*--------------------------------------
LUA	hit::DscElementNode (ElementNode)

IN	dscD, iElement
OUT	iNode
--------------------------------------*/
int hit::DscElementNode(lua_State* l)
{
	Descent* d = Descent::CheckLuaTo(1);
	((scn::WorldNode*)d->CheckLuaToDescElem(l, 2)->node)->LuaPush(l);
	return 1;
}

/*--------------------------------------
LUA	hit::DscElementLine (ElementLine)

IN	dscD, iElement
OUT	v3A, v3B
--------------------------------------*/
int hit::DscElementLine(lua_State* l)
{
	Descent* d = Descent::CheckLuaTo(1);
	desc_elem& e = *d->CheckLuaToDescElem(l, 2);
	vec::LuaPushVec(l, e.a);
	vec::LuaPushVec(l, e.b);
	return 6;
}

/*
################################################################################################


	LEAF TEST


################################################################################################
*/

/*--------------------------------------
	hit::BehindBevel

Returns 0 if the line is in front of the bevel. Returns 1 if the line is completely behind the
bevel. Returns 2 if the line hits the bevel, and aInOut and hitNormalOut are adjusted.
--------------------------------------*/
int hit::BehindBevel(const Hull& h, com::Vec3& aInOut, const com::Vec3& b, const com::Qua& ori,
	const com::Plane& bevel, com::Vec3& hitNormalOut)
{
	float min, max;
	Span(h, ori, bevel.normal, min, max);
	float offset = bevel.offset - min;
	float dists[2];
	dists[0] = com::Dot(bevel.normal, aInOut) - offset;

	if(dists[0] > 0.0f)
	{
		dists[1] = com::Dot(bevel.normal, b) - offset;

		if(dists[1] > 0.0f)
			return 0; // Outside
		else
		{
			// Hit
			aInOut = COM_LERP(aInOut, b, dists[0] / (dists[0] - dists[1]));
			hitNormalOut = bevel.normal;
			return 2;
		}
	}
	else
		return 1; // Behind
}

/*--------------------------------------
	hit::LeafExitVec

Assumes leaf is solid and pos is intersecting with leaf. Sets resOut's minimum translation
vector to the closest leaf plane that has a portal to a non-solid leaf.
--------------------------------------*/
void hit::LeafExitVec(const com::Vec3& pos, const scn::WorldNode& leaf, Result& resOut)
{
	resOut.mtvMag = FLT_MAX;

	for(uint32_t i = 0; i < leaf.numPlanes; i++)
	{
		if(!leaf.planeExits[i])
			continue;

		float mag = fabs(com::Dot(pos, leaf.planes[i].normal) - leaf.planes[i].offset);

		if(resOut.mtvMag > mag)
		{
			resOut.mtvMag = mag;
			resOut.mtvDir = leaf.planes[i].normal;
		}
	}

	if(resOut.mtvMag == 0.0f)
		resOut.mtvMag = 0.1f;
}

void hit::LeafExitVec(const Hull& h, const com::Vec3& pos, const com::Qua& ori,
	const scn::WorldNode& leaf, Result& resOut)
{
	resOut.mtvMag = FLT_MAX;

	for(uint32_t i = 0; i < leaf.numPlanes; i++)
	{
		if(!leaf.planeExits[i])
			continue;

		float min, max;
		Span(h, ori, leaf.planes[i].normal, min, max);
		float offset = leaf.planes[i].offset - min;
		float mag = fabs(com::Dot(pos, leaf.planes[i].normal) - offset);

		if(resOut.mtvMag > mag)
		{
			resOut.mtvMag = mag;
			resOut.mtvDir = leaf.planes[i].normal;
		}
	}

	if(resOut.mtvMag == 0.0f)
		resOut.mtvMag = 0.1f; // FIXME: epsilon
}