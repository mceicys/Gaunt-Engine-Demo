// scene_world_lua.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../vector/vec_lua.h"

/*
################################################################################################


	WORLD LUA


################################################################################################
*/

/*--------------------------------------
LUA	scn::LoadWorld

IN	sFileName
--------------------------------------*/
int scn::LoadWorld(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	LoadWorld(fileName);
	return 0;
}

/*--------------------------------------
LUA	scn::WorldBox

OUT	xMin, yMin, zMin, xMax, yMax, zMax
--------------------------------------*/
int scn::WorldBox(lua_State* l)
{
	vec::LuaPushVec(l, WorldMin());
	vec::LuaPushVec(l, WorldMax());
	return 6;
}

/*--------------------------------------
LUA	scn::NearbyTriangle

IN	v3Pos
OUT	[iNode, iTri]
--------------------------------------*/
int scn::NearbyTriangle(lua_State* l)
{
	com::Vec3 pos = vec::LuaToVec(l, 1, 2, 3);
	const WorldNode* leaf;
	const leaf_triangle* tri = NearbyTriangle(WorldRoot(), pos, leaf);

	if(tri)
	{
		leaf->LuaPush(l);
		lua_pushinteger(l, tri - leaf->triangles);
		return 2;
	}

	return 0;
}

/*--------------------------------------
LUA	scn::PosToLeaf

IN	v3Pos
OUT	[iNode]
--------------------------------------*/
int scn::PosToLeaf(lua_State* l)
{
	const WorldNode* leaf = (const WorldNode*)PosToLeaf(WorldRoot(), vec::LuaToVec(l, 1, 2, 3));
	
	if(!leaf)
		return 0;

	leaf->LuaPush(l);
	return 1;
}

/*--------------------------------------
LUA	scn::RootNode

OUT	iNodeRoot
--------------------------------------*/
int scn::RootNode(lua_State* l)
{
	WorldNode* root = WorldRoot();
	root ? root->LuaPush(l) : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeLeft

IN	iNode
OUT	iNodeLeft
--------------------------------------*/
int scn::NodeLeft(lua_State* l)
{
	WorldNode* left = (WorldNode*)CheckLuaToWorldNode(l, 1)->left;
	left ? left->LuaPush(l) : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeRight

IN	iNode
OUT	iNodeRight
--------------------------------------*/
int scn::NodeRight(lua_State* l)
{
	WorldNode* right = (WorldNode*)CheckLuaToWorldNode(l, 1)->right;
	right ? right->LuaPush(l) : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeChildren

IN	iNode
OUT	iNodeLeft, iNodeRight
--------------------------------------*/
int scn::NodeChildren(lua_State* l)
{
	WorldNode* node = CheckLuaToWorldNode(l, 1);
	WorldNode* left = (WorldNode*)node->left;
	WorldNode* right = (WorldNode*)node->right;
	left ? left->LuaPush(l) : lua_pushnil(l);
	right ? right->LuaPush(l) : lua_pushnil(l);
	return 2;
}

/*--------------------------------------
LUA	scn::NodeParent

IN	iNode
OUT	iNodeParent
--------------------------------------*/
int scn::NodeParent(lua_State* l)
{
	WorldNode* parent = (WorldNode*)CheckLuaToWorldNode(l, 1)->parent;
	parent ? parent->LuaPush(l) : lua_pushnil(l);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeSolid

IN	iNode
OUT	bSolid
--------------------------------------*/
int scn::NodeSolid(lua_State* l)
{
	lua_pushboolean(l, CheckLuaToWorldNode(l, 1)->solid);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeNumEntities

IN	iNode
OUT	iNumEnts
--------------------------------------*/
int scn::NodeNumEntities(lua_State* l)
{
	lua_pushinteger(l, CheckLuaToWorldNode(l, 1)->numEntLinks);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeEntities

IN	iNode, iEnt
OUT	entLinked
--------------------------------------*/
int scn::NodeEntity(lua_State* l)
{
	WorldNode* node = CheckLuaToWorldNode(l, 1);
	size_t ent = (size_t)lua_tointeger(l, 2);

	if(ent >= node->numEntLinks)
		luaL_argerror(l, 2, "Entity link index out of bounds");

	node->entLinks[ent].obj->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	scn::NodeNumTriangles

IN	iNode
OUT	iNumTriangles
--------------------------------------*/
int scn::NodeNumTriangles(lua_State* l)
{
	lua_pushinteger(l, CheckLuaToWorldNode(l, 1)->numTriangles);
	return 1;
}

/*--------------------------------------
LUA	scn::NodeTriangleTexture

IN	iNode, iTri
OUT	texTri
--------------------------------------*/
int scn::NodeTriangleTexture(lua_State* l)
{
	CheckLuaToLeafTriangle(l, 1, 2)->tex->LuaPush();
	return 1;
}