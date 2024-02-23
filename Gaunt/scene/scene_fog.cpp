// scene_fog.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../../GauntCommon/json_ext.h"

namespace scn
{
	const float	FOG_DEFAULT_RADIUS = 6370000.0f,
				FOG_DEFAULT_THIN_RADIUS = 60000.0f,
				FOG_DEFAULT_SUN_RADIUS = COM_PI;
}

/*
################################################################################################


	FOG


################################################################################################
*/

const char* const res::Resource<scn::Fog>::METATABLE = "metaFog";

scn::Fog scn::fog;

/*--------------------------------------
	scn::Fog::Fog
--------------------------------------*/
scn::Fog::Fog() : radius(FOG_DEFAULT_RADIUS), thinRadius(FOG_DEFAULT_THIN_RADIUS),
	thinExponent(1.0f), geoSubStartDist(0.0f), geoSubHalfDist(0.0f),
	geoSubFactor(1.0f), geoSubPalette(0), fadeStartDist(0.0f),
	fadeHalfDist(0.0f), fadeAmount(0.0f), fadeRampPos(0.0f), fadeRampPosSun(0.0f),
	addStartDist(0.0f), addHalfDist(0.0f), addAmount(0.0f), addAmountSun(0.0f),
	sunRadius(FOG_DEFAULT_SUN_RADIUS), sunExponent(1.0f)
{
	AddLock(); // Permanent

	center = com::Vec3(0.0f, 0.0f, -radius);
}

/*--------------------------------------
	scn::Fog::TranscribeGlobals
--------------------------------------*/
void scn::Fog::TranscribeGlobals(com::PairMap<com::JSVar>& global, const char* prefix)
{
	SCN_SETUP_TRANSCRIPT_KEY_BUFFER(buf, 256, prefix, varDest, varSize);

	#define SCN_FOG_SAVE_NUMBER(nameString, member) \
		strncpy(varDest, nameString, varSize); \
		global.Ensure(buf)->Value().SetNumber(member)

	SCN_FOG_SAVE_NUMBER("Radius", radius);
	SCN_FOG_SAVE_NUMBER("ThinRadius", thinRadius);
	SCN_FOG_SAVE_NUMBER("ThinExponent", thinExponent);
	SCN_FOG_SAVE_NUMBER("GeoSubStartDist", geoSubStartDist);
	SCN_FOG_SAVE_NUMBER("GeoSubHalfDist", geoSubHalfDist);
	SCN_FOG_SAVE_NUMBER("GeoSubFactor", geoSubFactor);
	SCN_FOG_SAVE_NUMBER("GeoSubPalette", geoSubPalette);
	SCN_FOG_SAVE_NUMBER("FadeStartDist", fadeStartDist);
	SCN_FOG_SAVE_NUMBER("FadeHalfDist", fadeHalfDist);
	SCN_FOG_SAVE_NUMBER("FadeAmount", fadeAmount);
	SCN_FOG_SAVE_NUMBER("FadeRampPos", fadeRampPos);
	SCN_FOG_SAVE_NUMBER("FadeRampPosSun", fadeRampPosSun);
	SCN_FOG_SAVE_NUMBER("AddStartDist", addStartDist);
	SCN_FOG_SAVE_NUMBER("AddHalfDist", addHalfDist);
	SCN_FOG_SAVE_NUMBER("AddAmount", addAmount);
	SCN_FOG_SAVE_NUMBER("AddAmountSun", addAmountSun);
	SCN_FOG_SAVE_NUMBER("SunRadius", sunRadius);
	SCN_FOG_SAVE_NUMBER("SunExponent", sunExponent);

	#undef SCN_FOG_SAVE_NUMBER

	strncpy(varDest, "Center", varSize);
	com::JSVarSetVec(global.Ensure(buf)->Value(), center);
}

/*--------------------------------------
	scn::Fog::InterpretGlobals
--------------------------------------*/
void scn::Fog::InterpretGlobals(const com::PairMap<com::JSVar>& global, const char* prefix)
{
	SCN_SETUP_TRANSCRIPT_KEY_BUFFER(buf, 256, prefix, varDest, varSize);
	const com::Pair<com::JSVar>* pair;

	#define SCN_FOG_LOAD_NUMBER(nameString, member, def) \
		strncpy(varDest, nameString, varSize); \
		if(pair = global.Find(buf)) \
			member = pair->Value().Double(); \
		else \
			member = def

	SCN_FOG_LOAD_NUMBER("Radius", radius, FOG_DEFAULT_RADIUS);
	SCN_FOG_LOAD_NUMBER("ThinRadius", thinRadius, FOG_DEFAULT_THIN_RADIUS);
	SCN_FOG_LOAD_NUMBER("ThinExponent", thinExponent, 1.0f);
	SCN_FOG_LOAD_NUMBER("GeoSubStartDist", geoSubStartDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("GeoSubHalfDist", geoSubHalfDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("GeoSubFactor", geoSubFactor, 1.0f);
	SCN_FOG_LOAD_NUMBER("GeoSubPalette", geoSubPalette, 0);
	SCN_FOG_LOAD_NUMBER("FadeStartDist", fadeStartDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("FadeHalfDist", fadeHalfDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("FadeAmount", fadeAmount, 0.0f);
	SCN_FOG_LOAD_NUMBER("FadeRampPos", fadeRampPos, 0.0f);
	SCN_FOG_LOAD_NUMBER("FadeRampPosSun", fadeRampPosSun, 0.0f);
	SCN_FOG_LOAD_NUMBER("AddStartDist", addStartDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("AddHalfDist", addHalfDist, 0.0f);
	SCN_FOG_LOAD_NUMBER("AddAmount", addAmount, 0.0f);
	SCN_FOG_LOAD_NUMBER("AddAmountSun", addAmountSun, 0.0f);
	SCN_FOG_LOAD_NUMBER("SunRadius", sunRadius, FOG_DEFAULT_SUN_RADIUS);
	SCN_FOG_LOAD_NUMBER("SunExponent", sunExponent, 1.0f);

	#undef SCN_FOG_LOAD_NUMBER

	strncpy(varDest, "Center", varSize);
	if(pair = global.Find(buf))
		center = com::JSVarToVec3(pair->Value());
	else
		center = com::Vec3(0.0f, 0.0f, -radius);
}

/*
################################################################################################


	FOG LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::GetFog

OUT	fogGlobal
--------------------------------------*/
int scn::GetFog(lua_State* l)
{
	fog.LuaPush();
	return 1;
}

#define SCN_LUA_OBJ Fog
SCN_LUA_OBJ_VEC3(Center, center);
SCN_LUA_OBJ_NUMBER(Radius, radius);
SCN_LUA_OBJ_NUMBER(ThinRadius, thinRadius);
SCN_LUA_OBJ_NUMBER(ThinExponent, thinExponent);
SCN_LUA_OBJ_NUMBER(GeoSubStartDist, geoSubStartDist);
SCN_LUA_OBJ_NUMBER(GeoSubHalfDist, geoSubHalfDist);
SCN_LUA_OBJ_NUMBER(GeoSubFactor, geoSubFactor);
SCN_LUA_OBJ_INTEGER(GeoSubPalette, geoSubPalette);
SCN_LUA_OBJ_NUMBER(FadeStartDist, fadeStartDist);
SCN_LUA_OBJ_NUMBER(FadeHalfDist, fadeHalfDist);
SCN_LUA_OBJ_NUMBER(FadeAmount, fadeAmount);
SCN_LUA_OBJ_NUMBER(FadeRampPos, fadeRampPos);
SCN_LUA_OBJ_NUMBER(FadeRampPosSun, fadeRampPosSun);
SCN_LUA_OBJ_NUMBER(AddStartDist, addStartDist);
SCN_LUA_OBJ_NUMBER(AddHalfDist, addHalfDist);
SCN_LUA_OBJ_NUMBER(AddAmount, addAmount);
SCN_LUA_OBJ_NUMBER(AddAmountSun, addAmountSun);
SCN_LUA_OBJ_NUMBER(SunRadius, sunRadius);
SCN_LUA_OBJ_NUMBER(SunExponent, sunExponent);
#undef SCN_LUA_OBJ