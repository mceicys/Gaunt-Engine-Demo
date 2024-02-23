// scene_light.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../vector/vec_lua.h"

/*
################################################################################################


	LIGHT


################################################################################################
*/

scn::Light	scn::ambient(0.0f, 0);
scn::Light	scn::sky(1.0f, 0);
scn::Light	scn::cloud(0.0f, 0);
scn::Sun	scn::sun(1.0f, 0.0f, 0);

/*--------------------------------------
	scn::Light::Light
--------------------------------------*/
scn::Light::Light(float intensity, int subPalette) : flags(LERP_INTENSITY),
	intensity(intensity), oldIntensity(intensity), subPalette(subPalette), colorSmooth(1.0f),
	colorPriority(1.0f)
{}

/*--------------------------------------
	scn::Light::FinalIntensity
--------------------------------------*/
float scn::Light::FinalIntensity() const
{
	if(flags & LERP_INTENSITY)
		return COM_LERP(oldIntensity, intensity, wrp::Fraction());
	else
		return intensity;
}

/*--------------------------------------
	scn::Light::TranscribeGlobals
--------------------------------------*/
void scn::Light::TranscribeGlobals(com::PairMap<com::JSVar>& global, const char* prefix)
{
	SCN_SETUP_TRANSCRIPT_KEY_BUFFER(buf, 256, prefix, varDest, varSize);

	strncpy(varDest, "Intensity", varSize);
	global.Ensure(buf)->Value().SetNumber(intensity);

	if(oldIntensity != intensity)
	{
		strncpy(varDest, "OldIntensity", varSize);
		global.Ensure(buf)->Value().SetNumber(oldIntensity);
	}

	strncpy(varDest, "SubPalette", varSize);
	global.Ensure(buf)->Value().SetNumber(subPalette);

	if(colorSmooth != 1.0f)
	{
		strncpy(varDest, "ColorSmooth", varSize);
		global.Ensure(buf)->Value().SetNumber(colorSmooth);
	}

	if(colorPriority != 1.0f)
	{
		strncpy(varDest, "ColorPriority", varSize);
		global.Ensure(buf)->Value().SetNumber(colorPriority);
	}

	if(!(flags & LERP_INTENSITY))
	{
		strncpy(varDest, "LerpIntensity", varSize);
		global.Ensure(buf)->Value().SetBool(flags & LERP_INTENSITY);
	}
}

/*--------------------------------------
	scn::Light::InterpretGlobals
--------------------------------------*/
void scn::Light::InterpretGlobals(const com::PairMap<com::JSVar>& global, const char* prefix,
	float defIntensity)
{
	SCN_SETUP_TRANSCRIPT_KEY_BUFFER(buf, 256, prefix, varDest, varSize);
	const com::Pair<com::JSVar>* pair;

	strncpy(varDest, "Intensity", varSize);
	if(pair = global.Find(buf))
		intensity = pair->Value().Double();
	else
		intensity = defIntensity;

	strncpy(varDest, "OldIntensity", varSize);
	if(pair = global.Find(buf))
		oldIntensity = pair->Value().Double();
	else
		oldIntensity = intensity;

	strncpy(varDest, "SubPalette", varSize);
	if(pair = global.Find(buf))
		subPalette = pair->Value().Int();
	else
		subPalette = 0;

	strncpy(varDest, "ColorSmooth", varSize);
	if(pair = global.Find(buf))
		colorSmooth = pair->Value().Float();
	else
		colorSmooth = 1.0f;

	strncpy(varDest, "ColorPriority", varSize);
	if(pair = global.Find(buf))
		colorPriority = pair->Value().Float();
	else
		colorPriority = 1.0f;
}

/*--------------------------------------
	scn::Sun::Sun
--------------------------------------*/
scn::Sun::Sun(const com::Vec3& pos, float intensity, int subPalette)
	: Light(intensity, subPalette), pos(pos), oldPos(pos)
{
	flags = LERP_INTENSITY | LERP_POS;
}

/*--------------------------------------
	scn::Sun::FinalPos
--------------------------------------*/
com::Vec3 scn::Sun::FinalPos() const
{
	if(flags & LERP_POS)
		return COM_LERP(oldPos, pos, wrp::Fraction());
	else
		return pos;
}

/*
################################################################################################


	LIGHT LUA


################################################################################################
*/

// FIXME HACK: make these lights objects with permanent userdata
#define GLOBAL_LIGHT_LUA_DEFINITIONS(obj, Obj) \
int scn::##Obj##Intensity(lua_State* l){lua_pushnumber(l, obj.intensity); return 1;} \
int scn::##Obj##SetIntensity(lua_State* l){obj.intensity = lua_tonumber(l, 1); return 0;} \
int scn::##Obj##OldIntensity(lua_State* l){lua_pushnumber(l, obj.oldIntensity); return 1;} \
int scn::##Obj##SetOldIntensity(lua_State* l){obj.oldIntensity = lua_tonumber(l, 1); return 0;} \
\
int scn::##Obj##FinalIntensity(lua_State* l) \
{ \
	lua_pushnumber(l, obj.FinalIntensity()); \
	return 1; \
} \
\
int scn::##Obj##SubPalette(lua_State* l){lua_pushinteger(l, obj.subPalette); return 1;} \
\
int scn::##Obj##SetSubPalette(lua_State* l) \
{ \
	obj.subPalette = lua_tointeger(l, 1); \
	return 0; \
} \
\
int scn::##Obj##ColorSmooth(lua_State* l){lua_pushnumber(l, obj.colorSmooth); return 1;} \
int scn::##Obj##SetColorSmooth(lua_State* l){obj.colorSmooth = lua_tonumber(l, 1); return 0;} \
int scn::##Obj##ColorPriority(lua_State* l){lua_pushnumber(l, obj.colorPriority); return 1;} \
int scn::##Obj##SetColorPriority(lua_State* l){obj.colorPriority = lua_tonumber(l, 1); return 0;} \
\
int scn::##Obj##LerpIntensity(lua_State* l) \
{ \
	lua_pushboolean(l, obj.flags & obj.LERP_INTENSITY); \
	return 1; \
} \
\
int scn::##Obj##SetLerpIntensity(lua_State* l) \
{ \
	obj.flags = com::FixedBits(obj.flags, obj.LERP_INTENSITY, lua_toboolean(l, 1) != 0); \
	return 0; \
}

GLOBAL_LIGHT_LUA_DEFINITIONS(ambient, Ambient);
GLOBAL_LIGHT_LUA_DEFINITIONS(sky, Sky);
GLOBAL_LIGHT_LUA_DEFINITIONS(cloud, Cloud);

#if 0
/*--------------------------------------
LUA	scn::AmbientIntensity

OUT	nAmbientIntensity
--------------------------------------*/
int scn::AmbientIntensity(lua_State* l){lua_pushnumber(l, ambient.intensity); return 1;}

/*--------------------------------------
LUA	scn::AmbientSetIntensity

IN	nAmbientIntensity
--------------------------------------*/
int scn::AmbientSetIntensity(lua_State* l){ambient.intensity = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::AmbientOldIntensity

OUT	nAmbientOldIntensity
--------------------------------------*/
int scn::AmbientOldIntensity(lua_State* l){lua_pushnumber(l, ambient.oldIntensity); return 1;}

/*--------------------------------------
LUA	scn::AmbientSetOldIntensity

IN	nAmbientOldIntensity
--------------------------------------*/
int scn::AmbientSetOldIntensity(lua_State* l){ambient.oldIntensity = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::AmbientFinalIntensity

OUT	nAmbientFinalIntensity
--------------------------------------*/
int scn::AmbientFinalIntensity(lua_State* l)
{
	lua_pushnumber(l, ambient.FinalIntensity());
	return 1;
}

/*--------------------------------------
LUA	scn::AmbientSubPalette

OUT	iAmbientSubPalette
--------------------------------------*/
int scn::AmbientSubPalette(lua_State* l){lua_pushinteger(l, ambient.subPalette); return 1;}

/*--------------------------------------
LUA	scn::AmbientSetSubPalette

IN	iAmbientSubPalette
--------------------------------------*/
int scn::AmbientSetSubPalette(lua_State* l)
{
	ambient.subPalette = lua_tointeger(l, 1);
	return 0;
}

/*--------------------------------------
LUA	scn::AmbientColorSmooth

OUT	nAmbientColorSmooth
--------------------------------------*/
int scn::AmbientColorSmooth(lua_State* l){lua_pushnumber(l, ambient.colorSmooth); return 1;}

/*--------------------------------------
LUA	scn::AmbientSetColorSmooth

IN	nAmbientColorSmooth
--------------------------------------*/
int scn::AmbientSetColorSmooth(lua_State* l){ambient.colorSmooth = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::AmbientColorPriority

OUT	nAmbientColorPriority
--------------------------------------*/
int scn::AmbientColorPriority(lua_State* l){lua_pushnumber(l, ambient.colorPriority); return 1;}

/*--------------------------------------
LUA	scn::AmbientSetColorPriority

IN	nAmbientColorPriority
--------------------------------------*/
int scn::AmbientSetColorPriority(lua_State* l){ambient.colorPriority = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::AmbientLerpIntensity

OUT	bAmbientLerpIntensity
--------------------------------------*/
int scn::AmbientLerpIntensity(lua_State* l)
{
	lua_pushboolean(l, ambient.flags & ambient.LERP_INTENSITY);
	return 1;
}

/*--------------------------------------
LUA	scn::AmbientSetLerpIntensity

IN	bAmbientLerpIntensity
--------------------------------------*/
int scn::AmbientSetLerpIntensity(lua_State* l)
{
	ambient.flags = com::FixedBits(ambient.flags, ambient.LERP_INTENSITY,
		lua_toboolean(l, 1) != 0);
	return 0;
}
#endif

/*--------------------------------------
LUA	scn::SunPos

OUT	v3SunPos
--------------------------------------*/
int scn::SunPos(lua_State* l){vec::LuaPushVec(l, sun.pos); return 3;}

/*--------------------------------------
LUA	scn::SunSetPos

IN	v3SunPos
--------------------------------------*/
int scn::SunSetPos(lua_State* l){sun.pos = vec::LuaToVec(l, 1, 2, 3); return 0;}

/*--------------------------------------
LUA	scn::SunOldPos

OUT	v3SunOldPos
--------------------------------------*/
int scn::SunOldPos(lua_State* l){vec::LuaPushVec(l, sun.oldPos); return 3;}

/*--------------------------------------
LUA	scn::SunSetOldPos

IN	v3SunOldPos
--------------------------------------*/
int scn::SunSetOldPos(lua_State* l){sun.oldPos = vec::LuaToVec(l, 1, 2, 3); return 0;}

/*--------------------------------------
LUA	scn::SunFinalPos

OUT	v3SunFinalPos
--------------------------------------*/
int scn::SunFinalPos(lua_State* l){vec::LuaPushVec(l, sun.FinalPos()); return 3;}

/*--------------------------------------
LUA	scn::SunIntensity

OUT	nSunIntensity
--------------------------------------*/
int scn::SunIntensity(lua_State* l){lua_pushnumber(l, sun.intensity); return 1;}

/*--------------------------------------
LUA	scn::SunSetIntensity

IN	nSunIntensity
--------------------------------------*/
int scn::SunSetIntensity(lua_State* l){sun.intensity = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::SunOldIntensity

OUT	nSunOldIntensity
--------------------------------------*/
int scn::SunOldIntensity(lua_State* l){lua_pushnumber(l, sun.oldIntensity); return 1;}

/*--------------------------------------
LUA	scn::SunSetOldIntensity

IN	nSunOldIntensity
--------------------------------------*/
int scn::SunSetOldIntensity(lua_State* l){sun.oldIntensity = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::SunFinalIntensity

OUT	nSunFinalIntensity
--------------------------------------*/
int scn::SunFinalIntensity(lua_State* l){lua_pushnumber(l, sun.FinalIntensity()); return 1;}

/*--------------------------------------
LUA	scn::SunSubPalette

OUT	iSunSubPalette
--------------------------------------*/
int scn::SunSubPalette(lua_State* l){lua_pushinteger(l, sun.subPalette); return 1;}

/*--------------------------------------
LUA	scn::SunSetSubPalette

IN	iSunSubPalette
--------------------------------------*/
int scn::SunSetSubPalette(lua_State* l){sun.subPalette = lua_tointeger(l, 1); return 0;}

/*--------------------------------------
LUA	scn::SunColorSmooth

OUT	nSunColorSmooth
--------------------------------------*/
int scn::SunColorSmooth(lua_State* l){lua_pushnumber(l, sun.colorSmooth); return 1;}

/*--------------------------------------
LUA	scn::SunSetColorSmooth

IN	nSunColorSmooth
--------------------------------------*/
int scn::SunSetColorSmooth(lua_State* l){sun.colorSmooth = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::SunColorPriority

OUT	nSunColorPriority
--------------------------------------*/
int scn::SunColorPriority(lua_State* l){lua_pushnumber(l, sun.colorPriority); return 1;}

/*--------------------------------------
LUA	scn::SunSetColorPriority

IN	nSunColorPriority
--------------------------------------*/
int scn::SunSetColorPriority(lua_State* l){sun.colorPriority = lua_tonumber(l, 1); return 0;}

/*--------------------------------------
LUA	scn::SunLerpIntensity

OUT	bSunLerpIntensity
--------------------------------------*/
int scn::SunLerpIntensity(lua_State* l)
{
	lua_pushboolean(l, sun.flags & sun.LERP_INTENSITY);
	return 1;
}

/*--------------------------------------
LUA	scn::SunSetLerpIntensity

IN	bSunLerpIntensity
--------------------------------------*/
int scn::SunSetLerpIntensity(lua_State* l)
{
	sun.flags = com::FixedBits(sun.flags, sun.LERP_INTENSITY, lua_toboolean(l, 1) != 0);
	return 0;
}

/*--------------------------------------
LUA	scn::SunLerpPos

OUT	bSunLerpPos
--------------------------------------*/
int scn::SunLerpPos(lua_State* l){lua_pushboolean(l, sun.flags & sun.LERP_POS); return 1;}

/*--------------------------------------
LUA	scn::SunSetLerpPos

IN	bSunLerpPos
--------------------------------------*/
int scn::SunSetLerpPos(lua_State* l)
{
	sun.flags = com::FixedBits(sun.flags, sun.LERP_POS, lua_toboolean(l, 1) != 0);
	return 0;
}