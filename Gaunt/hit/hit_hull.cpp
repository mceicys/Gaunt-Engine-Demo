// hit_hull.cpp
// Martynas Ceicys

#include <string.h>

#include "hit.h"
#include "hit_lua.h"
#include "hit_private.h"
#include "../console/console.h"
#include "../../GauntCommon/convex.h"
#include "../../GauntCommon/io.h"
#include "../mod/mod.h"
#include "../quaternion/qua_lua.h"
#include "../render/render.h"
#include "../vector/vec_lua.h"

namespace hit
{
	// MANAGED HULL
	Hull*		CreateHull(const char* filePath);
	const char*	LoadHullFile(const char* filePath, const char* namePath, Hull*& hullOut);
}

/*
################################################################################################


	HULL


################################################################################################
*/

const char* const hit::Hull::METATABLE = "metaHull";

/*--------------------------------------
	hit::Hull::BoxSpan
--------------------------------------*/
void hit::Hull::BoxSpan(const com::Vec3& axis, float& minOut, float& maxOut) const
{
	if(axis.x >= 0.0f)
	{
		if(axis.y >= 0.0f)
		{
			if(axis.z >= 0.0f)
			{
				minOut = com::Dot(axis, boxMin);
				maxOut = com::Dot(axis, boxMax);
			}
			else
			{
				minOut = com::Dot(axis, com::Vec3(boxMin.x, boxMin.y, boxMax.z));
				maxOut = com::Dot(axis, com::Vec3(boxMax.x, boxMax.y, boxMin.z));
			}
		}
		else
		{
			if(axis.z >= 0.0f)
			{
				minOut = com::Dot(axis, com::Vec3(boxMin.x, boxMax.y, boxMin.z));
				maxOut = com::Dot(axis, com::Vec3(boxMax.x, boxMin.y, boxMax.z));
			}
			else
			{
				minOut = com::Dot(axis, com::Vec3(boxMin.x, boxMax.y, boxMax.z));
				maxOut = com::Dot(axis, com::Vec3(boxMax.x, boxMin.y, boxMin.z));
			}
		}
	}
	else
	{
		if(axis.y >= 0.0f)
		{
			if(axis.z >= 0.0f)
			{
				minOut = com::Dot(axis, com::Vec3(boxMax.x, boxMin.y, boxMin.z));
				maxOut = com::Dot(axis, com::Vec3(boxMin.x, boxMax.y, boxMax.z));
			}
			else
			{
				minOut = com::Dot(axis, com::Vec3(boxMax.x, boxMin.y, boxMax.z));
				maxOut = com::Dot(axis, com::Vec3(boxMin.x, boxMax.y, boxMin.z));
			}
		}
		else
		{
			if(axis.z >= 0.0f)
			{
				minOut = com::Dot(axis, com::Vec3(boxMax.x, boxMax.y, boxMin.z));
				maxOut = com::Dot(axis, com::Vec3(boxMin.x, boxMin.y, boxMax.z));
			}
			else
			{
				minOut = com::Dot(axis, boxMax);
				maxOut = com::Dot(axis, boxMin);
			}
		}
	}
}

/*--------------------------------------
	hit::Hull::SetBox
--------------------------------------*/
void hit::Hull::SetBox(const com::Vec3& min, const com::Vec3& max)
{
	if(type != BOX)
	{
		con::AlertF("Tried to modify box of non-box hull");
		return;
	}

	boxMin = min;
	boxMax = max;
}

/*--------------------------------------
	hit::Hull::DrawBoxWire
--------------------------------------*/
void hit::Hull::DrawBoxWire(const com::Vec3& p, const com::Qua& o, int color, float time) const
{
	com::Vec3 v[8];
	com::BoxVerts(boxMin, boxMax, o, v);
	
	const size_t DRAW[24] = {
		0, 1, 1, 3, 3, 2, 2, 0,
		0, 4, 1, 5, 2, 6, 3, 7,
		4, 5, 5, 7, 7, 6, 6, 4
	};
	
	for(size_t i = 0; i < sizeof(DRAW) / sizeof(size_t); i += 2)
		rnd::DrawLine(v[DRAW[i]] + p, v[DRAW[i + 1]] + p, color, time);
}

/*--------------------------------------
	hit::Convex::Convex

If only verts is given, generates a convex hull surrounding the vertices.

If vertices and axes are given, they must be the output of com::ReadConvexData and not 0. The
constructor copies the addresses of the arrays, so the caller should leave them alone after.
--------------------------------------*/
hit::Convex::Convex(const com::Vec3* verts, size_t numVerts) : Hull(CONVEX, 0.0f, 0.0f),
	markCode(0)
{
	com::list<com::Face> faces;
	com::Vertex* tempVerts;
	const char* err;

	if((err = com::ConvexHull(verts, numVerts, faces, tempVerts)) ||
	(err = com::ConvexVertsAxes(faces, vertices, numVertices, axes, numNormalAxes, numEdgeAxes,
	0)))
	{
		con::AlertF("Convex hull generation failed: %s", err);
		DefaultHull();
		return;
	}

	com::VertBox(vertices, numVertices, boxMin, boxMax);
	com::FreeHull(faces, tempVerts);
	CreateNormalSpans();
}

hit::Convex::Convex(const char* name, com::ClimbVertex* vertices, size_t numVertices,
	com::Vec3* axes, size_t numNormalAxes, size_t numEdgeAxes, unsigned numLocks)
	: Hull(name, CONVEX, 0.0f, 0.0f, numLocks), vertices(vertices), axes(axes),
	numVertices(numVertices), numNormalAxes(numNormalAxes), numEdgeAxes(numEdgeAxes),
	markCode(0)
{
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CreateNormalSpans();
}

/*--------------------------------------
	hit::Convex::~Convex
--------------------------------------*/
hit::Convex::~Convex()
{
	for(size_t i = 0; i < numVertices; i++)
	{
		if(vertices[i].adjacents)
			delete[] vertices[i].adjacents;
	}

	delete[] vertices;
	delete[] axes;
	delete[] normalSpans;
}

/*--------------------------------------
	hit::Convex::Span

FIXME: Hill climbing might be sped up with an initial tetrahedron check.
--------------------------------------*/
void hit::Convex::Span(const com::Vec3& axis, float& minOut, float& maxOut) const
{
	if(numVertices <= HIT_SIMPLE_VERT_LIMIT)
	{
		// Naive
		minOut = maxOut = com::Dot(vertices[0].pos, axis);

		for(size_t i = 1; i < numVertices; i++)
		{
			float dot = com::Dot(vertices[i].pos, axis);

			if(dot < minOut)
				minOut = dot;
			else if(dot > maxOut)
				maxOut = dot;
		}
	}
	else
	{
		// Hill climbing
		vertices->testCode = IncMarkCode();

		com::ClimbVertex *minVert = vertices, *maxVert = vertices;
		minOut = maxOut = com::Dot(*minVert, axis);

		while(1)
		{
			com::ClimbVertex* cur = minVert;

			for(size_t i = 0; i < cur->numAdjacents; i++)
			{
				com::ClimbVertex& adjacent = *cur->adjacents[i];

				if(adjacent.testCode == markCode)
					continue;
				else
					adjacent.testCode = markCode;

				float dot = com::Dot(adjacent.pos, axis);

				if(dot < minOut)
				{
					minOut = dot;
					minVert = &adjacent;
				}
				else if(dot > maxOut)
				{
					maxOut = dot;
					maxVert = &adjacent;
				}
			}

			if(cur == minVert)
				break;
		}

		while(1)
		{
			com::ClimbVertex* cur = maxVert;

			for(size_t i = 0; i < cur->numAdjacents; i++)
			{
				com::ClimbVertex& adjacent = *cur->adjacents[i];

				if(adjacent.testCode == markCode)
					continue;
				else
					adjacent.testCode = markCode;

				float dot = com::Dot(adjacent.pos, axis);

				if(dot > maxOut)
				{
					maxOut = dot;
					maxVert = &adjacent;
				}
			}

			if(cur == maxVert)
				break;
		}
	}
}

/*--------------------------------------
	hit::Convex::FarVertices
--------------------------------------*/
size_t hit::Convex::FarVertices(const com::Vec3& axis,
	com::Arr<com::ClimbVertex*>& far) const
{
	vertices->testCode = IncMarkCode();
	com::ClimbVertex* cur = &vertices[0];
	float maxDist = com::Dot(cur->pos, axis);
	size_t numFar = 1;
	far.Ensure(1);
	far[0] = &vertices[0];

	for(size_t i = 0; i < cur->numAdjacents;)
	{
		com::ClimbVertex& adjacent = *cur->adjacents[i];

		if(adjacent.testCode == markCode)
			continue;
		else
			adjacent.testCode = markCode;

		float dot = com::Dot(adjacent.pos, axis);

		// FIXME: epsilon based on hull radius?
		if(dot > maxDist + HIT_ERROR_BUFFER)
		{
			// Found farther plane, restart list
			maxDist = dot;
			numFar = 1;
			far[0] = &adjacent;
			cur = &adjacent;
			i = 0;
		}
		else if(dot >= maxDist - HIT_ERROR_BUFFER)
		{
			// Found coplanar vertex, add it to list
			far.Ensure(numFar + 1);
			far[numFar++] = &adjacent;
			cur = &adjacent; // Restart search at adjacent
			i = 0;
		}
		else
			i++; // Vertex is behind cur
	}

	return numFar;
}

/*--------------------------------------
	hit::Convex::Average
--------------------------------------*/
com::Vec3 hit::Convex::Average() const
{
	if(!numVertices)
		return 0.0f;

	com::Vec3 accum = 0.0f;

	for(size_t i = 0; i < numVertices; i++)
		accum += vertices[i].pos;

	return accum / (float)numVertices;
}

/*--------------------------------------
	hit::Convex::DrawWire
--------------------------------------*/
void hit::Convex::DrawWire(const com::Vec3& p, const com::Qua& o, int color, float time) const
{
	for(size_t i = 0; i < numVertices; i++)
	{
		const com::ClimbVertex& vert = vertices[i];
		com::Vec3 a = com::VecRot(vert.pos, o) + p;

		for(size_t j = 0; j < vert.numAdjacents; j++)
		{
			com::Vec3 b = com::VecRot(vert.adjacents[j]->pos, o) + p;
			rnd::DrawLine(a, b, color, time);
		}
	}
}

/*--------------------------------------
	hit::Convex::CreateNormalSpans
--------------------------------------*/
void hit::Convex::CreateNormalSpans()
{
	normalSpans = new float[numNormalAxes * 2];
	CalcNormalSpans();
}

/*--------------------------------------
	hit::Convex::CalcNormalSpans
--------------------------------------*/
void hit::Convex::CalcNormalSpans()
{
	for(size_t i = 0; i < numNormalAxes; i++)
		Span(axes[i], normalSpans[i * 2], normalSpans[i * 2 + 1]);
}

/*--------------------------------------
	hit::Convex::DefaultHull
--------------------------------------*/
void hit::Convex::DefaultHull()
{
	numVertices = 1;
	vertices = new com::ClimbVertex[1];
	vertices[0].pos = 0.0f;
	vertices[0].adjacents = 0;
	vertices[0].numAdjacents = vertices[0].testCode = 0;

	numNormalAxes = 1;
	numEdgeAxes = 0;

	axes = new com::Vec3[1];
	axes[0] = com::Vec3(1.0f, 0.0f, 0.0f);

	CreateNormalSpans();
}

/*--------------------------------------
	hit::Convex::IncMarkCode
--------------------------------------*/
size_t hit::Convex::IncMarkCode() const
{
	if(++markCode == 0)
	{
		for(size_t i = 0; i < numVertices; i++)
			vertices[i].testCode = 0;

		markCode = 1;
	}

	return markCode;
}

/*--------------------------------------
	hit::Frustum::Frustum
--------------------------------------*/
hit::Frustum::Frustum(float ha, float va, float nd, float fd) : ha(ha), va(va), nd(nd), fd(fd),
	Convex(FRUSTUM)
{
	numVertices = 8;
	vertices = new com::ClimbVertex[8];
	numNormalAxes = 5;
	numEdgeAxes = 6;
	axes = new com::Vec3[11];

	for(size_t i = 0; i < 8; i++)
	{
		vertices[i].numAdjacents = 3;
		vertices[i].adjacents = new com::ClimbVertex*[3];

		for(size_t j = 0; j < 3; j++)
			vertices[i].adjacents[j] = vertices + (i ^ (1 << j));

		vertices[i].testCode = 0;
	}

	CalcFrustum();
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CreateNormalSpans();
}

/*--------------------------------------
	hit::Frustum::SetAngles
--------------------------------------*/
void hit::Frustum::SetAngles(float h, float v)
{
	ha = h;
	va = v;
	CalcFrustum();
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CalcNormalSpans();
}

/*--------------------------------------
	hit::Frustum::SetNearDist
--------------------------------------*/
void hit::Frustum::SetNearDist(float f)
{
	nd = f;
	float th = tan(ha), tv = tan(va);
	float thn = th * nd, tvn = tv * nd;
	vertices[1].pos = com::Vec3(nd, thn, tvn);
	vertices[3].pos = com::Vec3(nd, -thn, tvn);
	vertices[5].pos = com::Vec3(nd, thn, -tvn);
	vertices[7].pos = com::Vec3(nd, -thn, -tvn);
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CalcNormalSpans();
}

/*--------------------------------------
	hit::Frustum::SetFarDist
--------------------------------------*/
void hit::Frustum::SetFarDist(float f)
{
	fd = f;
	float th = tan(ha), tv = tan(va);
	float thf = th * fd, tvf = tv * fd;
	vertices[0].pos = com::Vec3(fd, thf, tvf);
	vertices[2].pos = com::Vec3(fd, -thf, tvf);
	vertices[4].pos = com::Vec3(fd, thf, -tvf);
	vertices[6].pos = com::Vec3(fd, -thf, -tvf);
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CalcNormalSpans();
}

/*--------------------------------------
	hit::Frustum::Set
--------------------------------------*/
void hit::Frustum::Set(float horiAng, float vertAng, float nearDist, float farDist)
{
	ha = horiAng;
	va = vertAng;
	nd = nearDist;
	fd = farDist;
	CalcFrustum();
	com::VertBox(vertices, numVertices, boxMin, boxMax);
	CalcNormalSpans();
}

/*--------------------------------------
	hit::Frustum::MinSphere

Returns center. n and f temporarily change near and far dists.
--------------------------------------*/
com::Vec3 hit::Frustum::MinSphere(float& radius, float n, float f) const
{
	float kk = com::Vec2(vertices[1].pos.y, vertices[1].pos.z).MagSq() / (nd * nd);
	float fan = f + n;
	float len = f - n;

	if(kk >= len / fan)
	{
		radius = f * sqrt(kk);
		return com::Vec3(f, 0.0f, 0.0f);
	}
	else
	{
		radius = 0.5f * sqrt(len * len + 2.0f * (f * f + n * n) * kk + fan * fan * kk * kk);
		return com::Vec3(0.5f * fan * (1.0f + kk), 0.0f, 0.0f);
	}
}

com::Vec3 hit::Frustum::MinSphere(float& radius) const
{
	return MinSphere(radius, nd, fd);
}

/*--------------------------------------
	hit::Frustum::CheckLuaToFrustum
--------------------------------------*/
hit::Frustum* hit::Frustum::CheckLuaToFrustum(int index)
{
	index = lua_absindex(scr::state, index);
	Hull* h = Hull::CheckLuaTo(index);

	if(h->Type() != FRUSTUM)
		luaL_argerror(scr::state, index, "Expected hull type FRUSTUM");

	return (Frustum*)h;
}

/*--------------------------------------
	hit::Frustum::CalcFrustum
--------------------------------------*/
void hit::Frustum::CalcFrustum()
{
	float sh = sin(ha), ch = cos(ha), th = tan(ha);
	float sv = sin(va), cv = cos(va), tv = tan(va);
	float thn = th * nd, tvn = tv * nd, thf = th * fd, tvf = tv * fd;

	// Normal axes
	axes[0] = com::Vec3(-sh, ch, 0.0f); // left
	axes[1] = com::Vec3(-sv, 0.0f, cv); // top
	axes[2] = com::Vec3(-sh, -ch, 0.0f); // right
	axes[3] = com::Vec3(-sv, 0.0f, -cv); // bottom
	axes[4] = com::Vec3(1.0f, 0.0f, 0.0f); // forward

	// Edge axes
	axes[5] = com::Vec3(0.0f, 1.0f, 0.0f);
	axes[6] = com::Vec3(0.0f, 0.0f, 1.0f);
	axes[7] = com::Vec3(1.0f, th, tv).Normalized(); // left up
	axes[8] = com::Vec3(axes[7].x, -axes[7].y, axes[7].z); // right up
	axes[9] = com::Vec3(axes[7].x, -axes[7].y, -axes[7].z); // right down
	axes[10] = com::Vec3(axes[7].x, axes[7].y, -axes[7].z); // left down

	// Vertices
	vertices[0].pos = com::Vec3(fd, thf, tvf); // far left up
	vertices[1].pos = com::Vec3(nd, thn, tvn); // near left up
	vertices[2].pos = com::Vec3(fd, -thf, tvf); // far right up
	vertices[3].pos = com::Vec3(nd, -thn, tvn); // near right up
	vertices[4].pos = com::Vec3(fd, thf, -tvf); // far left down
	vertices[5].pos = com::Vec3(nd, thn, -tvn); // near left down
	vertices[6].pos = com::Vec3(fd, -thf, -tvf); // far right down
	vertices[7].pos = com::Vec3(nd, -thn, -tvn); // near right down
}

/*--------------------------------------
	hit::Span

Generic span.
--------------------------------------*/
void hit::Span(const Hull& h, const com::Qua& ori, const com::Vec3& axis, float& minOut,
	float& maxOut)
{
	com::Vec3 a(ori.Identity() ? axis : com::VecQua(ori.Conjugate() * axis * ori));

	if(h.Type() == BOX)
		h.BoxSpan(a, minOut, maxOut);
	else
		((Convex&)h).Span(a, minOut, maxOut);
}

/*--------------------------------------
	hit::Average

Generic hull average.
--------------------------------------*/
com::Vec3 hit::Average(const Hull& h)
{
	if(h.Type() == BOX)
		return h.BoxAverage();
	else
		return ((Convex&)h).Average();
}

/*--------------------------------------
	hit::DrawWire
--------------------------------------*/
void hit::DrawWire(const Hull& h, const com::Vec3& p, const com::Qua& o, int color, float time)
{
	if(h.Type() == BOX)
		h.DrawBoxWire(p, o, color, time);
	else
		((Convex&)h).DrawWire(p, o, color, time);
}

/*
################################################################################################


	HULL LUA


################################################################################################
*/

// These hulls are also deleted in __gc, but are not named nor automatically locked

/*--------------------------------------
LUA	hit::HullBox

IN	xMin, yMin, zMin, xMax, yMax, zMax
OUT	hulH
--------------------------------------*/
int hit::HullBox(lua_State* l)
{
	Hull* h = new Hull(vec::LuaToVec(l, 1, 2, 3), vec::LuaToVec(l, 4, 5, 6));
	h->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	hit::HullConvex

IN	tVerts
OUT	hulH

tVerts must be a sequence of 3D vectors.
--------------------------------------*/
int hit::HullConvex(lua_State* l)
{
	size_t len = lua_rawlen(l, 1) / 3;

	if(!len)
		luaL_argerror(l, 1, "tVerts has 0 3D vectors");

	com::Vec3* verts = new com::Vec3[len];

	for(size_t i = 0; i < len; i++)
	{
		lua_rawgeti(l, 1, i * 3 + 1);
		lua_rawgeti(l, 1, i * 3 + 2);
		lua_rawgeti(l, 1, i * 3 + 3);

		verts[i] = vec::LuaToVec(l, 2, 3, 4);

		lua_pop(l, 3);
	}

	Convex* h = new Convex(verts, len);
	h->LuaPush();

	delete[] verts;

	return 1;
}

/*--------------------------------------
LUA	hit::HullFrustum

IN	nHori, nVert, nNear, nFar
OUT	hulH
--------------------------------------*/
int hit::HullFrustum(lua_State* l)
{
	float ha = luaL_checknumber(l, 1), va = luaL_checknumber(l, 2), nd = luaL_checknumber(l, 3),
		fd = luaL_checknumber(l, 4);

	Frustum* h = new Frustum(ha, va, nd, fd);
	h->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	hit::HulDelete (__gc)

IN	hulH
--------------------------------------*/
int hit::HulDelete(lua_State* l)
{
	Hull* h = Hull::UserdataLuaTo(1);

	if(!h)
		return 0;

	DeleteHull(h);
	return 0;
}

/*--------------------------------------
LUA	hit::HulBounds (Bounds)

IN	hulH
OUT	xMin, yMin, zMin, xMax, yMax, zMax
--------------------------------------*/
int hit::HulBounds(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	vec::LuaPushVec(l, h->Min());
	vec::LuaPushVec(l, h->Max());
	return 6;
}

/*--------------------------------------
LUA	hit::HulName (Name)

IN	hulH
OUT	sName
--------------------------------------*/
int hit::HulName(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	lua_pushstring(l, h->Name());
	return 1;
}

/*--------------------------------------
LUA	hit::HulType (Type)

IN	hulH
OUT	iType

Returns BOX, CONVEX, or FRUSTUM.
--------------------------------------*/
int hit::HulType(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	lua_pushinteger(l, h->Type());
	return 1;
}

/*--------------------------------------
LUA	hit::HulBoxSpan (BoxSpan)

IN	hulH, v3Axis
OUT	nMin, nMax
--------------------------------------*/
int hit::HulBoxSpan(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	float min, max;
	h->BoxSpan(vec::LuaToVec(l, 2, 3, 4), min, max);
	lua_pushnumber(l, min);
	lua_pushnumber(l, max);
	return 2;
}

/*--------------------------------------
LUA	hit::HulSpan (Span)

IN	hulH, qOri, v3Axis
OUT	nMin, nMax
--------------------------------------*/
int hit::HulSpan(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	float min, max;
	Span(*h, qua::LuaToQua(l, 2, 3, 4, 5), vec::LuaToVec(l, 6, 7, 8), min, max);
	lua_pushnumber(l, min);
	lua_pushnumber(l, max);
	return 2;
}

/*--------------------------------------
LUA	hit::HulBoxAverage (BoxAverage)

IN	hulH
OUT	v3Avg
--------------------------------------*/
int hit::HulBoxAverage(lua_State* l)
{
	vec::LuaPushVec(l, Hull::CheckLuaTo(1)->BoxAverage());
	return 3;
}

/*--------------------------------------
LUA	hit::HulAverage (Average)

IN	hulH
OUT	v3Avg
--------------------------------------*/
int hit::HulAverage(lua_State* l)
{
	vec::LuaPushVec(l, Average(*Hull::CheckLuaTo(1)));
	return 3;
}

/*--------------------------------------
LUA	hit::HulSetBox (SetBox)

IN	hulH, v3Min, v3Max
--------------------------------------*/
int hit::HulSetBox(lua_State* l)
{
	Hull::CheckLuaTo(1)->SetBox(vec::LuaToVec(l, 2, 3, 4), vec::LuaToVec(l, 5, 6, 7));
	return 0;
}

/*--------------------------------------
LUA	hit::HulAngles (Angles)

IN	hulH
OUT	nHori, nVert
--------------------------------------*/
int hit::HulAngles(lua_State* l)
{
	Frustum* h = Frustum::CheckLuaToFrustum(1);
	lua_pushnumber(l, h->HoriAng());
	lua_pushnumber(l, h->VertAng());
	return 2;
}

/*--------------------------------------
LUA	hit::HulDists (Dists)

IN	hulH
OUT	nNear, nFar
--------------------------------------*/
int hit::HulDists(lua_State* l)
{
	Frustum* h = Frustum::CheckLuaToFrustum(1);
	lua_pushnumber(l, h->NearDist());
	lua_pushnumber(l, h->FarDist());
	return 2;
}

/*--------------------------------------
LUA	hit::HulSetFrustum (SetFrustum)

IN	hulH, nHori, nVert, nNear, nFar
--------------------------------------*/
int hit::HulSetFrustum(lua_State* l)
{
	Frustum* h = Frustum::CheckLuaToFrustum(1);
	h->Set(luaL_checknumber(l, 2), luaL_checknumber(l, 3), luaL_checknumber(l, 4),
		luaL_checknumber(l, 5));

	return 0;
}

/*--------------------------------------
LUA	hit::HulDrawBoxWire (DrawBoxWire)

IN	hulH, v3Pos, qOri, iColor, [nTime = 0]
--------------------------------------*/
int hit::HulDrawBoxWire(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	h->DrawBoxWire(vec::LuaToVec(l, 2, 3, 4), qua::LuaToQua(l, 5, 6, 7, 8), lua_tointeger(l, 9),
		luaL_optnumber(l, 10, 0.0));
	return 0;
}

/*--------------------------------------
LUA	hit::HulDrawWire (DrawWire)

IN	hulH, v3Pos, qOri, iColor, [nTime = 0]
--------------------------------------*/
int hit::HulDrawWire(lua_State* l)
{
	Hull* h = Hull::CheckLuaTo(1);
	DrawWire(*h, vec::LuaToVec(l, 2, 3, 4), qua::LuaToQua(l, 5, 6, 7, 8), lua_tointeger(l, 9),
		luaL_optnumber(l, 10, 0.0));
	return 0;
}

/*
################################################################################################


	MANAGED HULL


################################################################################################
*/

// FIXME: hash table
com::list<hit::Hull> hit::hulls = {};

/*--------------------------------------
	hit::FindHull
--------------------------------------*/
hit::Hull* hit::FindHull(const char* name)
{
	for(com::linker<Hull>* it = hulls.f; it; it = it->next)
	{
		if(!strcmp(it->o->Name(), name))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	hit::EnsureHull
--------------------------------------*/
hit::Hull* hit::EnsureHull(const char* filePath)
{
	Hull* h;
	return (h = FindHull(filePath)) ? h : CreateHull(filePath);
}

/*--------------------------------------
	hit::CreateHull
--------------------------------------*/
hit::Hull* hit::CreateHull(const char* filePath)
{
	Hull* h;
	const char *err = 0, *path = mod::Path("hulls/", filePath, err);

	if(err || (err = LoadHullFile(path, filePath, h)))
	{
		con::LogF("Failed to load hull '%s' (%s)", filePath, err);
		return 0;
	}

	com::Link(hulls.f, hulls.l, h);

	return h;
}

/*--------------------------------------
	hit::LoadHullFile

GAUNHL
le uint16_t version = 0
le uint32_t type

vec3_t = float[3]

if type == 0 (box)
	le vec3_t	min, max
else if type == 1 (convex)
	le uint32_t	numVertices
	le uint32_t	numNormalAxes
	le uint32_t	numEdgeAxes
	vertices[numVertices]
		le vec3_t	pos
		le uint32_t	numAdjacents
		le uint32_t	adjacentIndices[numAdjacents]
	le vec3_t	axes[numNormalAxes + numEdgeAxes]

FIXME: update signature
--------------------------------------*/

#define LOAD_HULL_FILE_FAIL(err) { \
	if(file) fclose(file); \
	return err; \
}

const char*	hit::LoadHullFile(const char* filePath, const char* namePath, Hull*& hullOut)
{
	unsigned char* convexBytes = 0;

	FILE* file = fopen(filePath, "rb");

	if(!file)
		LOAD_HULL_FILE_FAIL("Could not open file");

	// Header
	unsigned char header[12];

	if(fread(header, sizeof(unsigned char), 12, file) != 12)
		LOAD_HULL_FILE_FAIL("Could not read header");

	if(strncmp((char*)header, "GAUNHL", 6))
		LOAD_HULL_FILE_FAIL("Incorrect signature");

	uint16_t version;
	com::MergeLE(header + 6, version);

	if(version != 0)
		LOAD_HULL_FILE_FAIL("Unknown hull file version");

	uint32_t type;
	com::MergeLE(header + 8, type);

	// Hull
	if(type == BOX)
	{
		const size_t NUM_BOX_BYTES = sizeof(float) * 6;
		unsigned char boxBytes[NUM_BOX_BYTES];

		if(fread(boxBytes, sizeof(unsigned char), NUM_BOX_BYTES, file) != NUM_BOX_BYTES)
			LOAD_HULL_FILE_FAIL("Could not read box size");

		float boxSize[6];

		for(size_t i = 0; i < 6; i++)
			com::MergeLE(boxBytes + i * sizeof(float), boxSize[i]);

		hullOut = new Hull(namePath, com::Vec3(boxSize[0], boxSize[1], boxSize[2]),
			com::Vec3(boxSize[3], boxSize[4], boxSize[5]), 1);
	}
	else if(type == CONVEX)
	{
		com::ClimbVertex* vertices;
		com::Vec3* axes;
		size_t numVertices, numNormalAxes, numEdgeAxes;

		if(const char* err = com::ReadConvexData(vertices, numVertices, axes, numNormalAxes,
		numEdgeAxes, 0, file))
			LOAD_HULL_FILE_FAIL(err);

		if(!vertices)
			LOAD_HULL_FILE_FAIL("No vertices");

		hullOut = new Convex(namePath, vertices, numVertices, axes, numNormalAxes, numEdgeAxes,
			1);
	}
	else
		LOAD_HULL_FILE_FAIL("Invalid hull type");

	fclose(file);

	return 0;
}

/*--------------------------------------
	hit::DeleteHull
--------------------------------------*/
void hit::DeleteHull(Hull* h)
{
	if(h->Name())
	{
		for(com::linker<Hull>* it = hulls.f; it; it = it->next)
		{
			if(it->o->Name() == h->Name())
			{
				com::Unlink(hulls.f, hulls.l, it);
				break;
			}
		}
	}

	if(h->Type() == BOX)
		delete h;
	else
		delete (Convex*)h;
}

/*
################################################################################################


	MANAGED HULL LUA


################################################################################################
*/

/*--------------------------------------
LUA	hit::FindHull

IN	sFilePath
OUT	hulH
--------------------------------------*/
int hit::FindHull(lua_State* l)
{
	const char* filePath = luaL_checkstring(l, 1);

	if(Hull* h = FindHull(filePath))
	{
		h->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	hit::EnsureHull

IN	sFilePath
OUT	hulH
--------------------------------------*/
int hit::EnsureHull(lua_State* l)
{
	const char* filePath = luaL_checkstring(l, 1);

	if(Hull* h = EnsureHull(filePath))
	{
		h->LuaPush();
		return 1;
	}

	return 0;
}