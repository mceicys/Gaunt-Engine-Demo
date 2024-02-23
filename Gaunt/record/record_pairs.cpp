// record_pairs.cpp
// Martynas Ceicys

#include "pairs.h"
#include "../../GauntCommon/io.h"
#include "../wrap/wrap.h"

namespace rec
{
	struct traverse_node
	{
		const com::JSVar* parent;

		union
		{
			const com::Pair<com::JSVar>* pair;
			size_t elem;
		};
	};

	struct traverse_node_mutable
	{
		com::JSVar* parent;

		union
		{
			com::Pair<com::JSVar>* pair;
			size_t elem;
		};
	};

	// DIFF
	bool	ApplyJSVarDiffPartial(com::JSVar& destIO, const com::JSVar& src,
			com::Arr<traverse_node_mutable>& sdIO, com::Arr<traverse_node>& ssIO,
			size_t& numNodesIO);
	bool	ApplyBasicJSVarDiff(com::JSVar& destIO, const com::JSVar& src);

	// PUSH
	bool	LuaPushJSVarPartial(lua_State* l, const com::JSVar& var,
			com::Arr<traverse_node>& stackIO, size_t& numNodesIO);
	int		LuaPushBasicJSVar(lua_State* l, const com::JSVar& var);

	// TO
	bool	LuaToJSVarPartial(lua_State* l, int index, com::JSVar& varOut,
			com::Arr<com::JSVar*>& stackIO, size_t& numNodesIO);
	bool	LuaToBasicJSVar(lua_State* l, int index, com::JSVar& varOut);
	bool	LuaIsArray(lua_State* l, int index);
}

#define DEFAULT_JSVAR_STACK_SIZE 8

/*
################################################################################################


	DIFF


################################################################################################
*/

/*--------------------------------------
	rec::ApplyJSVarDiff

FIXME: separate JSVar for deleting keys?
--------------------------------------*/
void rec::ApplyJSVarDiff(com::JSVar& dest, const com::JSVar& src)
{
	if(ApplyBasicJSVarDiff(dest, src))
		return;

	com::Arr<traverse_node_mutable> sd(DEFAULT_JSVAR_STACK_SIZE);
	com::Arr<traverse_node> ss(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;

	ApplyJSVarDiffPartial(dest, src, sd, ss, numNodes);

	while(numNodes)
	{
		size_t curNode = numNodes - 1;
		com::JSVar& pd = *sd[curNode].parent;
		const com::JSVar& ps = *ss[curNode].parent;

		if(pd.Type() != ps.Type())
			WRP_FATAL("Destination JSVar's type does not match source's type");

		if(ps.Type() == com::JSVar::OBJECT)
		{
			const com::Pair<com::JSVar>* pair = ss[curNode].pair;

			if(pair)
			{
				ss[curNode].pair = pair->Next();
				com::JSVar& cd = pd.Object()->Ensure(pair->Key())->Value();
				ApplyJSVarDiffPartial(cd, pair->Value(), sd, ss, numNodes);
			}
			else
				numNodes--;
		}
		else if(ps.Type() == com::JSVar::ARRAY)
		{
			size_t elem = ss[curNode].elem;

			if(elem < ps.NumElems())
			{
				ss[curNode].elem++;
				ApplyJSVarDiffPartial((*pd.Array())[elem], (*ps.Array())[elem], sd, ss, numNodes);
			}
			else
				numNodes--;
		}
		else
			WRP_FATAL("Unexpected JSVar type on traversal stack");
	}

	sd.Free();
	ss.Free();
}

/*--------------------------------------
	rec::ApplyJSVarDiffPartial
--------------------------------------*/
bool rec::ApplyJSVarDiffPartial(com::JSVar& dest, const com::JSVar& src,
	com::Arr<traverse_node_mutable>& sd, com::Arr<traverse_node>& ss, size_t& numNodes)
{
	if(ApplyBasicJSVarDiff(dest, src))
		return true;

	sd.Ensure(numNodes + 1);
	sd[numNodes].parent = &dest;
	ss.Ensure(numNodes + 1);
	ss[numNodes].parent = &src;

	if(src.Type() == com::JSVar::OBJECT)
	{
		if(dest.Type() != com::JSVar::OBJECT)
			dest.SetObject();

		sd[numNodes].pair = 0; // Not needed, dest pairs are found by key
		ss[numNodes].pair = src.Object()->First();
	}
	else
	{
		com::FreeJSON(dest);
		dest.SetArray();
		dest.SetNumElems(src.NumElems());
		sd[numNodes].elem = 0; // Not needed, using ss.elem
		ss[numNodes].elem = 0;
	}

	numNodes++;
	return false;
}

/*--------------------------------------
	rec::ApplyBasicJSVarDiff
--------------------------------------*/
bool rec::ApplyBasicJSVarDiff(com::JSVar& dest, const com::JSVar& src)
{
	if(dest.Type() != src.Type())
		com::FreeJSON(dest);

	switch(src.Type())
	{
	case com::JSVar::NONE:
		return true;
	case com::JSVar::NUMBER:
		dest.SetNumber(src.Double());
		return true;
	case com::JSVar::BOOLEAN:
		dest.SetBool(src.Bool());
		return true;
	case com::JSVar::STRING:
		dest.SetString(src.String());
		return true;
	}

	return false;
}

/*
################################################################################################


	PUSH


################################################################################################
*/

/*--------------------------------------
	rec::LuaPushJSVar
--------------------------------------*/
void rec::LuaPushJSVar(lua_State* l, const com::JSVar& var)
{
	if(LuaPushBasicJSVar(l, var))
		return;

	com::Arr<traverse_node> stack(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;

	LuaPushJSVarPartial(l, var, stack, numNodes);

	while(numNodes)
	{
		size_t curNode = numNodes - 1;
		const com::JSVar& parent = *stack[curNode].parent;

		if(parent.Type() == com::JSVar::OBJECT)
		{
			const com::Pair<com::JSVar>* pair = stack[curNode].pair;

			if(pair)
			{
				stack[curNode].pair = pair->Next();
				lua_pushstring(l, pair->Key());

				if(LuaPushJSVarPartial(l, pair->Value(), stack, numNodes))
					lua_rawset(l, -3);
				else
				{
					lua_pushvalue(l, -1);
					lua_insert(l, -3);
					lua_rawset(l, -4);
				}
			}
			else
			{
				if(numNodes > 1)
					lua_pop(l, 1);

				numNodes--;
			}
		}
		else if(parent.Type() == com::JSVar::ARRAY)
		{
			size_t elem = stack[curNode].elem;

			if(elem < parent.NumElems())
			{
				stack[curNode].elem++;

				if(LuaPushJSVarPartial(l, parent.Array()->o[elem], stack, numNodes))
					lua_rawseti(l, -2, elem + 1);
				else
				{
					lua_pushvalue(l, -1);
					lua_rawseti(l, -3, elem + 1);
				}
			}
			else
			{
				if(numNodes > 1)
					lua_pop(l, 1);

				numNodes--;
			}
		}
		else
			WRP_FATAL("Unexpected JSVar type on traversal stack");
	}

	stack.Free();
}

/*--------------------------------------
	rec::LuaPushJSVarPartial
--------------------------------------*/
bool rec::LuaPushJSVarPartial(lua_State* l, const com::JSVar& var,
	com::Arr<traverse_node>& stackIO, size_t& numNodesIO)
{
	if(LuaPushBasicJSVar(l, var))
		return true;

	stackIO.Ensure(numNodesIO + 1);
	stackIO[numNodesIO].parent = &var;

	if(var.Type() == com::JSVar::OBJECT)
	{
		stackIO[numNodesIO].pair = var.Object()->First();
		lua_createtable(l, 0, var.Object()->Num());
	}
	else
	{
		stackIO[numNodesIO].elem = 0;
		lua_createtable(l, var.NumElems(), 0);
	}

	numNodesIO++;
	return false;
}

/*--------------------------------------
	rec::LuaPushBasicJSVar
--------------------------------------*/
int rec::LuaPushBasicJSVar(lua_State* l, const com::JSVar& var)
{
	scr::EnsureStack(l, 1, "Cannot push JSVar");

	switch(var.Type())
	{
	case com::JSVar::NONE:
		lua_pushnil(l);
		return 1;
	case com::JSVar::NUMBER:
		lua_pushnumber(l, var.Double());
		return 1;
	case com::JSVar::BOOLEAN:
		lua_pushboolean(l, var.Bool());
		return 1;
	case com::JSVar::STRING:
		lua_pushstring(l, var.String());
		return 1;
	}

	return 0;
}

/*
################################################################################################


	TO


################################################################################################
*/

/*--------------------------------------
	rec::LuaToJSVar
--------------------------------------*/
void rec::LuaToJSVar(lua_State* l, int index, com::JSVar& varOut)
{
	com::FreeJSON(varOut);

	if(LuaToBasicJSVar(l, index, varOut))
		return;

	char buf[COM_DBL_PRNT_LEN + 1];
	com::Arr<com::JSVar*> stack(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;
	
	LuaToJSVarPartial(l, index, varOut, stack, numNodes);

	while(numNodes)
	{
		scr::EnsureStack(l, 1);

		if(lua_next(l, -2))
		{
			com::JSVar& parent = *stack[numNodes - 1];

			if(parent.Type() == com::JSVar::OBJECT)
			{
				com::PairMap<com::JSVar>& obj = *parent.Object();
				com::Pair<com::JSVar>* pair = 0;

				int keyType = lua_type(l, -2);
				if(keyType == LUA_TSTRING)
					pair = obj.Ensure(lua_tostring(l, -2));
				else if(keyType == LUA_TNUMBER)
				{
					com::SNPrintF(buf, sizeof(buf), 0, COM_DBL_PRNT, lua_tonumber(l, -2));
					pair = obj.Ensure(buf);
				}
				else if(keyType == LUA_TBOOLEAN)
					pair = obj.Ensure(lua_toboolean(l, -2) ? "true" : "false");

				if(pair)
				{
					if(LuaToJSVarPartial(l, -1, pair->Value(), stack, numNodes))
						lua_pop(l, 1);
				}
				else
					lua_pop(l, 1); // Skip unsupported key type
			}
			else if(parent.Type() == com::JSVar::ARRAY)
			{
				com::Arr<com::JSVar>& arr = *parent.Array();
				int elem = lua_tointeger(l, -2) - 1;
				
				if(elem + 1 > parent.NumElems())
					parent.SetNumElems(elem + 1);

				if(LuaToJSVarPartial(l, -1, arr[elem], stack, numNodes))
					lua_pop(l, 1);
			}
			else
				WRP_FATAL("Unexpected JSVar type on traversal stack");
		}
		else
		{
			if(lua_gettop(l) != index) // Don't pop original value
				lua_pop(l, 1);

			numNodes--;
		}
	}

	stack.Free();
}

/*--------------------------------------
	rec::LuaToJSVarPartial
--------------------------------------*/
bool rec::LuaToJSVarPartial(lua_State* l, int index, com::JSVar& varOut,
	com::Arr<com::JSVar*>& stackIO, size_t& numNodesIO)
{
	if(LuaToBasicJSVar(l, index, varOut))
		return true;

	if(LuaIsArray(l, index))
		varOut.SetArray()->Init(1);
	else
		varOut.SetObject();

	stackIO.Ensure(numNodesIO + 1);
	stackIO[numNodesIO] = &varOut;
	numNodesIO++;

	scr::EnsureStack(l, 1);
	lua_pushnil(l); // First key for lua_next
	return false;
}

/*--------------------------------------
	rec::LuaToBasicJSVar

Returns false if index points to a table. Otherwise, sets varOut and returns true. Functions,
userdata, threads, and light userdata are not supported and set varOut to JSVar::NONE.
--------------------------------------*/
bool rec::LuaToBasicJSVar(lua_State* l, int index, com::JSVar& varOut)
{
	switch(lua_type(l, index))
	{
	case LUA_TTABLE:
		return false;
	case LUA_TNIL:
		varOut.Free();
		break;
	case LUA_TNUMBER:
		varOut.SetNumber(lua_tonumber(l, index));
		break;
	case LUA_TBOOLEAN:
		varOut.SetBool(lua_toboolean(l, index));
		break;
	case LUA_TSTRING:
		varOut.SetString(lua_tostring(l, index));
		break;
	default:
		varOut.Free();
	}

	return true;
}

/*--------------------------------------
	rec::LuaIsArray

Returns true if value at index is a table that only contains sequential keys starting from 1.
--------------------------------------*/
bool rec::LuaIsArray(lua_State* l, int index)
{
	index = lua_absindex(l, index);

	if(lua_type(l, index) != LUA_TTABLE)
		return false;

	int firstType = lua_rawgeti(l, index, 1);
	lua_pop(l, 1);

	if(firstType == LUA_TNIL)
		return false;

	size_t range = lua_rawlen(l, index);

	// Check whether any keys exist outside the range
	lua_pushnil(l);
	while(lua_next(l, index))
	{
		lua_pop(l, 1);
		int i = lua_tointeger(l, -1);

		if(!lua_isinteger(l, -1) || i < 1 || i > range)
		{
			lua_pop(l, 1);
			return false;
		}
	}

	return true;
}