// scene_bulb.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../../GauntCommon/json_ext.h"
#include "../hit/hit.h"
#include "../quaternion/qua_lua.h"
#include "../vector/vec_lua.h"

/*
################################################################################################


	BULB


################################################################################################
*/

const char* const res::Resource<scn::Bulb>::METATABLE = "metaBulb";

/*--------------------------------------
	scn::Bulb::Bulb
--------------------------------------*/
scn::Bulb::Bulb(const com::Vec3& pos, float radius, float exponent, float intensity,
	int subPalette, const com::Qua& ori, float outer, float inner)
	: Light(intensity, subPalette), exponent(exponent), ori(ori), outer(outer), inner(inner)
{
	flags = LERP_INTENSITY | LERP_POS | LERP_RADIUS | LERP_EXPONENT | LERP_ORI | LERP_CUTOFF;
	drawCode = 0;
	zoneLinks.Init(DEF_NUM_ZONE_LINKS_ALLOC);
	numZoneLinks = 0;
	SetPlace(pos, radius);
	oldPos = Pos();
	oldRadius = Radius();
	oldExponent = exponent;
	oldOri = ori;
	oldOuter = outer;
	oldInner = inner;
	AddLock(); // Permanent lock
	EnsureLink();
}

/*--------------------------------------
	scn::Bulb::~Bulb
--------------------------------------*/
scn::Bulb::~Bulb()
{
	UnlinkWorld();
	zoneLinks.Free();
}

/*--------------------------------------
	scn::Bulb::SetPos
--------------------------------------*/
void scn::Bulb::SetPos(const com::Vec3& p)
{
	pos = p;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Bulb::FinalPos
--------------------------------------*/
com::Vec3 scn::Bulb::FinalPos() const
{
	if(flags & LERP_POS)
		return COM_LERP(oldPos, pos, wrp::Fraction());
	else
		return pos;
}

/*--------------------------------------
	scn::Bulb::SetRadius
--------------------------------------*/
void scn::Bulb::SetRadius(float r)
{
	radius = r;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Bulb::FinalRadius
--------------------------------------*/
float scn::Bulb::FinalRadius() const
{
	if(flags & LERP_RADIUS)
		return COM_LERP(oldRadius, radius, wrp::Fraction());
	else
		return radius;
}

/*--------------------------------------
	scn::Bulb::FinalExponent
--------------------------------------*/
float scn::Bulb::FinalExponent() const
{
	if(flags & LERP_EXPONENT)
		return COM_LERP(oldExponent, exponent, wrp::Fraction());
	else
		return exponent;
}

/*--------------------------------------
	scn::Bulb::FinalOri
--------------------------------------*/
com::Qua scn::Bulb::FinalOri() const
{
	if(flags & LERP_ORI && oldOri != ori)
		return com::Slerp(oldOri, ori, wrp::Fraction()).Normalized();
	else
		return ori;
}

/*--------------------------------------
	scn::Bulb::FinalOuter
--------------------------------------*/
float scn::Bulb::FinalOuter() const
{
	if(flags & LERP_CUTOFF)
		return COM_LERP(oldOuter, outer, wrp::Fraction());
	else
		return outer;
}

/*--------------------------------------
	scn::Bulb::FinalInner
--------------------------------------*/
float scn::Bulb::FinalInner() const
{
	if(flags & LERP_CUTOFF)
		return COM_LERP(oldInner, inner, wrp::Fraction());
	else
		return inner;
}

/*--------------------------------------
	scn::Bulb::SetPlace
--------------------------------------*/
void scn::Bulb::SetPlace(const com::Vec3& p, float r)
{
	pos = p;
	radius = r;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Bulb::SaveOld
--------------------------------------*/
void scn::Bulb::SaveOld()
{
	oldIntensity = intensity;
	oldPos = pos;
	oldRadius = radius;
	oldExponent = exponent;
	oldOri = ori;
	oldOuter = outer;
	oldInner = inner;
}

/*--------------------------------------
	scn::Bulb::Transcribe
--------------------------------------*/
void scn::Bulb::Transcribe()
{
	if(!Transcript())
		return;

	com::PairMap<com::JSVar>& pairs = *Transcript()->SetObject();

	pairs.Ensure("intensity")->Value().SetNumber(intensity);

	if(oldIntensity != intensity)
		pairs.Ensure("oldIntensity")->Value().SetNumber(oldIntensity);

	if(subPalette)
		pairs.Ensure("subPalette")->Value().SetNumber(subPalette);

	com::JSVarSetVec(pairs.Ensure("pos")->Value(), Pos());

	if(oldPos != Pos())
		com::JSVarSetVec(pairs.Ensure("oldPos")->Value(), oldPos);

	pairs.Ensure("radius")->Value().SetNumber(Radius());

	if(oldRadius != Radius())
		pairs.Ensure("oldRadius")->Value().SetNumber(oldRadius);

	pairs.Ensure("exponent")->Value().SetNumber(exponent);

	if(oldExponent != exponent)
		pairs.Ensure("oldExponent")->Value().SetNumber(oldExponent);

	com::JSVarSetQua(pairs.Ensure("ori")->Value(), ori);

	if(oldOri != ori)
		com::JSVarSetQua(pairs.Ensure("oldOri")->Value(), oldOri);

	if(outer != COM_PI)
		pairs.Ensure("outer")->Value().SetNumber(outer);

	if(oldOuter != outer)
		pairs.Ensure("oldOuter")->Value().SetNumber(oldOuter);

	if(inner != COM_PI)
		pairs.Ensure("inner")->Value().SetNumber(inner);

	if(oldInner != inner)
		pairs.Ensure("oldInner")->Value().SetNumber(oldInner);

	if(colorSmooth != 1.0f)
		pairs.Ensure("colorSmooth")->Value().SetNumber(colorSmooth);

	if(colorPriority != 1.0f)
		pairs.Ensure("colorPriority")->Value().SetNumber(colorPriority);

	// FIXME: transcribe flags as one integer
	if(!(flags & LERP_INTENSITY))
		pairs.Ensure("lerpIntensity")->Value().SetBool((flags & LERP_INTENSITY) != 0);

	if(!(flags & LERP_POS))
		pairs.Ensure("lerpPos")->Value().SetBool((flags & LERP_POS) != 0);

	if(!(flags & LERP_RADIUS))
		pairs.Ensure("lerpRadius")->Value().SetBool((flags & LERP_RADIUS) != 0);

	if(!(flags & LERP_EXPONENT))
		pairs.Ensure("lerpExponent")->Value().SetBool((flags & LERP_EXPONENT) != 0);

	if(!(flags & LERP_ORI))
		pairs.Ensure("lerpOri")->Value().SetBool((flags & LERP_ORI) != 0);

	if(!(flags & LERP_CUTOFF))
		pairs.Ensure("lerpCutoff")->Value().SetBool((flags & LERP_CUTOFF) != 0);
}

/*--------------------------------------
	scn::Bulb::InterpretTranscript
--------------------------------------*/
void scn::Bulb::InterpretTranscript()
{
	com::PairMap<com::JSVar>* pairs;

	if(!Transcript() || !(pairs = Transcript()->Object()))
		return;

	com::Pair<com::JSVar>* pair;
	com::Vec3 pos = 0.0f;
	float radius = 0.0f;

	if(pair = pairs->Find("intensity"))
		intensity = pair->Value().Float();

	if(pair = pairs->Find("oldIntensity"))
		oldIntensity = pair->Value().Float();
	else
		oldIntensity = intensity;

	if(pair = pairs->Find("subPalette"))
		subPalette = pair->Value().Int();

	if(pair = pairs->Find("pos"))
		pos = com::JSVarToVec3(pair->Value());

	if(pair = pairs->Find("radius"))
		radius = pair->Value().Float();

	SetPlace(pos, radius);

	if(pair = pairs->Find("oldPos"))
		oldPos = com::JSVarToVec3(pair->Value());
	else
		oldPos = Pos();

	if(pair = pairs->Find("oldRadius"))
		oldRadius = pair->Value().Float();
	else
		oldRadius = Radius();

	if(pair = pairs->Find("exponent"))
		exponent = pair->Value().Float();

	if(pair = pairs->Find("oldExponent"))
		oldExponent = pair->Value().Float();
	else
		oldExponent = exponent;

	if(pair = pairs->Find("ori"))
		ori = com::JSVarToQua(pair->Value());

	if(pair = pairs->Find("oldOri"))
		oldOri = com::JSVarToQua(pair->Value());
	else
		oldOri = ori;

	if(pair = pairs->Find("outer"))
		outer = pair->Value().Float();

	if(pair = pairs->Find("oldOuter"))
		oldOuter = pair->Value().Float();
	else
		oldOuter = outer;

	if(pair = pairs->Find("inner"))
		inner = pair->Value().Float();

	if(pair = pairs->Find("oldInner"))
		oldInner = pair->Value().Float();
	else
		oldInner = inner;

	if(pair = pairs->Find("colorSmooth"))
		colorSmooth = pair->Value().Float();

	if(pair = pairs->Find("colorPriority"))
		colorPriority = pair->Value().Float();

	if(pair = pairs->Find("lerpIntensity"))
		flags = com::FixedBits(flags, LERP_INTENSITY, pair->Value().Bool());

	if(pair = pairs->Find("lerpPos"))
		flags = com::FixedBits(flags, LERP_POS, pair->Value().Bool());

	if(pair = pairs->Find("lerpRadius"))
		flags = com::FixedBits(flags, LERP_RADIUS, pair->Value().Bool());

	if(pair = pairs->Find("lerpExponent"))
		flags = com::FixedBits(flags, LERP_EXPONENT, pair->Value().Bool());

	if(pair = pairs->Find("lerpOri"))
		flags = com::FixedBits(flags, LERP_ORI, pair->Value().Bool());

	if(pair = pairs->Find("lerpCutoff"))
		flags = com::FixedBits(flags, LERP_CUTOFF, pair->Value().Bool());
}

/*--------------------------------------
	scn::Bulb::LinkWorld
--------------------------------------*/
void scn::Bulb::LinkWorld()
{
	if(radius <= 0.0f)
		return; // FIXME: radius lerp to 0 won't show if oldRadius isn't taken into account here

#if DRAW_POINT_SHADOWS
	const WorldNode* leaf = (const WorldNode*)PosToLeaf(WorldRoot(), pos);

	if(!leaf || !leaf->zone)
		return;
	
	float radSq = radius * radius;
	unsigned zoneCode = IncZoneDrawCode(); // FIXME: rename draw code to be more general
	LinkZone(*leaf->zone);
	leaf->zone->drawCode = zoneCode;

	// Flood through in-range portals
	for(size_t z = numZoneLinks - 1; z < numZoneLinks; z++)
	{
		Zone& zone = *zoneLinks[z].obj;

		for(size_t p = 0; p < zone.numPortals; p++)
		{
			const PortalSet& set = *zone.portals[p];
			Zone& other = *set.Other(zone);

			if(other.drawCode == zoneCode)
				continue; // Already linked this zone

			const com::Plane& pln = set.polys[0].pln;
			float dist = com::Dot(pos, pln.normal) - pln.offset;

			if(&other == set.front)
				dist = -dist;

			if(dist < 0.0f || dist > radius)
				continue; // Behind plane or too far

			// In range of plane, check polygons
			size_t l = 0;
			for(; l < set.numPolys; l++)
			{
				const com::Poly& poly = set.polys[l];

				if(poly.InteriorProjection(pos, 0.0f))
					break; // Projected position is on polygon, plane dist is portal dist

				size_t v = 0;
				for(; v < poly.numVerts; v++)
				{
					size_t next = (v + 1) % poly.numVerts;
					com::Vec3 pt = com::ClosestSegmentPoint(poly.verts[v], poly.verts[next], pos);
					float distSq = (pos - pt).MagSq();

					if(distSq < radSq)
						break; // Portal's edge is in range
				}

				if(v != poly.numVerts)
					break;
			}

			if(l == set.numPolys)
				continue;

			LinkZone(other);
			other.drawCode = zoneCode;
		}
	}
#else
	unsigned zoneCode = IncZoneDrawCode();
	hit::Descent& d = hit::GlobalDescent();
	d.Begin(WorldRoot(), pos, pos);

	while(d.numStacked)
	{
		Zone* zone = ((WorldNode&)*d.s[d.numStacked - 1].node).zone;

		if(zone && zone->drawCode != zoneCode)
		{
			LinkZone(*zone);
			zone->drawCode = zoneCode;
		}

		d.SphereDescend(radius);
	}
#endif
}

/*--------------------------------------
	scn::Bulb::UnlinkWorld
--------------------------------------*/
void scn::Bulb::UnlinkWorld()
{
	UnlinkObjects<Bulb, Zone>(*this, &Bulb::zoneLinks, &Bulb::numZoneLinks, &Zone::bulbLinks,
		&Zone::numBulbLinks);
}

/*--------------------------------------
	scn::Bulb::LinkZone
--------------------------------------*/
void scn::Bulb::LinkZone(Zone& zone)
{
	LinkObject<Bulb, Zone>(*this, &Bulb::zoneLinks, &Bulb::numZoneLinks, zone, &Zone::bulbLinks,
		&Zone::numBulbLinks);
}

/*--------------------------------------
	scn::CreateBulb
--------------------------------------*/
scn::Bulb* scn::CreateBulb(const com::Vec3& pos, float radius, float exponent, float intensity,
	int subPalette, const com::Qua& ori, float outer, float inner)
{
	return new Bulb(pos, radius, exponent, intensity, subPalette, ori, outer, inner);
}

/*--------------------------------------
	scn::DeleteBulb
--------------------------------------*/
void scn::DeleteBulb(scn::Bulb* bulb)
{
	delete bulb;
}

/*--------------------------------------
	scn::ClearBulbs
--------------------------------------*/
void scn::ClearBulbs()
{
	while(Bulb::List().f)
		DeleteBulb(Bulb::List().f->o);
}

/*--------------------------------------
	scn::InterpretBulbs
--------------------------------------*/
void scn::InterpretBulbs(com::PairMap<com::JSVar>& bulbs)
{
	for(com::Pair<com::JSVar>* it = bulbs.First(); it; it = it->Next())
	{
		scn::Bulb& blb = *scn::CreateBulb(0.0f, 0.0f, 2.0f, 1.0f, 0);
		blb.SetRecordID(it->Key(), true);
		blb.SetTranscriptPtr(&it->Value());
		blb.InterpretTranscript();
	}
}

/*
################################################################################################


	BULB LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::CreateBulb

IN	v3Pos, nRadius, nExponent, nIntensity, iSubPalette, [q4Ori, nOuter, nInner]
OUT	blb
--------------------------------------*/
int scn::CreateBulb(lua_State* l)
{
	if(lua_gettop(l) <= 7)
	{
		CreateBulb(vec::LuaToVec(l, 1, 2, 3), lua_tonumber(l, 4), lua_tonumber(l, 5),
			lua_tonumber(l, 6), lua_tointeger(l, 7))->LuaPush();
	}
	else
	{
		CreateBulb(vec::LuaToVec(l, 1, 2, 3), lua_tonumber(l, 4), lua_tonumber(l, 5),
			lua_tonumber(l, 6), lua_tointeger(l, 7), qua::LuaToQua(l, 8, 9, 10, 11),
			lua_tonumber(l, 12), lua_tonumber(l, 13))->LuaPush();
	}

	return 1;
}

/*--------------------------------------
LUA	scn::BlbDelete (Delete)

IN	blbB
--------------------------------------*/
int scn::BlbDelete(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	DeleteBulb(b);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbPos (Pos)

IN	blbB
OUT	v3Pos
--------------------------------------*/
int scn::BlbPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	vec::LuaPushVec(l, b->Pos());
	return 3;
}

/*--------------------------------------
LUA	scn::BlbSetPos (SetPos)

IN	blbB, v3Pos
--------------------------------------*/
int scn::BlbSetPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->SetPos(vec::LuaToVec(l, 2, 3, 4));
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldPos (OldPos)

IN	blbB
OUT	v3OldPos
--------------------------------------*/
int scn::BlbOldPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	vec::LuaPushVec(l, b->oldPos);
	return 3;
}

/*--------------------------------------
LUA	scn::BlbSetOldPos (SetOldPos)

IN	blbB, v3OldPos
--------------------------------------*/
int scn::BlbSetOldPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldPos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalPos (FinalPos)

IN	blbB
OUT	v3FinalPos
--------------------------------------*/
int scn::BlbFinalPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	vec::LuaPushVec(l, b->FinalPos());
	return 3;
}

/*--------------------------------------
LUA	scn::BlbRadius (Radius)

IN	blbB
OUT	nRadius
--------------------------------------*/
int scn::BlbRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->Radius());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetRadius (SetRadius)

IN	blbB, nRadius
--------------------------------------*/
int scn::BlbSetRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->SetRadius(lua_tonumber(l, 2));
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldRadius (OldRadius)

IN	blbB
OUT	nOldRadius
--------------------------------------*/
int scn::BlbOldRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->oldRadius);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOldRadius (SetOldRadius)

IN	blbB, nOldRadius
--------------------------------------*/
int scn::BlbSetOldRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldRadius = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalRadius (FinalRadius)

IN	blbB
OUT	nFinalRadius
--------------------------------------*/
int scn::BlbFinalRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->FinalRadius());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbExponent (Exponent)

IN	blbB
OUT	nExponent
--------------------------------------*/
int scn::BlbExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->exponent);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetExponent (SetExponent)

IN	blbB, nExponent
--------------------------------------*/
int scn::BlbSetExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->exponent = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldExponent (OldExponent)

IN	blbB
OUT	nOldExponent
--------------------------------------*/
int scn::BlbOldExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->oldExponent);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOldExponent (SetOldExponent)

IN	blbB, nOldExponent
--------------------------------------*/
int scn::BlbSetOldExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldExponent = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalExponent (FinalExponent)

IN	blbB
OUT	nFinalExponent
--------------------------------------*/
int scn::BlbFinalExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->FinalExponent());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbOri (Ori)

IN	blbB
OUT	q4Ori
--------------------------------------*/
int scn::BlbOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	qua::LuaPushQua(l, b->ori);
	return 4;
}

/*--------------------------------------
LUA	scn::BlbSetOri (SetOri)

IN	blbB, q4Ori
--------------------------------------*/
int scn::BlbSetOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->ori = qua::LuaToQua(l, 2, 3, 4, 5);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldOri (OldOri)

IN	blbB
OUT	q4OldOri
--------------------------------------*/
int scn::BlbOldOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	qua::LuaPushQua(l, b->oldOri);
	return 4;
}

/*--------------------------------------
LUA	scn::BlbSetOldOri (SetOldOri)

IN	blbB, q4OldOri
--------------------------------------*/
int scn::BlbSetOldOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldOri = qua::LuaToQua(l, 2, 3, 4, 5);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalOri (FinalOri)

IN	blbB
OUT	q4FinOri
--------------------------------------*/
int scn::BlbFinalOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	qua::LuaPushQua(l, b->FinalOri());
	return 4;
}

/*--------------------------------------
LUA	scn::BlbPitch (Pitch)

IN	blbB
OUT	nPitch
--------------------------------------*/
int scn::BlbPitch(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->ori.Pitch());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetPitch (SetPitch)

IN	blbB, nPitch
--------------------------------------*/
int scn::BlbSetPitch(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->ori = com::QuaEulerPitchYaw(luaL_checknumber(l, 2), b->ori.Yaw());
	return 0;
}

/*--------------------------------------
LUA	scn::BlbYaw (Yaw)

IN	blbB
OUT	nYaw
--------------------------------------*/
int scn::BlbYaw(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->ori.Yaw());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetYaw (SetYaw)

IN	blbB, nYaw
--------------------------------------*/
int scn::BlbSetYaw(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->ori = com::QuaEulerPitchYaw(b->ori.Pitch(), luaL_checknumber(l, 2));
	return 0;
}

/*--------------------------------------
LUA	scn::BlbCutoff (Cutoff)

IN	blbB
OUT	nOuter, nInner
--------------------------------------*/
int scn::BlbCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->outer);
	lua_pushnumber(l, b->inner);
	return 2;
}

/*--------------------------------------
LUA	scn::BlbSetCutoff (SetCutoff)

IN	blbB, nOuter, nInner
--------------------------------------*/
int scn::BlbSetCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->outer = luaL_checknumber(l, 2);
	b->inner = luaL_checknumber(l, 3);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldCutoff (OldCutoff)

IN	blbB
OUT	nOldOuter, nOldInner
--------------------------------------*/
int scn::BlbOldCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->oldOuter);
	lua_pushnumber(l, b->oldInner);
	return 2;
}

/*--------------------------------------
LUA	scn::BlbSetOldCutoff (SetOldCutoff)

IN	blbB, nOldOuter, nOldInner
--------------------------------------*/
int scn::BlbSetOldCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldOuter = luaL_checknumber(l, 2);
	b->oldInner = luaL_checknumber(l, 3);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalCutoff (FinalCutoff)

IN	blbB
OUT	nFinalOuter, nFinalInner
--------------------------------------*/
int scn::BlbFinalCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->FinalOuter());
	lua_pushnumber(l, b->FinalInner());
	return 2;
}

/*--------------------------------------
LUA	scn::BlbOuter (Outer)

IN	blbB
OUT	nOuter
--------------------------------------*/
int scn::BlbOuter(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->outer);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOuter (SetOuter)

IN	blbB, nOuter
--------------------------------------*/
int scn::BlbSetOuter(lua_State* l)
{
	Bulb::CheckLuaTo(1)->outer = luaL_checknumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldOuter (OldOuter)

IN	blbB
OUT	nOldOuter
--------------------------------------*/
int scn::BlbOldOuter(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->oldOuter);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOldOuter (SetOldOuter)

IN	blbB, nOldOuter
--------------------------------------*/
int scn::BlbSetOldOuter(lua_State* l)
{
	Bulb::CheckLuaTo(1)->oldOuter = luaL_checknumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalOuter (FinalOuter)

IN	blbB
OUT	nFinalOuter
--------------------------------------*/
int scn::BlbFinalOuter(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->FinalOuter());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbInner (Inner)

IN	blbB
OUT	nInner
--------------------------------------*/
int scn::BlbInner(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->inner);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetInner (SetInner)

IN	blbB, nInner
--------------------------------------*/
int scn::BlbSetInner(lua_State* l)
{
	Bulb::CheckLuaTo(1)->inner = luaL_checknumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldInner (OldInner)

IN	blbB
OUT	nOldInner
--------------------------------------*/
int scn::BlbOldInner(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->oldInner);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOldInner (SetOldInner)

IN	blbB, nOldInner
--------------------------------------*/
int scn::BlbSetOldInner(lua_State* l)
{
	Bulb::CheckLuaTo(1)->oldInner = luaL_checknumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalInner (FinalInner)

IN	blbB
OUT	nFinalInner
--------------------------------------*/
int scn::BlbFinalInner(lua_State* l)
{
	lua_pushnumber(l, Bulb::CheckLuaTo(1)->FinalInner());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetPlace (SetPlace)

IN	blbB, v3Pos, nRadius
--------------------------------------*/
int scn::BlbSetPlace(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->SetPlace(vec::LuaToVec(l, 2, 3, 4), lua_tonumber(l, 5));
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalPlace (FinalPlace)

IN	blbB
OUT	v3FinalPos, nFinalRadius
--------------------------------------*/
int scn::BlbFinalPlace(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	vec::LuaPushVec(l, b->FinalPos());
	lua_pushnumber(l, b->FinalRadius());
	return 4;
}

/*--------------------------------------
LUA	scn::BlbIntensity (Intensity)

IN	blbB
OUT	nIntensity
--------------------------------------*/
int scn::BlbIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->intensity);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetIntensity (SetIntensity)

IN	blbB, nIntensity
--------------------------------------*/
int scn::BlbSetIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->intensity = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbOldIntensity (OldIntensity)

IN	blbB
OUT	nOldIntensity
--------------------------------------*/
int scn::BlbOldIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->oldIntensity);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetOldIntensity (SetOldIntensity)

IN	blbB, nOldIntensity
--------------------------------------*/
int scn::BlbSetOldIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->oldIntensity = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbFinalIntensity (FinalIntensity)

IN	blbB
OUT	nFinalIntensity
--------------------------------------*/
int scn::BlbFinalIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->FinalIntensity());
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSubPalette (SubPalette)

IN	blbB
OUT	iSubPalette
--------------------------------------*/
int scn::BlbSubPalette(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushinteger(l, b->subPalette);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetSubPalette (SetSubPalette)

IN	blbB, iSubPalette
--------------------------------------*/
int scn::BlbSetSubPalette(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->subPalette = lua_tointeger(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbColorSmooth (ColorSmooth)

IN	blbB
OUT	nColorSmooth
--------------------------------------*/
int scn::BlbColorSmooth(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->colorSmooth);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetColorSmooth (SetColorSmooth)

IN	blbB, nColorSmooth
--------------------------------------*/
int scn::BlbSetColorSmooth(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->colorSmooth = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbColorPriority (ColorPriority)

IN	blbB
OUT	nColorPriority
--------------------------------------*/
int scn::BlbColorPriority(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushnumber(l, b->colorPriority);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetColorPriority (SetColorPriority)

IN	blbB, nColorPriority
--------------------------------------*/
int scn::BlbSetColorPriority(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->colorPriority = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpIntensity (LerpIntensity)

IN	blbB
OUT	bLerpIntensity
--------------------------------------*/
int scn::BlbLerpIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_INTENSITY);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpIntensity (SetLerpIntensity)

IN	blbB
OUT	bLerpIntensity
--------------------------------------*/
int scn::BlbSetLerpIntensity(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_INTENSITY, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpPos (LerpPos)

IN	blbB
OUT	bLerpPos
--------------------------------------*/
int scn::BlbLerpPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_POS);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpPos (SetLerpPos)

IN	blbB, bLerpPos
--------------------------------------*/
int scn::BlbSetLerpPos(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_POS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpRadius (LerpRadius)

IN	blbB
OUT	bLerpRadius
--------------------------------------*/
int scn::BlbLerpRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_RADIUS);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpRadius (SetLerpRadius)

IN	blbB, bLerpRadius
--------------------------------------*/
int scn::BlbSetLerpRadius(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_RADIUS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpExponent (LerpExponent)

IN	blbB
OUT	bLerpExponent
--------------------------------------*/
int scn::BlbLerpExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_EXPONENT);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpExponent (SetLerpExponent)

IN	blbB, bLerpExponent
--------------------------------------*/
int scn::BlbSetLerpExponent(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_EXPONENT, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpOri (LerpOri)

IN	blbB
OUT	bLerpOri
--------------------------------------*/
int scn::BlbLerpOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_ORI);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpOri (SetLerpOri)

IN	blbB, bLerpOri
--------------------------------------*/
int scn::BlbSetLerpOri(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_ORI, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::BlbLerpCutoff (LerpCutoff)

IN	blbB
OUT	bLerpCutoff
--------------------------------------*/
int scn::BlbLerpCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	lua_pushboolean(l, b->flags & Bulb::LERP_CUTOFF);
	return 1;
}

/*--------------------------------------
LUA	scn::BlbSetLerpCutoff (SetLerpCutoff)

IN	blbB, bLerpCutoff
--------------------------------------*/
int scn::BlbSetLerpCutoff(lua_State* l)
{
	Bulb* b = Bulb::CheckLuaTo(1);
	b->flags = com::FixedBits(b->flags, Bulb::LERP_CUTOFF, lua_toboolean(l, 2) != 0);
	return 0;
}