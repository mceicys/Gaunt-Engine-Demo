// audio_voice.cpp
// Martynas Ceicys

#include <math.h>

#include "audio.h"
#include "audio_lua.h"
#include "../../GauntCommon/math.h"
#include "../vector/vec_lua.h"

/*
################################################################################################
	
	
	VOICE


################################################################################################
*/

const char* const res::Resource<aud::Voice>::METATABLE = "metaVoice";

/*--------------------------------------
	aud::Voice::Voice
--------------------------------------*/
aud::Voice::Voice(const com::Vec3& pos, float radius, float srcRadius, float volume)
	: pos(pos), oldPos(pos), radius(radius), oldRadius(radius), srcRadius(srcRadius),
	oldSrcRadius(srcRadius), volume(volume), volumeTarget(volume), volumeTime(0.0f),
	flags(LERP_POS | LERP_RADIUS | LERP_SRC_RADIUS), curSample(0)
{
	EnsureLink();
}

/*--------------------------------------
	aud::Voice::FinalPos
--------------------------------------*/
com::Vec3 aud::Voice::FinalPos() const
{
	if(flags & LERP_POS)
		return COM_LERP(oldPos, pos, wrp::Fraction());
	else
		return pos;
}

/*--------------------------------------
	aud::Voice::FinalRadius
--------------------------------------*/
float aud::Voice::FinalRadius() const
{
	if(flags & LERP_RADIUS)
		return COM_LERP(oldRadius, radius, wrp::Fraction());
	else
		return radius;
}

/*--------------------------------------
	aud::Voice::FinalSrcRadius
--------------------------------------*/
float aud::Voice::FinalSrcRadius() const
{
	if(flags & LERP_SRC_RADIUS)
		return COM_LERP(oldSrcRadius, srcRadius, wrp::Fraction());
	else
		return srcRadius;
}

/*--------------------------------------
	aud::Voice::Volume

FIXME: This volume model may cause glitches if mixer time and sim time desync

FIXME: Need sub-frame volume fading done in mixer
	Also means the volume var needs to be permanently updated each aud::Update, not once a tick
--------------------------------------*/
float aud::Voice::Volume(float timeAdd) const
{
	if(volumeTime)
	{
		float f = timeAdd / volumeTime;

		if(f > 1.0f)
			f = 1.0f;

		return COM_LERP(volume, volumeTarget, f);
	}
	else
		return volumeTarget;
}

/*--------------------------------------
	aud::Voice::FadeVolume
--------------------------------------*/
void aud::Voice::FadeVolume(float v, float t)
{
	volumeTarget = v;
	volumeTime = t;
}

/*--------------------------------------
	aud::Voice::Play
--------------------------------------*/
void aud::Voice::Play(const Sound* s, float t)
{
	if(s)
	{
		if(!snd)
			AddLock(); // Don't garbage collect while playing

		snd.Set(s);
		curSample = size_t((t / s->Seconds()) * s->NumFrames()) * s->NumChannels();
		Advance(0);
	}
	else
	{
		if(snd)
			RemoveLock();

		snd.Set(0);
		curSample = 0;
	}
}

/*--------------------------------------
	aud::Voice::Loudness
--------------------------------------*/
float aud::Voice::Loudness(float ahead) const
{
	if(!snd || ahead <= 0.0f)
		return 0.0f;

	const Sound& s = *snd;
	unsigned numFramesAhead = ahead * s.FrameRate();
	unsigned numChannels = s.NumChannels();
	unsigned limit = s.NumFrames() - curSample / numChannels;

	if(numFramesAhead > limit)
		numFramesAhead = limit;

	if(!numFramesAhead)
		return 0.0f;

	unsigned numSamplesAhead = numFramesAhead * numChannels;
	const float* smps = s.Samples();
	float sum = 0.0f;

	for(unsigned i = 0; i < numSamplesAhead; i++)
		sum += fabs(smps[curSample + i]);


	return sum / (float)numSamplesAhead;
}

/*--------------------------------------
	aud::Voice::SaveOld
--------------------------------------*/
void aud::Voice::SaveOld()
{
	oldPos = pos;
	oldRadius = radius;
	oldSrcRadius = srcRadius;
	float step = wrp::timeStep.Float();
	volume = Volume(step);
	volumeTime = com::Max(0.0f, volumeTime - step);
}

/*--------------------------------------
	aud::Voice::Advance

Returns true if still playing. numSamples must be a multiple of sound's numChannels.
--------------------------------------*/
bool aud::Voice::Advance(size_t numSamples)
{
	if(!snd)
		return false;

	curSample += numSamples;

	if(curSample >= snd->NumSamples())
	{
		if(flags & LOOP)
			curSample %= snd->NumSamples();
		else
		{
			RemoveLock();
			snd.Set(0);
			curSample = 0;
			return false;
		}
	}

	return true;
}

/*
################################################################################################
	
	
	VOICE LUA


################################################################################################
*/

/*--------------------------------------
LUA	aud::CreateVoice (Voice)

IN	v3Pos, nRadius, nSrcRadius, nVolume
OUT	vox
--------------------------------------*/
int aud::CreateVoice(lua_State* l)
{
	com::Vec3 pos = vec::LuaToVec(l, 1, 2, 3);
	float radius = lua_tonumber(l, 4);
	float srcRadius = lua_tonumber(l, 5);
	float volume = lua_tonumber(l, 6);
	Voice* v = new aud::Voice(pos, radius, srcRadius, volume);
	v->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	aud::VoxPos (Pos)

IN	voxV
OUT	v3Pos
--------------------------------------*/
int aud::VoxPos(lua_State* l)
{
	vec::LuaPushVec(l, Voice::CheckLuaTo(1)->pos);
	return 3;
}

/*--------------------------------------
LUA	aud::VoxSetPos (SetPos)

IN	voxV, v3Pos
--------------------------------------*/
int aud::VoxSetPos(lua_State* l)
{
	Voice::CheckLuaTo(1)->pos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxOldPos (OldPos)

IN	voxV
OUT	v3OldPos
--------------------------------------*/
int aud::VoxOldPos(lua_State* l)
{
	vec::LuaPushVec(l, Voice::CheckLuaTo(1)->oldPos);
	return 3;
}

/*--------------------------------------
LUA	aud::VoxSetOldPos (SetOldPos)

IN	voxV, v3OldPos
--------------------------------------*/
int aud::VoxSetOldPos(lua_State* l)
{
	Voice::CheckLuaTo(1)->oldPos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxFinalPos (FinalPos)

IN	voxV
OUT	v3FinalPos
--------------------------------------*/
int aud::VoxFinalPos(lua_State* l)
{
	vec::LuaPushVec(l, Voice::CheckLuaTo(1)->FinalPos());
	return 3;
}

/*--------------------------------------
LUA	aud::VoxRadius (Radius)

IN	voxV
OUT	nRadius
--------------------------------------*/
int aud::VoxRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->radius);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetRadius (SetRadius)

IN	voxV, nRadius
--------------------------------------*/
int aud::VoxSetRadius(lua_State* l)
{
	Voice::CheckLuaTo(1)->radius = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxOldRadius (OldRadius)

IN	voxV
OUT	nOldRadius
--------------------------------------*/
int aud::VoxOldRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->oldRadius);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetOldRadius (SetOldRadius)

IN	voxV, nOldRadius
--------------------------------------*/
int aud::VoxSetOldRadius(lua_State* l)
{
	Voice::CheckLuaTo(1)->oldRadius = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxFinalRadius (FinalRadius)

IN	voxV
OUT	nFinalRadius
--------------------------------------*/
int aud::VoxFinalRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->FinalRadius());
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSrcRadius (SrcRadius)

IN	voxV
OUT	nSrcRadius
--------------------------------------*/
int aud::VoxSrcRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->srcRadius);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetSrcRadius (SetSrcRadius)

IN	voxV, nSrcRadius
--------------------------------------*/
int aud::VoxSetSrcRadius(lua_State* l)
{
	Voice::CheckLuaTo(1)->srcRadius = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxOldSrcRadius (OldSrcRadius)

IN	voxV
OUT	nOldSrcRadius
--------------------------------------*/
int aud::VoxOldSrcRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->oldSrcRadius);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetOldSrcRadius (SetOldSrcRadius)

IN	voxV, nOldSrcRadius
--------------------------------------*/
int aud::VoxSetOldSrcRadius(lua_State* l)
{
	Voice::CheckLuaTo(1)->oldSrcRadius = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxFinalSrcRadius (FinalSrcRadius)

IN	voxV
OUT	nFinalSrcRadius
--------------------------------------*/
int aud::VoxFinalSrcRadius(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->FinalSrcRadius());
	return 1;
}

/*--------------------------------------
LUA	aud::VoxVolume (Volume)

IN	voxV, [nTimeAdd = 0]
OUT	nVolume
--------------------------------------*/
int aud::VoxVolume(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->Volume(lua_tonumber(l, 2)));
	return 1;
}

/*--------------------------------------
LUA	aud::VoxVolumeTarget (VolumeTarget)

IN	voxV
OUT	nVolumeTarget
--------------------------------------*/
int aud::VoxVolumeTarget(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->volumeTarget);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetVolumeTarget (SetVolumeTarget)

IN	voxV, nVolumeTarget
--------------------------------------*/
int aud::VoxSetVolumeTarget(lua_State* l)
{
	Voice::CheckLuaTo(1)->volumeTarget = lua_tonumber(l, 2);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxVolumeTime (VolumeTime)

IN	voxV
OUT	nVolumeTime
--------------------------------------*/
int aud::VoxVolumeTime(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->VolumeTime());
	return 1;
}

/*--------------------------------------
LUA	aud::VoxFadeVolume (FadeVolume)

IN	voxV, nTarget, nTime
--------------------------------------*/
int aud::VoxFadeVolume(lua_State* l)
{
	Voice::CheckLuaTo(1)->FadeVolume(lua_tonumber(l, 2), lua_tonumber(l, 3));
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLerpPos (LerpPos)

IN	voxV
OUT	bLerpPos
--------------------------------------*/
int aud::VoxLerpPos(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::LERP_POS);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetLerpPos (SetLerpPos)

IN	voxV, bLerpPos
--------------------------------------*/
int aud::VoxSetLerpPos(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::LERP_POS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLerpRadius (LerpRadius)

IN	voxV
OUT	bLerpRadius
--------------------------------------*/
int aud::VoxLerpRadius(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::LERP_RADIUS);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetLerpRadius (SetLerpRadius)

IN	voxV, bLerpRadius
--------------------------------------*/
int aud::VoxSetLerpRadius(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::LERP_RADIUS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLerpSrcRadius (LerpSrcRadius)

IN	voxV
OUT	bLerpSrcRadius
--------------------------------------*/
int aud::VoxLerpSrcRadius(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::LERP_SRC_RADIUS);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetLerpSrcRadius (SetLerpSrcRadius)

IN	voxV, bLerpSrcRadius
--------------------------------------*/
int aud::VoxSetLerpSrcRadius(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::LERP_SRC_RADIUS, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLoop (Loop)

IN	voxV
OUT	bLoop
--------------------------------------*/
int aud::VoxLoop(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::LOOP);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetLoop (SetLoop)

IN	voxV, bLoop
--------------------------------------*/
int aud::VoxSetLoop(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::LOOP, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxBackground (Background)

IN	voxV
OUT	bBackground
--------------------------------------*/
int aud::VoxBackground(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::BACKGROUND);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetBackground (SetBackground)

IN	voxV, bBackground
--------------------------------------*/
int aud::VoxSetBackground(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::BACKGROUND, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLogarithmic (Logarithmic)

IN	voxV
OUT	bLogarithmic
--------------------------------------*/
int aud::VoxLogarithmic(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->flags & Voice::LOGARITHMIC);
	return 1;
}

/*--------------------------------------
LUA	aud::VoxSetLogarithmic (SetLogarithmic)

IN	voxV, bLogarithmic
--------------------------------------*/
int aud::VoxSetLogarithmic(lua_State* l)
{
	Voice* v = Voice::CheckLuaTo(1);
	v->flags = com::FixedBits(v->flags, Voice::LOGARITHMIC, lua_toboolean(l, 2) != 0);
	return 0;
}

/*--------------------------------------
LUA	aud::VoxPlay (Play)

IN	voxV, sndS, nTime
--------------------------------------*/
int aud::VoxPlay(lua_State* l)
{
	Voice::CheckLuaTo(1)->Play(Sound::OptionalLuaTo(2), lua_tonumber(l, 3));
	return 0;
}

/*--------------------------------------
LUA	aud::VoxLoudness (Loudness)

IN	voxV, nAhead
OUT	nLoudness
--------------------------------------*/
int aud::VoxLoudness(lua_State* l)
{
	lua_pushnumber(l, Voice::CheckLuaTo(1)->Loudness(lua_tonumber(l, 2)));
	return 1;
}

/*--------------------------------------
LUA	aud::VoxPlaying (Playing)

IN	voxV
OUT	bPlaying
--------------------------------------*/
int aud::VoxPlaying(lua_State* l)
{
	lua_pushboolean(l, Voice::CheckLuaTo(1)->Playing());
	return 1;
}