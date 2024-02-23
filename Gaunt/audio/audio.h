// audio.h
// Martynas Ceicys

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#include "../console/option.h"
#include "../../GauntCommon/vec.h"
#include "../resource/resource.h"

namespace aud
{

/*
################################################################################################
	SOUND
################################################################################################
*/

/*======================================
	aud::Sound
======================================*/
class Sound : public res::Resource<Sound>
{
public:
					// Managed sound constructor; don't call outside of audio subsystem
					Sound(const char* fileName, float* samples, size_t numFrames,
					size_t numChannels, unsigned frameRate);
					~Sound();

	const char*		FileName() const {return fileName;}
	const float*	Samples() const {return samples;}
	size_t			NumFrames() const {return numFrames;}
	size_t			NumChannels() const {return numChannels;}
	size_t			NumSamples() const {return numSamples;}
	unsigned		FrameRate() const {return frameRate;}
	unsigned		SampleRate() const {return frameRate * numChannels;}
	float			Seconds() const {return numFrames / (float)frameRate;}

private:
	const char*		fileName;
	float*			samples;
	size_t			numFrames, numChannels, numSamples;
	unsigned		frameRate;
};

Sound*	FindSound(const char* fileName);
Sound*	EnsureSound(const char* fileName);

/*
################################################################################################
	VOICE
################################################################################################
*/

/*======================================
	aud::Voice

FIXME: Pool
FIXME: Flag to pause playback and volume change when simulation is paused
======================================*/
class Voice : public res::Resource<Voice>
{
public:
	enum
	{
		LERP_POS = 1 << 0,
		LERP_RADIUS = 1 << 1,
		LERP_SRC_RADIUS = 1 << 2,
		LOOP = 1 << 3,
		BACKGROUND = 1 << 4, // FIXME: replace w/ ATTENUATION, PANNING, ABSOLUTE_PANNING
		LOGARITHMIC = 1 << 5
	};

	com::Vec3				pos, oldPos;
	float					radius, oldRadius;
	float					srcRadius, oldSrcRadius; // FIXME: remove if this turns out to be useless
	float					volumeTarget;
	uint16_t				flags;

							Voice(const com::Vec3& pos, float radius, float srcRadius,
							float volume);
	com::Vec3				FinalPos() const;
	float					FinalRadius() const;
	float					FinalSrcRadius() const;
	float					Volume(float time) const;
	float					VolumeTime() const {return volumeTime;}
	void					FadeVolume(float target, float time);
	void					Play(const Sound* snd, float time = 0.0f);
	float					Loudness(float ahead) const;
	bool					Playing() const {return snd.Value() != 0;}
	void					SaveOld();

private:
	float					volume, volumeTime;
	res::Ptr<const Sound>	snd;
	size_t					curSample;

	bool					Advance(size_t numSamples);
	friend void				Update();
};

/*
################################################################################################
	GENERAL
################################################################################################
*/

extern con::Option masterVolume;

void Update();
void SaveOld();
void StopVoices();
void DeleteUnused();
bool Init();
void CleanUp();

}

#endif