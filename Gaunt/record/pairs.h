// pairs.h
// Martynas Ceicys

// FIXME: Take out of the record system; common should be able to provide Lua code

#ifndef REC_PAIRS_H
#define REC_PAIRS_H

#include "../../GauntCommon/json.h"
#include "../script/script.h"

namespace rec
{

void ApplyJSVarDiff(com::JSVar& destIO, const com::JSVar& src);
void LuaPushJSVar(lua_State* l, const com::JSVar& var);
void LuaToJSVar(lua_State* l, int index, com::JSVar& varOut);

}

#endif