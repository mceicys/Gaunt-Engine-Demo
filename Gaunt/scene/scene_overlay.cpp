// scene_overlay.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"

namespace scn
{
	const size_t NUM_OVERLAYS = 2;
	Overlay overlays[NUM_OVERLAYS];
	Overlay skyOverlay;
}

/*
################################################################################################


	OVERLAY


################################################################################################
*/

const char* const res::Resource<scn::Overlay>::METATABLE = "metaOverlay";

/*--------------------------------------
	scn::Overlay::Overlay
--------------------------------------*/
scn::Overlay::Overlay() : numEnts(0), flags(0)
{
	AddLock(); // Permanent
}

/*--------------------------------------
	scn::Overlay::~Overlay
--------------------------------------*/
scn::Overlay::~Overlay()
{
	while(numEnts)
		RemoveEntity(*ents[0]);

	ents.Free();
}

/*--------------------------------------
	scn::Overlay::AddEntity
--------------------------------------*/
void scn::Overlay::AddEntity(const scn::Entity& ent)
{
	if(FindEntity(ent) != -1)
		return;

	ents.Ensure(numEnts + 1);
	ents[numEnts++].Set(&ent);
}

/*--------------------------------------
	scn::Overlay::RemoveEntity
--------------------------------------*/
void scn::Overlay::RemoveEntity(const scn::Entity& ent)
{
	size_t i = FindEntity(ent);
	
	if(i == -1)
		return;

	ents[i] = ents[--numEnts];
	ents[numEnts].Set(0); // Must do this to remove lock
}

/*--------------------------------------
	scn::Overlay::FindEntity
--------------------------------------*/
size_t scn::Overlay::FindEntity(const scn::Entity& ent)
{
	for(size_t i = 0; i < numEnts; i++)
	{
		if(ents[i].Value() == &ent)
			return i;
	}

	return -1;
}

/*--------------------------------------
	scn::Overlays
--------------------------------------*/
scn::Overlay* scn::Overlays()
{
	return overlays;
}

/*--------------------------------------
	scn::SkyOverlay
--------------------------------------*/
scn::Overlay& scn::SkyOverlay()
{
	return skyOverlay;
}

/*
################################################################################################


	OVERLAY LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::GetOverlay

IN	iIndex
OUT	ovrO
--------------------------------------*/
int scn::GetOverlay(lua_State* l)
{
	int index = lua_tointeger(l, 1);

	if(index < 0 || index >= NUM_OVERLAYS)
		luaL_argerror(l, 1, "out of bounds overlay index");

	overlays[index].LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::SkyOverlay

OUT	ovrSky
--------------------------------------*/
int scn::SkyOverlay(lua_State* l)
{
	skyOverlay.LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::OvrCamera (Camera)

IN	ovrO
OUT	[camC]
--------------------------------------*/
int scn::OvrCamera(lua_State* l)
{
	Overlay* o = scn::Overlay::CheckLuaTo(1);

	if(o->cam)
		o->cam->LuaPush();
	else
		lua_pushnil(l);

	return 1;
}

/*--------------------------------------
LUA	scn::OvrSetCamera (SetCamera)

IN	ovrO, [camC]
--------------------------------------*/
int scn::OvrSetCamera(lua_State* l)
{
	scn::Overlay::CheckLuaTo(1)->cam.Set(Camera::OptionalLuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	scn::OvrRelit (Relit)

IN	ovrO
OUT	bRelit
--------------------------------------*/
int scn::OvrRelit(lua_State* l)
{
	lua_pushboolean(l, scn::Overlay::CheckLuaTo(1)->flags & Overlay::RELIT);
	return 1;
}

/*--------------------------------------
LUA	scn::OvrSetRelit (SetRelit)

IN	ovrO, bRelit
--------------------------------------*/
int scn::OvrSetRelit(lua_State* l)
{
	Overlay* o = Overlay::CheckLuaTo(1);
	o->flags = com::FixedBits(o->flags, Overlay::RELIT, lua_toboolean(l, 2) != 0);
	return 0;
}