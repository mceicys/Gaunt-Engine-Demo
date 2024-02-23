// render_lua.h
// Martynas Ceicys

#ifndef RENDER_LUA_H
#define RENDER_LUA_H

#include "../script/script.h"

namespace rnd
{
	// PALETTE LUA
	int FindPalette(lua_State* l);
	int EnsurePalette(lua_State* l);

	int PalFileName(lua_State* l);
	int PalNumSubPalettes(lua_State* l);
	int PalDelete(lua_State* l);

	int CurrentPalette(lua_State* l);
	int SetPalette(lua_State* l);

	// TEXTURE LUA
	int FindTexture(lua_State* l);
	int EnsureTexture(lua_State* l);

	int TexFileName(lua_State* l);
	int TexShortName(lua_State* l);
	int TexNumFrames(lua_State* l);
	int TexFrameDims(lua_State* l);
	int TexMip(lua_State* l);
	int TexSetMip(lua_State* l);
	int TexDelete(lua_State* l);

	// SOCKET LUA
	int SocPlace(lua_State* l);
	int SocPlaceEntity(lua_State* l);
	int SocPlaceAlign(lua_State* l);

	// ANIMATION LUA
	int AniFrames(lua_State* l);
	int AniEnd(lua_State* l);
	int AniNumFrames(lua_State* l);

	// MESH LUA
	int FindMesh(lua_State* l);
	int EnsureMesh(lua_State* l);

	int MshFileName(lua_State* l);
	int MshFlags(lua_State* l);
	int MshNumFrames(lua_State* l);
	int MshFrameRate(lua_State* l);
	int MshSocket(lua_State* l);
	int MshNumSockets(lua_State* l);
	int MshAnimation(lua_State* l);
	int MshNumAnimations(lua_State* l);
	int MshBounds(lua_State* l);
	int MshRadius(lua_State* l);
	int MshDelete(lua_State* l);

	// CURVE LUA
	int SetCurvePoint(lua_State* l);
	int CurvePoint(lua_State* l);
	int NumCurvePoints(lua_State* l);
	int FindCurvePoint(lua_State* l);
	int DeleteCurvePoint(lua_State* l);
	int ClearCurvePoints(lua_State* l);

	// SKY LUA
	int SetSkyFace(lua_State* l);
	int SkySphereTexture(lua_State* l);
	int SetSkySphereTexture(lua_State* l);

	// LINE LUA
	int DrawLine(lua_State* l);
	int Draw2DLine(lua_State* l);
	int DrawSphereLines(lua_State* l);

	// SPACE LUA
	int LensDirection(lua_State* l);
	int VecUserToWorldZPlane(lua_State* l);
	int VecWorldToUser(lua_State* l);

	// SHADOW LUA
	int CalculateCascadeDistances(lua_State* l);
}

#endif