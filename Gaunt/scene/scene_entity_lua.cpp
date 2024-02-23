// scene_entity_lua.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../quaternion/qua_lua.h"
#include "../render/render.h"

namespace scn
{
	void PushUservalueTable(lua_State* l, int index);
}

/*--------------------------------------
	scn::PushUservalueTable
--------------------------------------*/
void scn::PushUservalueTable(lua_State* l, int index)
{
	if(lua_type(l, index) != LUA_TUSERDATA)
		luaL_argerror(l, index, "not a full userdata");

	if(lua_getuservalue(l, index) != LUA_TTABLE)
	{
		lua_pop(l, 1);
		luaL_argerror(l, index, "has no uservalue table");
	}
}

/*
################################################################################################


	ENTITY LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::CreateEntity

IN	etT, xPos, yPos, zPos, xOri, yOri, zOri, wOri, ...
OUT	ent

A reference to the new entity and extra arguments are sent to the Init function.
--------------------------------------*/
int scn::CreateEntity(lua_State* l)
{
	EntityType* t = EntityType::OptionalLuaTo(1);
	com::Vec3 pos = vec::LuaToVec(l, 2, 3, 4);
	com::Qua ori = qua::LuaToQua(l, 5, 6, 7, 8);
	Entity* ent = CreateEntity(t, pos, ori);

	// Push userdata first since Init may delete the entity
	scr::EnsureStack(l, 1);
	ent->LuaPush();
	lua_insert(l, 9);

	ent->Call(ENT_FUNC_INIT, lua_gettop(l) - 9);
	return 1;
}

/*--------------------------------------
LUA	scn::EntIndex (__index)

IN	ud, key
OUT	value

Returns uval[key] (where uval is ud's uservalue) unless it's nil, in which case returns raw
meta[key] (where meta is ud's metatable) if meta exists.
--------------------------------------*/
int scn::EntIndex(lua_State* l)
{
	PushUservalueTable(l, 1);
	lua_pushvalue(l, 2); // Push copy of key

	// Pop key and push uval[key]
	if(lua_gettable(l, -2) == LUA_TNIL)
	{
		lua_pop(l, 2); // Pop nil and uservalue table

		// Push metatable (presumably metaEntity)
		if(!lua_getmetatable(l, 1))
		{
			lua_pushnil(l);
			return 1;
		}

		lua_pushvalue(l, 2); // Push copy of key
		lua_rawget(l, -2); // Pop key and push metaEntity[key]
	}

	lua_remove(l, -2); // Remove table
	return 1;
}

/*--------------------------------------
LUA	scn::EntNewIndex (__newindex)

IN	ud, key, value

Does uval[key] = value where uval is ud's uservalue.

FIXME: If Lua functions may consume arguments, this can avoid copying key and value
--------------------------------------*/
int scn::EntNewIndex(lua_State* l)
{
	PushUservalueTable(l, 1);
	lua_pushvalue(l, 2);
	lua_pushvalue(l, 3);
	lua_rawset(l, -3);
	lua_pop(l, 1);
	return 0;
}

/*--------------------------------------
LUA	scn::EntLen (__len)

IN	ud
OUT	len

Returns #uval (where uval is ud's uservalue).
--------------------------------------*/
int scn::EntLen(lua_State* l)
{
	PushUservalueTable(l, 1);
	lua_len(l, -1);
	return 1;
}

/*--------------------------------------
LUA	scn::EntTable (Table)

IN	ud
OUT	tUV

Returns ud's uservalue table.
--------------------------------------*/
int scn::EntTable(lua_State* l)
{
	PushUservalueTable(l, 1);
	return 1;
}

/*--------------------------------------
LUA	scn::EntDelete (Delete)

IN	entE
--------------------------------------*/
int scn::EntDelete(lua_State* l)
{
	KillEntity(*Entity::CheckLuaTo(1));
	return 0;
}

/*--------------------------------------
LUA	scn::EntDummyCopy (DummyCopy)

IN	entDest, entSrc
--------------------------------------*/
int scn::EntDummyCopy(lua_State* l)
{
	Entity::CheckLuaTo(1)->DummyCopy(*Entity::CheckLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntType (Type)

IN	entE
OUT	etT
--------------------------------------*/
int scn::EntType(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);

	if(e->type)
		e->type->LuaPush();
	else
		lua_pushnil(l);

	return 1;
}

/*--------------------------------------
LUA	scn::EntType (SetType)

IN	entE, etT
--------------------------------------*/
int scn::EntSetType(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->type = EntityType::OptionalLuaTo(2);
	return 0;
}

/*--------------------------------------
LUA	scn::EntTypeName (TypeName)

IN	entE
OUT	sType
--------------------------------------*/
int scn::EntTypeName(lua_State* l)
{
	lua_pushstring(l, Entity::CheckLuaTo(1)->TypeName());
	return 1;
}

/*--------------------------------------
LUA	scn::EntMesh (Mesh)

IN	entE
OUT	mshM
--------------------------------------*/
int scn::EntMesh(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);

	if(e->Mesh())
		e->Mesh()->LuaPush();
	else
		lua_pushnil(l);

	return 1;
}

/*--------------------------------------
LUA	scn::EntSetMesh (SetMesh)

IN	entE, mshM
--------------------------------------*/
int scn::EntSetMesh(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	rnd::Mesh* m = rnd::Mesh::OptionalLuaTo(2);
	e->SetMesh(m);
	return 0;
}

/*--------------------------------------
LUA	scn::EntTexture (Texture)

IN	entE
OUT	texT
--------------------------------------*/
int scn::EntTexture(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);

	if(e->tex)
		e->tex->LuaPush();
	else
		lua_pushnil(l);

	return 1;
}

/*--------------------------------------
LUA	scn::EntSetTexture (SetTexture)

IN	entE, texT
--------------------------------------*/
int scn::EntSetTexture(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	rnd::Texture* t = rnd::Texture::OptionalLuaTo(2);
	e->tex.Set(t);
	return 0;
}

/*--------------------------------------
LUA	scn::EntHull (Hull)

IN	entE
OUT	hulH
--------------------------------------*/
int scn::EntHull(lua_State* l)
{
	if(hit::Hull* h = Entity::CheckLuaTo(1)->Hull())
	{
		h->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	scn::EntSetHull (SetHull)

IN	entE, hulH
--------------------------------------*/
int scn::EntSetHull(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetHull(hit::Hull::OptionalLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntLinkHull (LinkHull)

IN	entE
OUT	hulLink
--------------------------------------*/
int scn::EntLinkHull(lua_State* l)
{
	if(hit::Hull* link = Entity::CheckLuaTo(1)->LinkHull())
	{
		link->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	scn::EntSetLinkHull (SetLinkHull)

IN	entE, hulLink
--------------------------------------*/
int scn::EntSetLinkHull(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetLinkHull(hit::Hull::OptionalLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntAltHitTest (AltHitTest)

IN	entE
OUT	bAltHitTest
--------------------------------------*/
int scn::EntAltHitTest(lua_State* l)
{
	lua_pushboolean(l, Entity::CheckLuaTo(1)->flags & Entity::ALT_HIT_TEST);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetAltHitTest (SetAltHitTest)

IN	entE, bAltHitTest
--------------------------------------*/
int scn::EntSetAltHitTest(lua_State* l)
{
	scn::Entity* e = Entity::CheckLuaTo(1);
	e->flags = com::FixedBits(e->flags, e->ALT_HIT_TEST, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::EntPos (Pos)

IN	entE
OUT	xPos, yPos, zPos
--------------------------------------*/
int scn::EntPos(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->Pos());
	return 3;
}

/*--------------------------------------
LUA	scn::EntSetPos (SetPos)

IN	entE, xPos, yPos, zPos
--------------------------------------*/
int scn::EntSetPos(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetPos(vec::CheckLuaToVec(l, 2, 3, 4));
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldPos (OldPos)

IN	entE
OUT	xOldPos, yOldPos, zOldPos
--------------------------------------*/
int scn::EntOldPos(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->oldPos);
	return 3;
}

/*--------------------------------------
LUA	scn::EntSetOldPos (SetOldPos)

IN	entE, xOldPos, yOldPos, zOldPos
--------------------------------------*/
int scn::EntSetOldPos(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->oldPos = vec::CheckLuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	scn::EntFinalPos (FinalPos)

IN	entE
OUT	xFinPos, yFinPos, zFinPos
--------------------------------------*/
int scn::EntFinalPos(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->FinalPos());
	return 3;
}

/*--------------------------------------
LUA	scn::EntOri (Ori)

IN	entE
OUT	xOri, yOri, zOri, wOri
--------------------------------------*/
int scn::EntOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	qua::LuaPushQua(l, e->Ori());
	return 4;
}

/*--------------------------------------
LUA	scn::EntSetOri (SetOri)

IN	entE, xOri, yOri, zOri, wOri
--------------------------------------*/
int scn::EntSetOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetOri(qua::CheckLuaToQua(l, 2, 3, 4, 5));
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldOri (OldOri)

IN	entE
OUT	xOldOri, yOldOri, zOldOri, wOldOri
--------------------------------------*/
int scn::EntOldOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	qua::LuaPushQua(l, e->oldOri);
	return 4;
}

/*--------------------------------------
LUA	scn::EntSetOldOri (SetOldOri)

IN	entE, xOldOri, yOldOri, zOldOri, wOldOri
--------------------------------------*/
int scn::EntSetOldOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->oldOri = qua::CheckLuaToQua(l, 2, 3, 4, 5);
	return 0;
}

/*--------------------------------------
LUA	scn::EntFinalOri (FinalOri)

IN	entE
OUT	xFinOri, yFinOri, zFinOri, wFinOri
--------------------------------------*/
int scn::EntFinalOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	qua::LuaPushQua(l, e->FinalOri());
	return 4;
}

/*--------------------------------------
LUA	scn::EntHullOri (HullOri)

IN	entE
OUT	xHullOri, yHullOri, zHullOri, wHullOri
--------------------------------------*/
int scn::EntHullOri(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	qua::LuaPushQua(l, e->HullOri());
	return 4;
}

/*--------------------------------------
LUA	scn::EntPlace (Place)

IN	entE
OUT	xPos, yPos, zPos, xOri, yOri, zOri, wOri
--------------------------------------*/
int scn::EntPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->Pos());
	qua::LuaPushQua(l, e->Ori());
	return 7;
}

/*--------------------------------------
LUA	scn::EntSetPlace (SetPlace)

IN	entE, xPos, yPos, zPos, xOri, yOri, zOri, wOri
--------------------------------------*/
int scn::EntSetPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetPlace(vec::CheckLuaToVec(l, 2, 3, 4), qua::CheckLuaToQua(l, 5, 6, 7, 8));
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldPlace (OldPlace)

IN	entE
OUT	xOldPos, yOldPos, zOldPos, xOldOri, yOldOri, zOldOri, wOldOri
--------------------------------------*/
int scn::EntOldPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->oldPos);
	qua::LuaPushQua(l, e->oldOri);
	return 7;
}

/*--------------------------------------
LUA	scn::EntSetOldPlace (SetOldPlace)

IN	entE, xOldPos, yOldPos, zOldPos, xOldOri, yOldOri, zOldOri, wOldOri
--------------------------------------*/
int scn::EntSetOldPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->oldPos = vec::CheckLuaToVec(l, 2, 3, 4);
	e->oldOri = qua::CheckLuaToQua(l, 5, 6, 7, 8);
	return 0;
}

/*--------------------------------------
LUA	scn::EntFinalPlace (FinalPlace)

IN	entE
OUT	xFinPos, yFinPos, zFinPos, xFinOri, yFinOri, zFinOri, wFinOri
--------------------------------------*/
int scn::EntFinalPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	vec::LuaPushVec(l, e->FinalPos());
	qua::LuaPushQua(l, e->FinalOri());
	return 7;
}

/*--------------------------------------
LUA	scn::EntScale (Scale)

IN	entE
OUT	nScale
--------------------------------------*/
int scn::EntScale(lua_State* l)
{
	lua_pushnumber(l, Entity::CheckLuaTo(1)->Scale());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetScale (SetScale)

IN	entE, nScale
--------------------------------------*/
int scn::EntSetScale(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetScale(luaL_checknumber(l, 2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldScale (OldScale)

IN	entE
OUT	nOldScale
--------------------------------------*/
int scn::EntOldScale(lua_State* l)
{
	lua_pushnumber(l, Entity::CheckLuaTo(1)->oldScale);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetOldScale (SetOldScale)

IN	entE, nOldScale
--------------------------------------*/
int scn::EntSetOldScale(lua_State* l)
{
	Entity::CheckLuaTo(1)->oldScale = luaL_checknumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::EntFinalScale (FinalScale)

IN	entE
OUT	nFinalScale
--------------------------------------*/
int scn::EntFinalScale(lua_State* l)
{
	lua_pushnumber(l, Entity::CheckLuaTo(1)->FinalScale());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetScaledPlace (SetScaledPlace)

IN	entE, v3Pos, q4Ori, nScale
--------------------------------------*/
int scn::EntSetScaledPlace(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetScaledPlace(vec::CheckLuaToVec(l, 2, 3, 4), qua::CheckLuaToQua(l, 5, 6, 7, 8),
		luaL_checknumber(l, 9));
	return 0;
}

/*--------------------------------------
LUA	scn::EntUV (UV)

IN	entE
OUT	v2UV
--------------------------------------*/
int scn::EntUV(lua_State* l)
{
	vec::LuaPushVec(l, Entity::CheckLuaTo(1)->uv);
	return 2;
}

/*--------------------------------------
LUA	scn::EntSetUV (SetUV)

IN	entE, v2UV
--------------------------------------*/
int scn::EntSetUV(lua_State* l)
{
	Entity::CheckLuaTo(1)->uv = vec::LuaToVec(l, 2, 3);
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldUV (OldUV)

IN	entE
OUT	v2OldUV
--------------------------------------*/
int scn::EntOldUV(lua_State* l)
{
	vec::LuaPushVec(l, Entity::CheckLuaTo(1)->oldUV);
	return 2;
}

/*--------------------------------------
LUA	scn::EntSetOldUV (SetOldUV)

IN	entE, v2OldUV
--------------------------------------*/
int scn::EntSetOldUV(lua_State* l)
{
	Entity::CheckLuaTo(1)->oldUV = vec::LuaToVec(l, 2, 3);
	return 0;
}

/*--------------------------------------
LUA	scn::EntFinalUV (FinalUV)

IN	entE
OUT	v2FinalUV
--------------------------------------*/
int scn::EntFinalUV(lua_State* l)
{
	vec::LuaPushVec(l, Entity::CheckLuaTo(1)->FinalUV());
	return 2;
}

/*--------------------------------------
LUA	scn::EntAverage (Average)

IN	entE
OUT	v3Avg
--------------------------------------*/
int scn::EntAverage(lua_State* l)
{
	vec::LuaPushVec(l, Entity::CheckLuaTo(1)->Average());
	return 3;
}

/*--------------------------------------
LUA	scn::EntFrame (Frame)

IN	entE
OUT	nFrame
--------------------------------------*/
int scn::EntFrame(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushnumber(l, e->Frame());
	return 1;
}

/*--------------------------------------
LUA	scn::EntAnimation (Animation)

IN	entE
OUT	[aniA]
--------------------------------------*/
int scn::EntAnimation(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);

	if(e->Animation())
	{
		e->Animation()->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	scn::EntSetAnimation (SetAnimation)

IN	entE, aniA, nFrame, nTransTime
--------------------------------------*/
int scn::EntSetAnimation(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	const rnd::Animation* a = rnd::Animation::CheckLuaTo(2);
	float frame = lua_tonumber(l, 3);
	float transTime = lua_tonumber(l, 4);
	e->SetAnimation(*a, frame, transTime);
	return 0;
}

/*--------------------------------------
LUA	scn::EntClearAnimation (ClearAnimation)

IN	entE
--------------------------------------*/
int scn::EntClearAnimation(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->ClearAnimation();
	return 0;
}

/*--------------------------------------
LUA	scn::EntAnimationLoop (AnimationLoop)

IN	entE
OUT	bAniLoop
--------------------------------------*/
int scn::EntAnimationLoop(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushboolean(l, e->flags & e->LOOP_ANIM);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetAnimationLoop (SetAnimationLoop)

IN	entE, bAniLoop
--------------------------------------*/
int scn::EntSetAnimationLoop(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->flags = com::FixedBits(e->flags, e->LOOP_ANIM, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::EntAnimationSpeed (AnimationSpeed)

IN	entE
OUT	nAniSpeed
--------------------------------------*/
int scn::EntAnimationSpeed(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushnumber(l, e->animSpeed);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetAnimationSpeed (SetAnimationSpeed)

IN	entE, nAniSpeed
--------------------------------------*/
int scn::EntSetAnimationSpeed(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->animSpeed = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::EntAnimationName (AnimationName)

IN	entE
OUT	[sName]
--------------------------------------*/
int scn::EntAnimationName(lua_State* l)
{
	const rnd::Animation* a = Entity::CheckLuaTo(1)->Animation();
	a ? lua_pushstring(l, a->Name()) : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::EntAnimationPlaying (AnimationPlaying)

IN	entE
OUT	bPlaying
--------------------------------------*/
int scn::EntAnimationPlaying(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushboolean(l, e->AnimationPlaying());
	return 1;
}

/*--------------------------------------
LUA	scn::EntUpcomingFrame (UpcomingFrame)

IN	entE, nTarget
OUT	bUpcoming
--------------------------------------*/
int scn::EntUpcomingFrame(lua_State* l)
{
	lua_pushboolean(l, Entity::CheckLuaTo(1)->UpcomingFrame(luaL_checknumber(l, 2)));
	return 1;
}

/*--------------------------------------
LUA	scn::EntTransitionTime (TransitionTime)

IN	entE
OUT	nTransTime
--------------------------------------*/
int scn::EntTransitionTime(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushnumber(l, e->TransitionTime());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSubPalette (SubPalette)

IN	entE
OUT	iSubPalette
--------------------------------------*/
int scn::EntSubPalette(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushinteger(l, e->subPalette);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetSubPalette (SetSubPalette)

IN	entE, iSubPalette
--------------------------------------*/
int scn::EntSetSubPalette(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->subPalette = lua_tointeger(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::EntOpacity (Opacity)

IN	entE
OUT	nOpacity
--------------------------------------*/
int scn::EntOpacity(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushnumber(l, e->opacity);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetOpacity (SetOpacity)

IN	entE, nOpacity
--------------------------------------*/
int scn::EntSetOpacity(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->opacity = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::EntOldOpacity (OldOpacity)

IN	entE
OUT	nOldOpacity
--------------------------------------*/
int scn::EntOldOpacity(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushnumber(l, e->oldOpacity);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetOldOpacity (SetOldOpacity)

IN	entE, nOldOpacity
--------------------------------------*/
int scn::EntSetOldOpacity(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->oldOpacity = lua_tonumber(l, 2);
	return 0;
}

#define FLAG_GETTER_SETTER(flag, Flag) \
int scn::Ent##Flag(lua_State* l) \
{ \
	Entity* e = Entity::CheckLuaTo(1); \
	lua_pushboolean(l, e->flags & flag); \
	return 1; \
} \
\
int scn::EntSet##Flag(lua_State* l) \
{ \
	Entity* e = Entity::CheckLuaTo(1); \
	e->flags = com::FixedBits(e->flags, flag, lua_toboolean(l, 2) != 0); \
	return 0; \
}

FLAG_GETTER_SETTER(Entity::VISIBLE, Visible);
FLAG_GETTER_SETTER(Entity::WORLD_VISIBLE, WorldVisible);
FLAG_GETTER_SETTER(Entity::SHADOW, Shadow);

/*--------------------------------------
LUA	scn::EntSetDisplay (SetDisplay)

IN	entE, bDisplay
--------------------------------------*/
int scn::EntSetDisplay(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->flags = com::FixedBits(e->flags, Entity::VISIBLE | Entity::SHADOW,
		lua_toboolean(l, 2) != 0);
	return 0;
}

FLAG_GETTER_SETTER(Entity::REGULAR_GLASS, RegularGlass);
FLAG_GETTER_SETTER(Entity::AMBIENT_GLASS, AmbientGlass);
FLAG_GETTER_SETTER(Entity::MINIMUM_GLASS, MinimumGlass);
FLAG_GETTER_SETTER(Entity::WEAK_GLASS, WeakGlass);
FLAG_GETTER_SETTER(Entity::CLOUD, Cloud);
FLAG_GETTER_SETTER(Entity::LERP_POS, LerpPos);
FLAG_GETTER_SETTER(Entity::LERP_ORI, LerpOri);
FLAG_GETTER_SETTER(Entity::LERP_SCALE, LerpScale);
FLAG_GETTER_SETTER(Entity::LERP_UV, LerpUV);
FLAG_GETTER_SETTER(Entity::LERP_OPACITY, LerpOpacity);
FLAG_GETTER_SETTER(Entity::LERP_FRAME, LerpFrame);

/*--------------------------------------
LUA	scn::EntOrientHull (OrientHull)

IN	entE
OUT	bOrientHull
--------------------------------------*/
int scn::EntOrientHull(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushboolean(l, e->OrientHull());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetOrientHull (SetOrientHull)

IN	entE, bOrientHull
--------------------------------------*/
int scn::EntSetOrientHull(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetOrientHull(lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::EntOverlay (Overlay)

IN	entE
OUT	iOverlay
--------------------------------------*/
int scn::EntOverlay(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	lua_pushinteger(l, e->Overlay());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetOverlay (SetOverlay)

IN	entE, iOverlay
--------------------------------------*/
int scn::EntSetOverlay(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->SetOverlay(luaL_checkinteger(l, 2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntChild (Child)

IN	entE
OUT	[entChild]
--------------------------------------*/
int scn::EntChild(lua_State* l)
{
	Entity* child = Entity::CheckLuaTo(1)->Child();
	child ? child->LuaPush() : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetChild (SetChild)

IN	entE, [entChild]
--------------------------------------*/
int scn::EntSetChild(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetChild(Entity::OptionalLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntPointLink (PointLink)

IN	entE
OUT	bPointLink
--------------------------------------*/
int scn::EntPointLink(lua_State* l)
{
	lua_pushboolean(l, Entity::CheckLuaTo(1)->PointLink());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetPointLink (SetPointLink)

IN	entE, bPointLink
--------------------------------------*/
int scn::EntSetPointLink(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetPointLink(lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::EntSky (Sky)

IN	entE
OUT	bSky
--------------------------------------*/
int scn::EntSky(lua_State* l)
{
	lua_pushboolean(l, Entity::CheckLuaTo(1)->Sky());
	return 1;
}

/*--------------------------------------
LUA	scn::EntSetSky (SetSky)

IN	entE, bSky
--------------------------------------*/
int scn::EntSetSky(lua_State* l)
{
	Entity::CheckLuaTo(1)->SetSky(lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::EntMimicMesh (MimicMesh)

IN	entE, entOrig
--------------------------------------*/
int scn::EntMimicMesh(lua_State* l)
{
	Entity::CheckLuaTo(1)->MimicMesh(*Entity::CheckLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntMimicScaledPlace (MimicScaledPlace)

IN	entE, entOrig
--------------------------------------*/
int scn::EntMimicScaledPlace(lua_State* l)
{
	Entity::CheckLuaTo(1)->MimicScaledPlace(*Entity::CheckLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::EntFlagSet (FlagSet)

IN	entE
OUT	fsetF
--------------------------------------*/
int scn::EntFlagSet(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->gFlags.LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::EntPrioritize (Prioritize)

IN	entE
--------------------------------------*/
int scn::EntPrioritize(lua_State* l)
{
	Entity* e = Entity::CheckLuaTo(1);
	e->Prioritize();
	return 0;
}