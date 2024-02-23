// audio_private.h
// Martynas Ceicys

#ifndef AUDIO_PRIVATE_H
#define AUDIO_PRIVATE_H

#include <stdint.h>

#define AUD_NUM_MIX_CHANNELS 2

namespace aud
{

extern float *mix, *mixRead;
extern unsigned numMixFrames, numMixSamples, mixFrameRate;
__declspec(align(8)) extern volatile uint32_t numReadyFrames;

// audio.cpp
void	MixBufRange(float* cur, unsigned num, unsigned& startOut, unsigned& endOut);
int		RefreshDevice(lua_State* l);

// Library-dependent implementation
void	LibUpdate();
bool	LibInit();
void	LibCleanUp();
void	RefreshDevice();

// audio_sound.cpp
const char* SaveWAV(const char* filePath, const float* samples, size_t numSamples,
	size_t numChannels, unsigned frameRate, bool i16 = false);

}

#endif