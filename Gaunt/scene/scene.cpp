// scene.cpp -- Game space
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../../GauntCommon/json_ext.h"
#include "../render/render.h"

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	scn::SaveSpace

Sets old space to equal new space so interpolation can be done. Call before a tick.

FIXME: probably a lot of cache misses here with lots of entities
--------------------------------------*/
void scn::SaveSpace()
{
	// Cameras
	for(const com::linker<Camera>* it = Camera::List().f; it; it = it->next)
		it->o->SaveOld();

	// Entities
	for(const com::linker<Entity>* it = Entity::List().f; it; it = it->next)
		it->o->SaveOld();

	// Lights
	ambient.oldIntensity = ambient.intensity;
	sky.oldIntensity = sky.intensity;
	cloud.oldIntensity = cloud.intensity;
	sun.oldIntensity = sun.intensity;
	sun.oldPos = sun.pos;

	for(com::linker<Bulb>* it = Bulb::List().f; it != 0; it = it->next)
		it->o->SaveOld();
}

/*--------------------------------------
	scn::Init
--------------------------------------*/
void scn::Init()
{
	// Lua
	luaL_Reg camRegs[] = {
		{"Pos", CamPos},
		{"SetPos", CamSetPos},
		{"OldPos", CamOldPos},
		{"SetOldPos", CamSetOldPos},
		{"FinalPos", CamFinalPos},
		{"Ori", CamOri},
		{"SetOri", CamSetOri},
		{"OldOri", CamOldOri},
		{"SetOldOri", CamSetOldOri},
		{"FinalOri", CamFinalOri},
		{"Place", CamPlace},
		{"SetPlace", CamSetPlace},
		{"FOV", CamFOV},
		{"SetFOV", CamSetFOV},
		{"OldFOV", CamOldFOV},
		{"SetOldFOV", CamSetOldFOV},
		{"FinalFOV", CamFinalFOV},
		{"FinalHFOV", CamFinalHFOV},
		{"SkewOrigin", CamSkewOrigin},
		{"SetSkewOrigin", CamSetSkewOrigin},
		{"ZSkew", CamZSkew},
		{"SetZSkew", CamSetZSkew},
		{"LerpPos", CamLerpPos},
		{"SetLerpPos", CamSetLerpPos},
		{"LerpOri", CamLerpOri},
		{"SetLerpOri", CamSetLerpOri},
		{"LerpFOV", CamLerpFOV},
		{"SetLerpFOV", CamSetLerpFOV},
		{"__gc", Camera::CLuaDelete},
		{0, 0}
	};

	luaL_Reg typRegs[] = {
		{"Name", TypName},
		{"Funcs", TypFuncs},
		{0, 0}
	};

	luaL_Reg entRegs[] = {
		{"__index", EntIndex},
		{"__newindex", EntNewIndex},
		{"__len", EntLen},
		{"Table", EntTable},
		{"Delete", EntDelete},
		{"DummyCopy", EntDummyCopy},
		{"Type", EntType},
		{"SetType", EntSetType},
		{"TypeName", EntTypeName},
		{"Mesh", EntMesh},
		{"SetMesh", EntSetMesh},
		{"Texture", EntTexture},
		{"SetTexture", EntSetTexture},
		{"Hull", EntHull},
		{"SetHull", EntSetHull},
		{"LinkHull", EntLinkHull},
		{"SetLinkHull", EntSetLinkHull},
		{"AltHitTest", EntAltHitTest},
		{"SetAltHitTest", EntSetAltHitTest},
		{"Pos", EntPos},
		{"SetPos", EntSetPos},
		{"OldPos", EntOldPos},
		{"SetOldPos", EntSetOldPos},
		{"FinalPos", EntFinalPos},
		{"Ori", EntOri},
		{"SetOri", EntSetOri},
		{"OldOri", EntOldOri},
		{"SetOldOri", EntSetOldOri},
		{"FinalOri", EntFinalOri},
		{"HullOri", EntHullOri},
		{"Place", EntPlace},
		{"SetPlace", EntSetPlace},
		{"OldPlace", EntOldPlace},
		{"SetOldPlace", EntSetOldPlace},
		{"FinalPlace", EntFinalPlace},
		{"Scale", EntScale},
		{"SetScale", EntSetScale},
		{"OldScale", EntOldScale},
		{"SetOldScale", EntSetOldScale},
		{"FinalScale", EntFinalScale},
		{"SetScaledPlace", EntSetScaledPlace},
		{"UV", EntUV},
		{"SetUV", EntSetUV},
		{"OldUV", EntOldUV},
		{"SetOldUV", EntSetOldUV},
		{"FinalUV", EntFinalUV},
		{"Average", EntAverage},
		{"Frame", EntFrame},
		{"Animation", EntAnimation},
		{"SetAnimation", EntSetAnimation},
		{"ClearAnimation", EntClearAnimation},
		{"AnimationLoop", EntAnimationLoop},
		{"SetAnimationLoop", EntSetAnimationLoop},
		{"AnimationSpeed", EntAnimationSpeed},
		{"SetAnimationSpeed", EntSetAnimationSpeed},
		{"AnimationName", EntAnimationName},
		{"AnimationPlaying", EntAnimationPlaying},
		{"UpcomingFrame", EntUpcomingFrame},
		{"TransitionTime", EntTransitionTime},
		{"SubPalette", EntSubPalette},
		{"SetSubPalette", EntSetSubPalette},
		{"Opacity", EntOpacity},
		{"SetOpacity", EntSetOpacity},
		{"OldOpacity", EntOldOpacity},
		{"SetOldOpacity", EntSetOldOpacity},
		{"Visible", EntVisible},
		{"SetVisible", EntSetVisible},
		{"WorldVisible", EntWorldVisible},
		{"SetWorldVisible", EntSetWorldVisible},
		{"Shadow", EntShadow},
		{"SetShadow", EntSetShadow},
		{"SetDisplay", EntSetDisplay},
		{"RegularGlass", EntRegularGlass},
		{"SetRegularGlass", EntSetRegularGlass},
		{"AmbientGlass", EntAmbientGlass},
		{"SetAmbientGlass", EntSetAmbientGlass},
		{"MinimumGlass", EntMinimumGlass},
		{"SetMinimumGlass", EntSetMinimumGlass},
		{"WeakGlass", EntWeakGlass},
		{"SetWeakGlass", EntSetWeakGlass},
		{"Cloud", EntCloud},
		{"SetCloud", EntSetCloud},
		{"LerpPos", EntLerpPos},
		{"SetLerpPos", EntSetLerpPos},
		{"LerpOri", EntLerpOri},
		{"SetLerpOri", EntSetLerpOri},
		{"LerpScale", EntLerpScale},
		{"SetLerpScale", EntSetLerpScale},
		{"LerpUV", EntLerpUV},
		{"SetLerpUV", EntSetLerpUV},
		{"LerpOpacity", EntLerpOpacity},
		{"SetLerpOpacity", EntSetLerpOpacity},
		{"LerpFrame", EntLerpFrame},
		{"SetLerpFrame", EntSetLerpFrame},
		{"OrientHull", EntOrientHull},
		{"SetOrientHull", EntSetOrientHull},
		{"Overlay", EntOverlay},
		{"SetOverlay", EntSetOverlay},
		{"Child", EntChild},
		{"SetChild", EntSetChild},
		{"PointLink", EntPointLink},
		{"SetPointLink", EntSetPointLink},
		{"Sky", EntSky},
		{"SetSky", EntSetSky},
		{"MimicMesh", EntMimicMesh},
		{"MimicScaledPlace", EntMimicScaledPlace},
		{"FlagSet", EntFlagSet},
		{"Prioritize", EntPrioritize},

		{"RecordID", Entity::CLuaRecordID},
		{"SetRecordID", Entity::CLuaSetRecordID},
		{"Transcript", Entity::CLuaTranscript},
		{"SetTranscript", Entity::CLuaSetTranscript},
		{"Prev", Entity::CLuaPrev},
		{"Next", Entity::CLuaNext},
		{0, 0}
	};

	luaL_Reg blbRegs[] = {
		{"Delete", BlbDelete},
		{"Pos", BlbPos},
		{"SetPos", BlbSetPos},
		{"OldPos", BlbOldPos},
		{"SetOldPos", BlbSetOldPos},
		{"FinalPos", BlbFinalPos},
		{"Radius", BlbRadius},
		{"SetRadius", BlbSetRadius},
		{"OldRadius", BlbOldRadius},
		{"SetOldRadius", BlbSetOldRadius},
		{"FinalRadius", BlbFinalRadius},
		{"Exponent", BlbExponent},
		{"SetExponent", BlbSetExponent},
		{"OldExponent", BlbOldExponent},
		{"SetOldExponent", BlbSetOldExponent},
		{"FinalExponent", BlbFinalExponent},
		{"Ori", BlbOri},
		{"SetOri", BlbSetOri},
		{"OldOri", BlbOldOri},
		{"SetOldOri", BlbSetOldOri},
		{"FinalOri", BlbFinalOri},
		{"Pitch", BlbPitch},
		{"SetPitch", BlbSetPitch},
		{"Yaw", BlbYaw},
		{"SetYaw", BlbSetYaw},
		{"Cutoff", BlbCutoff},
		{"SetCutoff", BlbSetCutoff},
		{"OldCutoff", BlbOldCutoff},
		{"SetOldCutoff", BlbSetOldCutoff},
		{"FinalCutoff", BlbFinalCutoff},
		{"Outer", BlbOuter},
		{"SetOuter", BlbSetOuter},
		{"OldOuter", BlbOldOuter},
		{"SetOldOuter", BlbSetOldOuter},
		{"FinalOuter", BlbFinalOuter},
		{"Inner", BlbInner},
		{"SetInner", BlbSetInner},
		{"OldInner", BlbOldInner},
		{"SetOldInner", BlbSetOldInner},
		{"FinalInner", BlbFinalInner},
		{"SetPlace", BlbSetPlace},
		{"FinalPlace", BlbFinalPlace},
		{"Intensity", BlbIntensity},
		{"SetIntensity", BlbSetIntensity},
		{"OldIntensity", BlbOldIntensity},
		{"SetOldIntensity", BlbSetOldIntensity},
		{"FinalIntensity", BlbFinalIntensity},
		{"SubPalette", BlbSubPalette},
		{"SetSubPalette", BlbSetSubPalette},
		{"ColorSmooth", BlbColorSmooth},
		{"SetColorSmooth", BlbSetColorSmooth},
		{"ColorPriority", BlbColorPriority},
		{"SetColorPriority", BlbSetColorPriority},
		{"LerpIntensity", BlbLerpIntensity},
		{"SetLerpIntensity", BlbSetLerpIntensity},
		{"LerpPos", BlbLerpPos},
		{"SetLerpPos", BlbSetLerpPos},
		{"LerpRadius", BlbLerpRadius},
		{"SetLerpRadius", BlbSetLerpRadius},
		{"LerpExponent", BlbLerpExponent},
		{"SetLerpExponent", BlbSetLerpExponent},
		{"LerpOri", BlbLerpOri},
		{"SetLerpOri", BlbSetLerpOri},
		{"LerpCutoff", BlbLerpCutoff},
		{"SetLerpCutoff", BlbSetLerpCutoff},

		{"RecordID", Bulb::CLuaRecordID},
		{"SetRecordID", Bulb::CLuaSetRecordID},
		{"Transcript", Bulb::CLuaTranscript},
		{"SetTranscript", Bulb::CLuaSetTranscript},
		{"Prev", Bulb::CLuaPrev},
		{"Next", Bulb::CLuaNext},
		{0, 0}
	};

	luaL_Reg ovrRegs[] = {
		{"Camera", OvrCamera},
		{"SetCamera", OvrSetCamera},
		{"Relit", OvrRelit},
		{"SetRelit", OvrSetRelit},
		{0, 0}
	};

	luaL_Reg fogRegs[] = {
		{"Center", FogCenter},
		{"SetCenter", FogSetCenter},
		{"Radius", FogRadius},
		{"SetRadius", FogSetRadius},
		{"ThinRadius", FogThinRadius},
		{"SetThinRadius", FogSetThinRadius},
		{"ThinExponent", FogThinExponent},
		{"SetThinExponent", FogSetThinExponent},
		{"GeoSubStartDist", FogGeoSubStartDist},
		{"SetGeoSubStartDist", FogSetGeoSubStartDist},
		{"GeoSubHalfDist", FogGeoSubHalfDist},
		{"SetGeoSubHalfDist", FogSetGeoSubHalfDist},
		{"GeoSubFactor", FogGeoSubFactor},
		{"SetGeoSubFactor", FogSetGeoSubFactor},
		{"GeoSubPalette", FogGeoSubPalette},
		{"SetGeoSubPalette", FogSetGeoSubPalette},
		{"FadeStartDist", FogFadeStartDist},
		{"SetFadeStartDist", FogSetFadeStartDist},
		{"FadeHalfDist", FogFadeHalfDist},
		{"SetFadeHalfDist", FogSetFadeHalfDist},
		{"FadeAmount", FogFadeAmount},
		{"SetFadeAmount", FogSetFadeAmount},
		{"FadeRampPos", FogFadeRampPos},
		{"SetFadeRampPos", FogSetFadeRampPos},
		{"FadeRampPosSun", FogFadeRampPosSun},
		{"SetFadeRampPosSun", FogSetFadeRampPosSun},
		{"AddStartDist", FogAddStartDist},
		{"SetAddStartDist", FogSetAddStartDist},
		{"AddHalfDist", FogAddHalfDist},
		{"SetAddHalfDist", FogSetAddHalfDist},
		{"AddAmount", FogAddAmount},
		{"SetAddAmount", FogSetAddAmount},
		{"AddAmountSun", FogAddAmountSun},
		{"SetAddAmountSun", FogSetAddAmountSun},
		{"SunRadius", FogSunRadius},
		{"SetSunRadius", FogSetSunRadius},
		{"SunExponent", FogSunExponent},
		{"SetSunExponent", FogSetSunExponent},
		{0, 0}
	};

	Camera::RegisterMetatable(camRegs, 0);
	EntityType::RegisterMetatable(typRegs, 0);
	Entity::RegisterMetatable(entRegs, 0, false);
	Bulb::RegisterMetatable(blbRegs, 0);
	Overlay::RegisterMetatable(ovrRegs, 0);
	Fog::RegisterMetatable(fogRegs, 0);

	// FIXME HACK
	#define GLOBAL_LIGHT_LUA_REGS(string, Obj) \
	{string "Intensity", Obj##Intensity}, \
	{string "SetIntensity", Obj##SetIntensity}, \
	{string "OldIntensity", Obj##OldIntensity}, \
	{string "SetOldIntensity", Obj##SetOldIntensity}, \
	{string "FinalIntensity", Obj##FinalIntensity}, \
	{string "SubPalette", Obj##SubPalette}, \
	{string "SetSubPalette", Obj##SetSubPalette}, \
	{string "ColorSmooth", Obj##ColorSmooth}, \
	{string "SetColorSmooth", Obj##SetColorSmooth}, \
	{string "ColorPriority", Obj##ColorPriority}, \
	{string "SetColorPriority", Obj##SetColorPriority}, \
	{string "LerpIntensity", Obj##LerpIntensity}, \
	{string "SetLerpIntensity", Obj##SetLerpIntensity}

	luaL_Reg regs[] = {
		{"Camera", CreateCamera},
		{"ActiveCamera", ActiveCamera},
		{"SetActiveCamera", SetActiveCamera},

		{"CreateEntityType", CreateEntityType},
		{"FindEntityType", FindEntityType},
		{"EntityTypeNames", EntityTypeNames},

		{"CreateEntity", CreateEntity},
		{"EntityExists", Entity::CLuaExists},
		{"FirstEntity", Entity::CLuaFirst},
		{"LastEntity", Entity::CLuaLast},
		{"EntityFromRecordID", Entity::CLuaFromRecordID},

		GLOBAL_LIGHT_LUA_REGS("Ambient", Ambient),
		GLOBAL_LIGHT_LUA_REGS("Sky", Sky),
		GLOBAL_LIGHT_LUA_REGS("Cloud", Cloud),

		{"SunPos", SunPos},
		{"SunSetPos", SunSetPos},
		{"SunOldPos", SunOldPos},
		{"SunSetOldPos", SunSetOldPos},
		{"SunFinalPos", SunFinalPos},
		{"SunIntensity", SunIntensity},
		{"SunSetIntensity", SunSetIntensity},
		{"SunOldIntensity", SunOldIntensity},
		{"SunSetOldIntensity", SunSetOldIntensity},
		{"SunFinalIntensity", SunFinalIntensity},
		{"SunSubPalette", SunSubPalette},
		{"SunSetSubPalette", SunSetSubPalette},
		{"SunColorSmooth", SunColorSmooth},
		{"SunSetColorSmooth", SunSetColorSmooth},
		{"SunColorPriority", SunColorPriority},
		{"SunSetColorPriority", SunSetColorPriority},
		{"SunLerpIntensity", SunLerpIntensity},
		{"SunSetLerpIntensity", SunSetLerpIntensity},
		{"SunLerpPos", SunLerpPos},
		{"SunSetLerpPos", SunSetLerpPos},

		{"CreateBulb", CreateBulb},
		{"BulbExists", Bulb::CLuaExists},
		{"FirstBulb", Bulb::CLuaFirst},
		{"LastBulb", Bulb::CLuaLast},
		{"BulbFromRecordID", Bulb::CLuaFromRecordID},

		{"LoadWorld", LoadWorld},
		{"WorldBox", WorldBox},
		{"NearbyTriangle", NearbyTriangle},
		{"PosToLeaf", PosToLeaf},
		{"RootNode", RootNode},
		{"NodeLeft", NodeLeft},
		{"NodeRight", NodeRight},
		{"NodeChildren", NodeChildren},
		{"NodeParent", NodeParent},
		{"NodeSolid", NodeSolid},
		{"NodeNumEntities", NodeNumEntities},
		{"NodeEntity", NodeEntity},
		{"NodeNumTriangles", NodeNumTriangles},
		{"NodeTriangleTexture", NodeTriangleTexture},

		{"GetOverlay", GetOverlay},
		{"SkyOverlay", SkyOverlay},

		{"GetFog", GetFog},

		{0, 0}
	};

	const luaL_Reg* metas[] = {
		camRegs,
		typRegs,
		entRegs,
		blbRegs,
		ovrRegs,
		fogRegs,
		0
	};

	const char* prefixes[] = {
		"Cam",
		"EntType", // FIXME: long
		"Ent",
		"Blb",
		"Ovr",
		"Fog",
		0
	};

	const int NUM_FIELDS = 1;
	lua_pushliteral(scr::state, "fog");
	fog.LuaPush(); // gscn.fog = gscn.GetFog()

	scr::RegisterLibrary(scr::state, "gscn", regs, 0, NUM_FIELDS, metas, prefixes);
}

/*--------------------------------------
	scn::CallEntityFunctions

Calls the specified function type for every entity.

FIXME: entities have function called twice if an entity is Prioritize()'d during iteration
--------------------------------------*/
void scn::CallEntityFunctions(ent_func func)
{
	scr::EnsureStack(scr::state, 2);

	for(const com::linker<Entity>* reg = Entity::List().f, *next; reg; reg = next)
	{
		Entity& ent = *reg->o;

		if(ent.Alive() && ent.type && ent.type->HasRef(func))
		{
			ent.AddLock(); // Delay potential delete so next ent can be retrieved after call
			ent.UnsafeCall(func);
			next = reg->next;
			ent.RemoveLock();
		}
		else
			next = reg->next;
	}
}

/*--------------------------------------
	scn::SetGlobalPairs
--------------------------------------*/
void scn::SetGlobalPairs(com::PairMap<com::JSVar>& globalOut)
{
	// world
	if(scn::WorldFileName())
		globalOut.Ensure("world")->Value().SetString(scn::WorldFileName());

#if 0 // FIXME: multiple cameras can exist now
	// cam
	scn::Camera& cam = scn::GlobalCamera();

	com::JSVarSetVec(globalOut.Ensure("camPos")->Value(), cam.pos);

	if(cam.oldPos != cam.pos)
		com::JSVarSetVec(globalOut.Ensure("camOldPos")->Value(), cam.oldPos);

	com::JSVarSetQua(globalOut.Ensure("camOri")->Value(), cam.ori);

	if(cam.oldOri != cam.ori)
		com::JSVarSetQua(globalOut.Ensure("camOldOri")->Value(), cam.oldOri);

	globalOut.Ensure("camFOV")->Value().SetNumber(cam.fov);

	if(cam.oldFOV != cam.fov)
		globalOut.Ensure("camOldFOV")->Value().SetNumber(cam.oldFOV);

	if(!(cam.flags & cam.LERP_POS))
		globalOut.Ensure("camLerpPos")->Value().SetBool(cam.flags & cam.LERP_POS);

	if(!(cam.flags & cam.LERP_ORI))
		globalOut.Ensure("camLerpOri")->Value().SetBool(cam.flags & cam.LERP_ORI);

	if(!(cam.flags & cam.LERP_FOV))
		globalOut.Ensure("camLerpFOV")->Value().SetBool(cam.flags & cam.LERP_FOV);
#endif

	// ambient lights
	ambient.TranscribeGlobals(globalOut, "ambient");
	sky.TranscribeGlobals(globalOut, "sky");
	cloud.TranscribeGlobals(globalOut, "cloud");

	// sun
	globalOut.Ensure("sunIntensity")->Value().SetNumber(sun.intensity);

	if(sun.oldIntensity != sun.intensity)
		globalOut.Ensure("sunOldIntensity")->Value().SetNumber(sun.oldIntensity);

	globalOut.Ensure("sunSubPalette")->Value().SetNumber(sun.subPalette);
	com::JSVarSetVec(globalOut.Ensure("sunPos")->Value(), sun.pos);

	if(sun.colorSmooth != 1.0f)
		globalOut.Ensure("sunColorSmooth")->Value().SetNumber(sun.colorSmooth);

	if(sun.colorPriority != 1.0f)
		globalOut.Ensure("sunColorPriority")->Value().SetNumber(sun.colorPriority);

	if(sun.oldPos != sun.pos)
		com::JSVarSetVec(globalOut.Ensure("sunOldPos")->Value(), sun.oldPos);

	if(!(sun.flags & sun.LERP_INTENSITY))
		globalOut.Ensure("sunLerpIntensity")->Value().SetBool(sun.flags & sun.LERP_INTENSITY);

	if(!(sun.flags & sun.LERP_POS))
		globalOut.Ensure("sunLerpPos")->Value().SetBool(sun.flags & sun.LERP_POS);

	// fog
	fog.TranscribeGlobals(globalOut, "fog");
}

/*--------------------------------------
	scn::InterpretGlobalPairs
--------------------------------------*/
bool scn::InterpretGlobalPairs(const com::PairMap<com::JSVar>& global)
{
	const com::Pair<com::JSVar>* pair;

	if(pair = global.Find("world"))
	{
		if(pair->Value().String() && !scn::WorldFileName() ||
		strcmp(pair->Value().String(), scn::WorldFileName()))
		{
			if(!scn::LoadWorld(pair->Value().String()))
				return false;
		}
	}

#if 0
	scn::Camera& cam = scn::GlobalCamera();

	if(pair = global.Find("camPos"))
		cam.pos = com::JSVarToVec3(pair->Value());
	else
		cam.pos = 0.0f;

	if(pair = global.Find("camOldPos"))
		cam.oldPos = com::JSVarToVec3(pair->Value());
	else
		cam.oldPos = cam.pos;

	if(pair = global.Find("camOri"))
		cam.ori = com::JSVarToQua(pair->Value());
	else
		cam.ori = com::QUA_IDENTITY;

	if(pair = global.Find("camOldOri"))
		cam.oldOri = com::JSVarToQua(pair->Value());
	else
		cam.oldOri = cam.ori;

	if(pair = global.Find("camFOV"))
		cam.fov = pair->Value().Double();
	else
		cam.fov = 1.0f;

	if(pair = global.Find("camOldFOV"))
		cam.oldFOV = pair->Value().Double();
	else
		cam.oldFOV = cam.fov;

	if(pair = global.Find("camLerpPos"))
		cam.flags = com::FixedBits(cam.flags, cam.LERP_POS, pair->Value().Bool());
	else
		cam.flags |= cam.LERP_POS;

	if(pair = global.Find("camLerpOri"))
		cam.flags = com::FixedBits(cam.flags, cam.LERP_ORI, pair->Value().Bool());
	else
		cam.flags |= cam.LERP_ORI;

	if(pair = global.Find("camLerpFOV"))
		cam.flags = com::FixedBits(cam.flags, cam.LERP_FOV, pair->Value().Bool());
	else
		cam.flags |= cam.LERP_FOV;
#endif

	ambient.InterpretGlobals(global, "ambient", 0.0f);
	sky.InterpretGlobals(global, "sky", 1.0f);
	cloud.InterpretGlobals(global, "cloud", 0.0f);

	if(pair = global.Find("sunIntensity"))
		sun.intensity = pair->Value().Double();
	else
		sun.intensity = 1.0f;

	if(pair = global.Find("sunOldIntensity"))
		sun.oldIntensity = pair->Value().Double();
	else
		sun.oldIntensity = sun.intensity;

	if(pair = global.Find("sunSubPalette"))
		sun.subPalette = pair->Value().Int();
	else
		sun.subPalette = 0;

	if(pair = global.Find("sunColorSmooth"))
		sun.colorSmooth = pair->Value().Float();
	else
		sun.colorSmooth = 1.0f;

	if(pair = global.Find("sunColorPriority"))
		sun.colorPriority = pair->Value().Float();
	else
		sun.colorPriority = 1.0f;

	if(pair = global.Find("sunPos"))
		sun.pos = com::JSVarToVec3(pair->Value());
	else
		sun.pos = 1.0f;

	if(pair = global.Find("sunOldPos"))
		sun.oldPos = com::JSVarToVec3(pair->Value());
	else
		sun.oldPos = sun.pos;

	if(pair = global.Find("sunLerpIntensity"))
		sun.flags = com::FixedBits(sun.flags, sun.LERP_INTENSITY, pair->Value().Bool());
	else
		sun.flags |= sun.LERP_INTENSITY;

	if(pair = global.Find("sunLerpPos"))
		sun.flags = com::FixedBits(sun.flags, sun.LERP_POS, pair->Value().Bool());
	else
		sun.flags |= sun.LERP_POS;

	// FIXME HACK: should exposure be a camera attribute?
		// FIXME: either remove this from transcript or put exposure in scene
	if(pair = global.Find("exposure"))
		rnd::exposure.SetValue(pair->Value().Float());
	else
		rnd::exposure.SetValue(1.0f);

	fog.InterpretGlobals(global, "fog");

	return true;
}