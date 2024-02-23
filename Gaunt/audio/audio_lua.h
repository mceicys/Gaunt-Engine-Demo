// audio_lua.h
// Martynas Ceicys

#ifndef AUDIO_LUA_H
#define AUDIO_LUA_H

#include "../script/script.h"

namespace aud
{
	// SOUND LUA
	int FindSound(lua_State* l);
	int EnsureSound(lua_State* l);

	int SndFileName(lua_State* l);
	int SndNumFrames(lua_State* l);
	int SndNumChannels(lua_State* l);
	int SndNumSamples(lua_State* l);
	int SndFrameRate(lua_State* l);
	int SndSampleRate(lua_State* l);
	int SndSeconds(lua_State* l);

	// VOICE LUA
	int CreateVoice(lua_State* l);

	int VoxPos(lua_State* l);
	int VoxSetPos(lua_State* l);
	int VoxOldPos(lua_State* l);
	int VoxSetOldPos(lua_State* l);
	int VoxFinalPos(lua_State* l);
	int VoxRadius(lua_State* l);
	int VoxSetRadius(lua_State* l);
	int VoxOldRadius(lua_State* l);
	int VoxSetOldRadius(lua_State* l);
	int VoxFinalRadius(lua_State* l);
	int VoxSrcRadius(lua_State* l);
	int VoxSetSrcRadius(lua_State* l);
	int VoxOldSrcRadius(lua_State* l);
	int VoxSetOldSrcRadius(lua_State* l);
	int VoxFinalSrcRadius(lua_State* l);
	int VoxVolume(lua_State* l);
	int VoxVolumeTarget(lua_State* l);
	int VoxSetVolumeTarget(lua_State* l);
	int VoxVolumeTime(lua_State* l);
	int VoxFadeVolume(lua_State* l);
	int VoxLerpPos(lua_State* l);
	int VoxSetLerpPos(lua_State* l);
	int VoxLerpRadius(lua_State* l);
	int VoxSetLerpRadius(lua_State* l);
	int VoxLerpSrcRadius(lua_State* l);
	int VoxSetLerpSrcRadius(lua_State* l);
	int VoxLoop(lua_State* l);
	int VoxSetLoop(lua_State* l);
	int VoxBackground(lua_State* l);
	int VoxSetBackground(lua_State* l);
	int VoxLogarithmic(lua_State* l);
	int VoxSetLogarithmic(lua_State* l);
	int VoxPlay(lua_State* l);
	int VoxLoudness(lua_State* l);
	int VoxPlaying(lua_State* l);
}

#endif