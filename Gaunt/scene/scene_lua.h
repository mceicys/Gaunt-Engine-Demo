// scene_lua.h
// Martynas Ceicys

#ifndef SCENE_LUA_H
#define SCENE_LUA_H

#include "../script/script.h"
#include "../vector/vec_lua.h"

// FIXME: put this stuff in Resource system
#define SCN_CAT0(a, b) a##b
#define SCN_CAT1(a, b) SCN_CAT0(a, b)

#define SCN_LUA_OBJ_MEMBER(Member) \
int SCN_CAT1(SCN_LUA_OBJ, Member)(lua_State* l); \
int SCN_CAT1(SCN_LUA_OBJ, SCN_CAT1(Set, Member))(lua_State* l)

/*
Visual Studio regex for registration generation
Find:		SCN_LUA_OBJ_MEMBER\({.*}\);
Replace:	{"\1", Obj\1},\n\t\t{"Set\1", ObjSet\1},
*/

namespace scn
{

/*
################################################################################################
	CAMERA LUA
################################################################################################
*/

int CreateCamera(lua_State* l);
int ActiveCamera(lua_State* l);
int SetActiveCamera(lua_State* l);

int CamPos(lua_State* l);
int CamSetPos(lua_State* l);
int CamOldPos(lua_State* l);
int CamSetOldPos(lua_State* l);
int CamFinalPos(lua_State* l);
int CamOri(lua_State* l);
int CamSetOri(lua_State* l);
int CamOldOri(lua_State* l);
int CamSetOldOri(lua_State* l);
int CamFinalOri(lua_State* l);
int CamPlace(lua_State* l);
int CamSetPlace(lua_State* l);
int CamFOV(lua_State* l);
int CamSetFOV(lua_State* l);
int CamOldFOV(lua_State* l);
int CamSetOldFOV(lua_State* l);
int CamFinalFOV(lua_State* l);
int CamFinalHFOV(lua_State* l);
int CamSkewOrigin(lua_State* l);
int CamSetSkewOrigin(lua_State* l);
int CamZSkew(lua_State* l);
int CamSetZSkew(lua_State* l);
int CamLerpPos(lua_State* l);
int CamSetLerpPos(lua_State* l);
int CamLerpOri(lua_State* l);
int CamSetLerpOri(lua_State* l);
int CamLerpFOV(lua_State* l);
int CamSetLerpFOV(lua_State* l);

/*
################################################################################################
	ENTITY TYPE LUA
################################################################################################
*/

int CreateEntityType(lua_State* l);
int FindEntityType(lua_State* l);
int EntityTypeNames(lua_State* l);

//	metaEntityType
int TypName(lua_State* l);
int TypFuncs(lua_State* l);

/*
################################################################################################
	ENTITY LUA
################################################################################################
*/

int CreateEntity(lua_State* l);

//	metaEntity
int EntIndex(lua_State* l);
int EntNewIndex(lua_State* l);
int EntLen(lua_State* l);
int EntTable(lua_State* l);
int EntDelete(lua_State* l);
int EntDummyCopy(lua_State* l);
int EntType(lua_State* l);
int EntSetType(lua_State* l);
int EntTypeName(lua_State* l);
int EntMesh(lua_State* l);
int EntSetMesh(lua_State* l);
int EntTexture(lua_State* l);
int EntSetTexture(lua_State* l);
int EntHull(lua_State* l);
int EntSetHull(lua_State* l);
int EntLinkHull(lua_State* l);
int EntSetLinkHull(lua_State* l);
int EntAltHitTest(lua_State* l);
int EntSetAltHitTest(lua_State* l);
int EntPos(lua_State* l);
int EntSetPos(lua_State* l);
int EntOldPos(lua_State* l);
int EntSetOldPos(lua_State* l);
int EntFinalPos(lua_State* l);
int EntOri(lua_State* l);
int EntSetOri(lua_State* l);
int EntOldOri(lua_State* l);
int EntSetOldOri(lua_State* l);
int EntFinalOri(lua_State* l);
int EntHullOri(lua_State* l);
int EntPlace(lua_State* l);
int EntSetPlace(lua_State* l);
int EntOldPlace(lua_State* l);
int EntSetOldPlace(lua_State* l);
int EntFinalPlace(lua_State* l);
int EntScale(lua_State* l);
int EntSetScale(lua_State* l);
int EntOldScale(lua_State* l);
int EntSetOldScale(lua_State* l);
int EntFinalScale(lua_State* l);
int EntSetScaledPlace(lua_State* l);
int EntUV(lua_State* l);
int EntSetUV(lua_State* l);
int EntOldUV(lua_State* l);
int EntSetOldUV(lua_State* l);
int EntFinalUV(lua_State* l);
int EntAverage(lua_State* l);
int EntFrame(lua_State* l);
int EntAnimation(lua_State* l);
int EntSetAnimation(lua_State* l);
int EntClearAnimation(lua_State* l);
int EntAnimationLoop(lua_State* l);
int EntSetAnimationLoop(lua_State* l);
int EntAnimationSpeed(lua_State* l);
int EntSetAnimationSpeed(lua_State* l);
int EntAnimationName(lua_State* l);
int EntAnimationPlaying(lua_State* l);
int EntUpcomingFrame(lua_State* l);
int EntTransitionTime(lua_State* l);
int EntSubPalette(lua_State* l);
int EntSetSubPalette(lua_State* l);
int EntOpacity(lua_State* l);
int EntSetOpacity(lua_State* l);
int EntOldOpacity(lua_State* l);
int EntSetOldOpacity(lua_State* l);
int EntVisible(lua_State* l);
int EntSetVisible(lua_State* l);
int EntWorldVisible(lua_State* l);
int EntSetWorldVisible(lua_State* l);
int EntShadow(lua_State* l);
int EntSetShadow(lua_State* l);
int EntSetDisplay(lua_State* l);
int EntRegularGlass(lua_State* l);
int EntSetRegularGlass(lua_State* l);
int EntAmbientGlass(lua_State* l);
int EntSetAmbientGlass(lua_State* l);
int EntMinimumGlass(lua_State* l);
int EntSetMinimumGlass(lua_State* l);
int EntWeakGlass(lua_State* l);
int EntSetWeakGlass(lua_State* l);
int EntCloud(lua_State* l);
int EntSetCloud(lua_State* l);
int EntLerpPos(lua_State* l);
int EntSetLerpPos(lua_State* l);
int EntLerpOri(lua_State* l);
int EntSetLerpOri(lua_State* l);
int EntLerpScale(lua_State* l);
int EntSetLerpScale(lua_State* l);
int EntLerpUV(lua_State* l);
int EntSetLerpUV(lua_State* l);
int EntLerpOpacity(lua_State* l);
int EntSetLerpOpacity(lua_State* l);
int EntLerpFrame(lua_State* l);
int EntSetLerpFrame(lua_State* l);
int EntOrientHull(lua_State* l);
int EntSetOrientHull(lua_State* l);
int EntOverlay(lua_State* l);
int EntSetOverlay(lua_State* l);
int EntChild(lua_State* l);
int EntSetChild(lua_State* l);
int EntPointLink(lua_State* l);
int EntSetPointLink(lua_State* l);
int EntSky(lua_State* l);
int EntSetSky(lua_State* l);
int EntMimicMesh(lua_State* l);
int EntMimicScaledPlace(lua_State* l);
int EntFlagSet(lua_State* l);
int EntPrioritize(lua_State* l);

/*
################################################################################################
	LIGHT LUA
################################################################################################
*/

// FIXME HACK: lots of global lights, make them objects to reduce copy-pasted code
#define GLOBAL_LIGHT_LUA_DECLARATIONS(Obj) \
int Obj##Intensity(lua_State* l); \
int Obj##SetIntensity(lua_State* l); \
int Obj##OldIntensity(lua_State* l); \
int Obj##SetOldIntensity(lua_State* l); \
int Obj##FinalIntensity(lua_State* l); \
int Obj##SubPalette(lua_State* l); \
int Obj##SetSubPalette(lua_State* l); \
int Obj##ColorSmooth(lua_State* l); \
int Obj##SetColorSmooth(lua_State* l); \
int Obj##ColorPriority(lua_State* l); \
int Obj##SetColorPriority(lua_State* l); \
int Obj##LerpIntensity(lua_State* l); \
int Obj##SetLerpIntensity(lua_State* l)

GLOBAL_LIGHT_LUA_DECLARATIONS(Ambient);
GLOBAL_LIGHT_LUA_DECLARATIONS(Sky);
GLOBAL_LIGHT_LUA_DECLARATIONS(Cloud);

int SunPos(lua_State* l);
int SunSetPos(lua_State* l);
int SunOldPos(lua_State* l);
int SunSetOldPos(lua_State* l);
int SunFinalPos(lua_State* l);
int SunIntensity(lua_State* l);
int SunSetIntensity(lua_State* l);
int SunOldIntensity(lua_State* l);
int SunSetOldIntensity(lua_State* l);
int SunFinalIntensity(lua_State* l);
int SunSubPalette(lua_State* l);
int SunSetSubPalette(lua_State* l);
int SunColorSmooth(lua_State* l);
int SunSetColorSmooth(lua_State* l);
int SunColorPriority(lua_State* l);
int SunSetColorPriority(lua_State* l);
int SunLerpIntensity(lua_State* l);
int SunSetLerpIntensity(lua_State* l);
int SunLerpPos(lua_State* l);
int SunSetLerpPos(lua_State* l);

/*
################################################################################################
	BULB LUA
################################################################################################
*/

int CreateBulb(lua_State* l);

//	metaBulb
int BlbDelete(lua_State* l);
int BlbPos(lua_State* l);
int BlbSetPos(lua_State* l);
int BlbOldPos(lua_State* l);
int BlbSetOldPos(lua_State* l);
int BlbFinalPos(lua_State* l);
int BlbRadius(lua_State* l);
int BlbSetRadius(lua_State* l);
int BlbOldRadius(lua_State* l);
int BlbSetOldRadius(lua_State* l);
int BlbFinalRadius(lua_State* l);
int BlbExponent(lua_State* l);
int BlbSetExponent(lua_State* l);
int BlbOldExponent(lua_State* l);
int BlbSetOldExponent(lua_State* l);
int BlbFinalExponent(lua_State* l);
int BlbOri(lua_State* l);
int BlbSetOri(lua_State* l);
int BlbOldOri(lua_State* l);
int BlbSetOldOri(lua_State* l);
int BlbFinalOri(lua_State* l);
int BlbPitch(lua_State* l);
int BlbSetPitch(lua_State* l);
int BlbYaw(lua_State* l);
int BlbSetYaw(lua_State* l);
int BlbCutoff(lua_State* l);
int BlbSetCutoff(lua_State* l);
int BlbOldCutoff(lua_State* l);
int BlbSetOldCutoff(lua_State* l);
int BlbFinalCutoff(lua_State* l);
int BlbOuter(lua_State* l);
int BlbSetOuter(lua_State* l);
int BlbOldOuter(lua_State* l);
int BlbSetOldOuter(lua_State* l);
int BlbFinalOuter(lua_State* l);
int BlbInner(lua_State* l);
int BlbSetInner(lua_State* l);
int BlbOldInner(lua_State* l);
int BlbSetOldInner(lua_State* l);
int BlbFinalInner(lua_State* l);
int BlbSetPlace(lua_State* l);
int BlbFinalPlace(lua_State* l);
int BlbIntensity(lua_State* l);
int BlbSetIntensity(lua_State* l);
int BlbOldIntensity(lua_State* l);
int BlbSetOldIntensity(lua_State* l);
int BlbFinalIntensity(lua_State* l);
int BlbSubPalette(lua_State* l);
int BlbSetSubPalette(lua_State* l);
int BlbColorSmooth(lua_State* l);
int BlbSetColorSmooth(lua_State* l);
int BlbColorPriority(lua_State* l);
int BlbSetColorPriority(lua_State* l);
int BlbLerpIntensity(lua_State* l);
int BlbSetLerpIntensity(lua_State* l);
int BlbLerpPos(lua_State* l);
int BlbSetLerpPos(lua_State* l);
int BlbLerpRadius(lua_State* l);
int BlbSetLerpRadius(lua_State* l);
int BlbLerpExponent(lua_State* l);
int BlbSetLerpExponent(lua_State* l);
int BlbLerpOri(lua_State* l);
int BlbSetLerpOri(lua_State* l);
int BlbLerpCutoff(lua_State* l);
int BlbSetLerpCutoff(lua_State* l);

/*
################################################################################################
	WORLD LUA
################################################################################################
*/

int LoadWorld(lua_State* l);
int WorldBox(lua_State* l);
int NearbyTriangle(lua_State* l);
int PosToLeaf(lua_State* l);

// FIXME: WorldTree class
int RootNode(lua_State* l);
int NodeLeft(lua_State* l);
int NodeRight(lua_State* l);
int NodeChildren(lua_State* l);
int NodeParent(lua_State* l);
int NodeSolid(lua_State* l);
int NodeNumEntities(lua_State* l);
int NodeEntity(lua_State* l);
int NodeNumTriangles(lua_State* l);
int NodeTriangleTexture(lua_State* l);

/*
################################################################################################
	OVERLAY LUA
################################################################################################
*/

int GetOverlay(lua_State* l);
int SkyOverlay(lua_State* l);
int OvrCamera(lua_State* l);
int OvrSetCamera(lua_State* l);
int OvrRelit(lua_State* l);
int OvrSetRelit(lua_State* l);

/*
################################################################################################
	FOG LUA
################################################################################################
*/

int GetFog(lua_State* l);

// metaFog
#define SCN_LUA_OBJ Fog
SCN_LUA_OBJ_MEMBER(Radius);
SCN_LUA_OBJ_MEMBER(ThinRadius);
SCN_LUA_OBJ_MEMBER(ThinExponent);
SCN_LUA_OBJ_MEMBER(GeoSubStartDist);
SCN_LUA_OBJ_MEMBER(GeoSubHalfDist);
SCN_LUA_OBJ_MEMBER(GeoSubFactor);
SCN_LUA_OBJ_MEMBER(GeoSubPalette);
SCN_LUA_OBJ_MEMBER(FadeStartDist);
SCN_LUA_OBJ_MEMBER(FadeHalfDist);
SCN_LUA_OBJ_MEMBER(FadeAmount);
SCN_LUA_OBJ_MEMBER(FadeRampPos);
SCN_LUA_OBJ_MEMBER(FadeRampPosSun);
SCN_LUA_OBJ_MEMBER(AddStartDist);
SCN_LUA_OBJ_MEMBER(AddHalfDist);
SCN_LUA_OBJ_MEMBER(AddAmount);
SCN_LUA_OBJ_MEMBER(AddAmountSun);
SCN_LUA_OBJ_MEMBER(SunRadius);
SCN_LUA_OBJ_MEMBER(SunExponent);
SCN_LUA_OBJ_MEMBER(Center);
#undef SCN_LUA_OBJ

}

/*
################################################################################################
	DEFINITION MACROS
################################################################################################
*/

#define SCN_LUA_OBJ_NUMBER(Member, member) \
\
/*--------------------------------------
LUA	scn::ObjMember

OUT	nMember
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, Member))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	lua_pushnumber(l, o->member); \
	return 1; \
} \
\
/*--------------------------------------
LUA	scn::ObjSetMember

IN	objO, nMember
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, SCN_CAT1(Set, Member)))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	o->member = luaL_checknumber(l, 2); \
	return 0; \
}

#define SCN_LUA_OBJ_INTEGER(Member, member) \
\
/*--------------------------------------
LUA	scn::ObjMember

OUT	iMember
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, Member))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	lua_pushinteger(l, o->member); \
	return 1; \
} \
\
/*--------------------------------------
LUA	scn::ObjSetMember

IN	objO, iMember
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, SCN_CAT1(Set, Member)))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	o->member = luaL_checkinteger(l, 2); \
	return 0; \
}

#define SCN_LUA_OBJ_VEC3(Member, member) \
\
/*--------------------------------------
LUA	scn::ObjMember

OUT	v3Member
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, Member))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	vec::LuaPushVec(l, o->member); \
	return 3; \
} \
\
/*--------------------------------------
LUA	scn::ObjSetMember

IN	objO, v3Member
--------------------------------------*/ \
int SCN_CAT1(scn::, SCN_CAT1(SCN_LUA_OBJ, SCN_CAT1(Set, Member)))(lua_State* l) \
{ \
	SCN_LUA_OBJ* o = SCN_LUA_OBJ::CheckLuaTo(1); \
	o->member = vec::CheckLuaToVec(l, 2, 3, 4); \
	return 0; \
}

#endif