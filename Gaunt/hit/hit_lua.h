// hit_lua.h
// Martynas Ceicys

#ifndef HIT_LUA_H
#define HIT_LUA_H

#include "../script/script.h"

namespace hit
{
	// HULL LUA
	int HullBox(lua_State* l);
	int HullConvex(lua_State* l);
	int HullFrustum(lua_State* l);
	int HulDelete(lua_State* l);
	int HulBounds(lua_State* l);
	int HulName(lua_State* l);
	int HulType(lua_State* l);
	int HulBoxSpan(lua_State* l);
	int HulSpan(lua_State* l);
	int HulBoxAverage(lua_State* l);
	int HulAverage(lua_State* l);
	int HulSetBox(lua_State* l);
	int HulAngles(lua_State* l);
	int HulDists(lua_State* l);
	int HulSetFrustum(lua_State* l);
	int HulDrawBoxWire(lua_State* l);
	int HulDrawWire(lua_State* l);

	// MANAGED HULL LUA
	int FindHull(lua_State* l);
	int EnsureHull(lua_State* l);

	// HIT TEST LUA
	int LineTest(lua_State* l);
	int HullTest(lua_State* l);
	int DescentEntities(lua_State* l);
	int SphereEntities(lua_State* l);

	// HULL TEST LUA
	int TestLineHull(lua_State* l);
	int TestHullHull(lua_State* l);

	// COLLISION RESPONSE LUA
	int RespondStop(lua_State* l);
	int MoveStop(lua_State* l);
	int MoveClimb(lua_State* l);
	int MoveSlide(lua_State* l);

	// DESCENT LUA
	int CreateDescent(lua_State* l);
	int DscBegin(lua_State* l);
	int DscLineDescend(lua_State* l);
	int DscLineDescendLeaf(lua_State* l);
	int DscDescend(lua_State* l);
	int DscDescendLeaf(lua_State* l);
	int DscNumElements(lua_State* l);
	int DscElementNode(lua_State* l);
	int DscElementLine(lua_State* l);
}

#endif