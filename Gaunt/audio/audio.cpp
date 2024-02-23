// audio.cpp -- Library/device independent mixer
// Martynas Ceicys

#include <stdlib.h>
#include <stdint.h>

#include "audio.h"
#include "audio_lua.h"
#include "audio_private.h"
#include "../console/console.h"
#include "../scene/scene.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

namespace aud
{
	con::Option
		masterVolume("aud_master_volume", 1.0f),
		maxAttenuation("aud_max_attenuation", 10.0f);

	// FIXME: struct
	static const float EXP1 = exp(1.0f);
	float *mix, *mixWrite, *mixEnd;
	float* mixRead; // Only library-dependent code may read/set this between LibInit and LibCleanUp
	unsigned numMixFrames, numMixSamples, mixFrameRate;
	__declspec(align(8)) volatile uint32_t numReadyFrames; // Atomic operations only
}

/*--------------------------------------
	aud::Update

FIXME: Optimize iteration over buffer
FIXME: Separate thread?
	Lock individual voices and active camera when editing/mixing
	Keep fraction from last rendered frame (so sounds go with visuals)
	...
FIXME: Variable mixer-to-library latency
	Keep latency at small safe value ~1/(updates per second), [0.001, 1] seconds
--------------------------------------*/
void aud::Update()
{
	LibUpdate();
	scn::Camera* cam = scn::ActiveCamera();

	if(!cam)
		return;

	unsigned saveNumReadyFrames;

	// x86 atomic fetch (assuming 4-byte alignment)
	__asm
	{
		mov eax, numReadyFrames
		mov saveNumReadyFrames, eax
	}

	unsigned numWrite = numMixFrames - saveNumReadyFrames;

	if(!numWrite)
		return;

	unsigned start, end;
	MixBufRange(mixWrite, numWrite, start, end);
	unsigned cur = start;

	// Clear samples
	do
	{
		for(unsigned i = 0; i < AUD_NUM_MIX_CHANNELS; i++, cur++)
			mix[cur] = 0.0f;

		cur %= numMixSamples;
	} while(cur != end);

	// Mix
	com::Vec3 camPos = cam->FinalPos();
	com::Qua camOri = cam->FinalOri();
	float volTimeAdd = wrp::timeStep.Float() * wrp::Fraction();

	for(com::linker<Voice>* it = Voice::List().f; it; it = it->next)
	{
		Voice& v = *it->o;

		if(!v.snd)
			continue;

		const Sound& snd = *v.snd;

		if(snd.FrameRate() != mixFrameRate)
			continue; // FIXME: resample while mixing or when sound is loaded

		float rad = v.FinalRadius();
		float attn, left, right;

		if(v.flags & v.BACKGROUND)
			attn = left = right = 1.0f;
		else if(rad <= 0.0f)
		{
			v.Advance(numWrite * snd.NumChannels()); // FIXME: depends on sample rate being equal
			continue;
		}
		else
		{
			com::Vec3 dif = v.FinalPos() - camPos;
			float mag = dif.Mag();

			if(v.flags & v.LOGARITHMIC)
			{
				if(mag > 0.0f)
				{
					attn = 1.0f - log(mag * (EXP1 / rad));

					if(attn > maxAttenuation.Float()) // FIXME: change attenuation so it doesn't need to be clamped?
						attn = maxAttenuation.Float();
				}
				else
					attn = maxAttenuation.Float();
			}
			else
				attn = (rad - mag) / rad;

			if(attn < 0.0f)
			{
				v.Advance(numWrite * snd.NumChannels()); // FIXME: depends on sample rate being equal
				continue;
			}

			dif = com::VecRotInv(dif, camOri);
			
			if(!mag)
				left = right = 1.0f;
			else
			{
				float ny = dif.y / mag;
				float srcRad = v.FinalSrcRadius();
				float hard = !srcRad || srcRad <= mag ? 1.0f : mag / srcRad;

				if(ny >= 0.0f)
				{
					left = 1.0f;
					right = 1.0f - ny * hard;
				}
				else
				{
					left = 1.0f + ny * hard;
					right = 1.0f;
				}
			}
		}

		const float* ss = snd.Samples();
		float amp = v.Volume(volTimeAdd) * attn * masterVolume.Float();
		cur = start;

		if(snd.NumChannels() == 1) // FIXME: template function?
		{
			do
			{
				float mono = ss[v.curSample] * amp;
				mix[cur] += mono * left;
				mix[cur + 1] += mono * right;
				cur = (cur + AUD_NUM_MIX_CHANNELS) % numMixSamples;

				/* FIXME: Advancing every frame is probably slow
				Prepare range until some event (stop/loop), then do fast iteration and mixing */
				if(!v.Advance(1))
					break;
			} while(cur != end);
		}
		else
		{
			// FIXME: approach mono as src radius is exited?
			do
			{
				mix[cur] += ss[v.curSample] * amp;
				mix[cur + 1] += ss[v.curSample + 1] * amp;
				cur = (cur + AUD_NUM_MIX_CHANNELS) % numMixSamples;

				if(!v.Advance(snd.NumChannels()))
					break;
			} while(cur != end);
		}
	}

	// Clip
	cur = start;

	do
	{
		for(unsigned i = 0; i < AUD_NUM_MIX_CHANNELS; i++, cur++)
		{
			if(mix[cur] > 1.0f)
				mix[cur] = 1.0f;
			else if(mix[cur] < -1.0f)
				mix[cur] = -1.0f;
		}

		cur %= numMixSamples;
	} while(cur != end);

	mixWrite = mix + end;

	// x86 atomic add
	__asm
	{
		mov eax, numWrite
		lock add numReadyFrames, eax
	}
}

/*--------------------------------------
	aud::SaveOld
--------------------------------------*/
void aud::SaveOld()
{
	for(com::linker<Voice>* it = Voice::List().f; it; it = it->next)
		it->o->SaveOld();
}

/*--------------------------------------
	aud::StopVoices
--------------------------------------*/
void aud::StopVoices()
{
	for(com::linker<Voice>* it = Voice::List().f, *next; it; it = it->next)
		it->o->Play(0);
}

/*--------------------------------------
	aud::DeleteUnused
--------------------------------------*/
void aud::DeleteUnused()
{
	for(com::linker<Sound>* it = Sound::List().f, *next; it; it = next)
	{
		next = it->next;

		if(!it->o->Used())
			delete it->o;
	}

	for(com::linker<Voice>* it = Voice::List().f, *next; it; it = next)
	{
		next = it->next;

		if(!it->o->Used())
			delete it->o;
	}
}

/*--------------------------------------
	aud::Init
--------------------------------------*/
bool aud::Init()
{
	uintptr_t addr = (uintptr_t)(&numReadyFrames);

	// FIXME: x86 specific; make shared memory portable or add preprocessor conditions
	if((addr & 0x3) != 0)
		WRP_FATAL("Bad build: numReadyFrames is not 4-byte aligned");

	mixFrameRate = 44100; // FIXME: configurable?
	numMixFrames = 2048; // FIXME: variable latency
	numMixSamples = numMixFrames * AUD_NUM_MIX_CHANNELS;
	mix = (float*)malloc(numMixFrames * AUD_NUM_MIX_CHANNELS * sizeof(float));
	mixWrite = mixRead = mix;
	mixEnd = mix + numMixFrames * AUD_NUM_MIX_CHANNELS + 1;
	numReadyFrames = 0;
	bool good = LibInit();

	// Lua
	luaL_Reg regs[] =
	{
		{"FindSound", FindSound},
		{"EnsureSound", EnsureSound},
		{"Voice", CreateVoice},
		{0, 0}
	};

	luaL_Reg sndRegs[] =
	{
		{"__gc", Sound::CLuaDelete},
		{"FileName", SndFileName},
		{"NumFrames", SndNumFrames},
		{"NumChannels", SndNumChannels},
		{"NumSamples", SndNumSamples},
		{"FrameRate", SndFrameRate},
		{"SampleRate", SndSampleRate},
		{"Seconds", SndSeconds},
		{0, 0}
	};

	luaL_Reg voxRegs[] =
	{
		{"__gc", Voice::CLuaDelete},
		{"Pos", VoxPos},
		{"SetPos", VoxSetPos},
		{"OldPos", VoxOldPos},
		{"SetOldPos", VoxSetOldPos},
		{"FinalPos", VoxFinalPos},
		{"Radius", VoxRadius},
		{"SetRadius", VoxSetRadius},
		{"OldRadius", VoxOldRadius},
		{"SetOldRadius", VoxSetOldRadius},
		{"FinalRadius", VoxFinalRadius},
		{"SrcRadius", VoxSrcRadius},
		{"SetSrcRadius", VoxSetSrcRadius},
		{"OldSrcRadius", VoxOldSrcRadius},
		{"SetOldSrcRadius", VoxSetOldSrcRadius},
		{"FinalSrcRadius", VoxFinalSrcRadius},
		{"Volume", VoxVolume},
		{"VolumeTarget", VoxVolumeTarget},
		{"SetVolumeTarget", VoxSetVolumeTarget},
		{"VolumeTime", VoxVolumeTime},
		{"FadeVolume", VoxFadeVolume},
		{"LerpPos", VoxLerpPos},
		{"SetLerpPos", VoxSetLerpPos},
		{"LerpRadius", VoxLerpRadius},
		{"SetLerpRadius", VoxSetLerpRadius},
		{"LerpSrcRadius", VoxLerpSrcRadius},
		{"SetLerpSrcRadius", VoxSetLerpSrcRadius},
		{"Loop", VoxLoop},
		{"SetLoop", VoxSetLoop},
		{"Background", VoxBackground},
		{"SetBackground", VoxSetBackground},
		{"Logarithmic", VoxLogarithmic},
		{"SetLogarithmic", VoxSetLogarithmic},
		{"Play", VoxPlay},
		{"Loudness", VoxLoudness},
		{"Playing", VoxPlaying},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		sndRegs,
		voxRegs,
		0
	};

	const char* prefixes[] = {
		"Snd",
		"Vox",
		0
	};

	scr::RegisterLibrary(scr::state, "gaud", regs, 0, 0, metas, prefixes);
	Sound::RegisterMetatable(sndRegs, 0);
	Voice::RegisterMetatable(voxRegs, 0);

	// Console commands
	lua_pushcfunction(scr::state, RefreshDevice); con::CreateCommand("refresh_audio_device");

	return good;
}

/*--------------------------------------
	aud::CleanUp
--------------------------------------*/
void aud::CleanUp()
{
	LibCleanUp();
	free(mix);
	mix = mixWrite = mixRead = mixEnd = 0;
}

/*--------------------------------------
	aud::MixBufRange
--------------------------------------*/
void aud::MixBufRange(float* cur, unsigned num, unsigned& start, unsigned& end)
{
	start = cur - mix;
	end = start + num * AUD_NUM_MIX_CHANNELS;

	if(end >= numMixSamples)
		end -= numMixSamples;
}

/*--------------------------------------
LUA	aud::RefreshDevice
--------------------------------------*/
int aud::RefreshDevice(lua_State* l)
{
	RefreshDevice();
	return 0;
}