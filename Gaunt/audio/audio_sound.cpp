// audio_sound.cpp
// Martynas Ceicys

#include "audio.h"
#include "audio_lua.h"
#include "audio_private.h"
#include "../../GauntCommon/io.h"
#include "../mod/mod.h"

namespace aud
{
	Sound*		CreateSound(const char* fileName);
	const char*	LoadWAV(const char* filePath, float*& samplesOut, size_t& numFramesOut,
				size_t& numChannelsOut, unsigned& frameRateOut);
	const char*	LoadWAVFail(FILE* file, float* samples, const char* err);
	void		FreeSoundSamples(float* samples);
}

/*
################################################################################################
	
	
	SOUND


################################################################################################
*/

const char* const res::Resource<aud::Sound>::METATABLE = "metaSound";

/*--------------------------------------
	aud::Sound::Sound
--------------------------------------*/
aud::Sound::Sound(const char* fileName, float* samples, size_t numFrames, size_t numChannels,
	unsigned frameRate) : fileName(com::NewStringCopy(fileName)), samples(samples),
	numFrames(numFrames), numChannels(numChannels), numSamples(numFrames * numChannels),
	frameRate(frameRate)
{
	EnsureLink();
}

/*--------------------------------------
	aud::Sound::~Sound
--------------------------------------*/
aud::Sound::~Sound()
{
	if(fileName)
		delete[] fileName;

	FreeSoundSamples(samples);
}

/*--------------------------------------
	aud::FindSound
--------------------------------------*/
aud::Sound* aud::FindSound(const char* fileName)
{
	for(com::linker<Sound>* it = Sound::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->FileName(), fileName))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	aud::EnsureSound
--------------------------------------*/
aud::Sound* aud::EnsureSound(const char* fileName)
{
	Sound* snd;
	return (snd = FindSound(fileName)) ? snd : CreateSound(fileName);
}

/*--------------------------------------
	aud::CreateSound
--------------------------------------*/
aud::Sound* aud::CreateSound(const char* fileName)
{
	const char *err = 0, *path = mod::Path("sounds/", fileName, err);
	float* samples;
	size_t numFrames, numChannels;
	unsigned frameRate;

	if(err || (err = LoadWAV(path, samples, numFrames, numChannels, frameRate)))
	{
		con::LogF("Failed to load WAV sound '%s' (%s)", fileName, err);
		return 0;
	}

	Sound* snd = new Sound(fileName, samples, numFrames, numChannels, frameRate);
	return snd;
}

/*--------------------------------------
	aud::LoadWAV

Loads WAV file at filePath. If successful, samples and subsequent arguments are modified, and 0
is returned. Otherwise, an error string is returned.

Only supports basic WAVE chunk with uncompressed PCM.

FIXME: Read but ignore unsupported chunks
FIXME: Support fmtFormat 3, IEEE float
--------------------------------------*/
#define LOAD_WAV_FAIL(err) return LoadWAVFail(file, samples, err);

const char* aud::LoadWAV(const char* filePath, float*& samplesOut, size_t& numFramesOut,
	size_t& numChannelsOut, unsigned& frameRateOut)
{
	float* samples = 0;
	FILE* file = fopen(filePath, "rb");

	if(!file)
		LOAD_WAV_FAIL("Could not open file");

	const size_t HEADER_SIZE = 44;
	unsigned char header[HEADER_SIZE];
	uint32_t riffTag;
	// Ignored 32 bits
	uint32_t waveTag;
	uint32_t fmtTag;
	uint32_t fmtSize;
	uint16_t fmtFormat;
	uint16_t fmtNumChannels;
	uint32_t fmtSampleRate;
	// Ignored 48 bits
	uint16_t fmtBPS;
	uint32_t dataTag;
	uint32_t dataSize;

	if(fread(header, sizeof(unsigned char), HEADER_SIZE, file) != HEADER_SIZE)
		LOAD_WAV_FAIL("Could not read header");

	com::MergeBE(header, riffTag);
	if(riffTag != 0x52494646)
		LOAD_WAV_FAIL("Bad 'RIFF' tag");

	// Ignoring 4 bytes: riffSize

	com::MergeBE(header + 8, waveTag);
	if(waveTag != 0x57415645)
		LOAD_WAV_FAIL("Bad 'WAVE' tag");

	com::MergeBE(header + 12, fmtTag);
	if(fmtTag != 0x666d7420)
		LOAD_WAV_FAIL("Bad 'fmt ' tag");

	com::MergeLE(header + 16, fmtSize);
	if(fmtSize != 16)
		LOAD_WAV_FAIL("fmtSize is not 16");

	com::MergeLE(header + 20, fmtFormat);
	if(fmtFormat != 1)
		LOAD_WAV_FAIL("fmtFormat is not 1 (PCM)");

	com::MergeLE(header + 22, fmtNumChannels);
	if(!fmtNumChannels)
		LOAD_WAV_FAIL("fmtNumChannels is 0");

	com::MergeLE(header + 24, fmtSampleRate);
	if(!fmtSampleRate)
		LOAD_WAV_FAIL("fmtSampleRate is 0");

	// Ignoring 6 bytes: fmtByteRate and fmtBlockAlign

	com::MergeLE(header + 34, fmtBPS);
	if(fmtBPS != 8 && fmtBPS != 16 && fmtBPS != 24 && fmtBPS != 32)
		LOAD_WAV_FAIL("Unsupported fmtBPS; must be 8, 16, 24, or 32");

	com::MergeBE(header + 36, dataTag);
	if(dataTag != 0x64617461)
		LOAD_WAV_FAIL("Bad 'data' tag");

	com::MergeLE(header + 40, dataSize);
	if(!dataSize)
		LOAD_WAV_FAIL("dataSize is 0");

	uint16_t bytesPerSample = fmtBPS / 8;

	if(dataSize % bytesPerSample)
		LOAD_WAV_FAIL("dataSize not multiple of bytesPerSample");

	size_t numSamples = dataSize / bytesPerSample;
	samples = new float[numSamples];
	size_t curSample = 0;
	const size_t BUF_SIZE = 1024;
	unsigned char buf[BUF_SIZE];
	const size_t readLim = BUF_SIZE - BUF_SIZE % bytesPerSample;
	size_t toRead = dataSize;

	// FIXME: template function for different BPS
	do
	{
		size_t numRead = toRead <= readLim ? toRead : readLim;

		if(fread(buf, sizeof(unsigned char), numRead, file) != numRead)
			LOAD_WAV_FAIL("Could not read samples");

		for(size_t i = 0; i < numRead; i += bytesPerSample, curSample++)
		{
			switch(fmtBPS)
			{
			case 8: // FIXME: test
				samples[curSample] = buf[i] * 0.0078125 - 1.0;
				break;
			case 16:
			{
				uint16_t s;
				com::MergeLE(buf + i, s);
				samples[curSample] = uint16_t(s + 32768) * 0.000030517578125 - 1.0;
				break;
			}
			case 24: // FIXME: test
			{
				uint32_t s;
				com::MergeLE<3>(buf + i, s);
				samples[curSample] = (s + 8388608) % 16777216 * 0.00000011920928955078125 - 1.0;
				break;
			}
			case 32: // FIXME: test
			{
				uint32_t s;
				com::MergeLE(buf + i, s);
				samples[curSample] = uint32_t(s + 2147483648) * 0.0000000004656612873077392578125 - 1.0;
				break;
			}
			}
		}

		toRead -= numRead;
	} while(toRead);

	fclose(file);
	samplesOut = samples;
	numFramesOut = numSamples / fmtNumChannels;
	numChannelsOut = fmtNumChannels;
	frameRateOut = fmtSampleRate;
	return 0;
}

/*--------------------------------------
	aud::LoadWAVFail
--------------------------------------*/
const char* aud::LoadWAVFail(FILE* file, float* samples, const char* err)
{
	if(file)
		fclose(file);

	if(samples)
		delete[] samples;

	file = 0;
	samples = 0;
	return err;
}

/*--------------------------------------
	aud::FreeSoundSamples
--------------------------------------*/
void aud::FreeSoundSamples(float* samples)
{
	delete[] samples;
}

/*--------------------------------------
	aud::SaveWAV

For debugging.
--------------------------------------*/
const char* aud::SaveWAV(const char* filePath, const float* samples, size_t numSamples,
	size_t numChannels, unsigned frameRate, bool i16)
{
	FILE* file = fopen(filePath, "wb");

	if(!file)
		return "Could not open file";

	const size_t HEADER_SIZE = 44;
	unsigned char header[HEADER_SIZE];
	uint32_t bytesPerSample = i16 ? sizeof(int16_t) : sizeof(float);
	uint32_t riffTag = 0x52494646;
	uint32_t dataSize = numSamples * bytesPerSample;
	uint32_t riffSize = 36 + dataSize;
	uint32_t waveTag = 0x57415645;
	uint32_t fmtTag = 0x666d7420;
	uint32_t fmtSize = 16;
	uint16_t fmtFormat = i16 ? 1 : 3;
	uint16_t fmtNumChannels = numChannels;
	uint32_t fmtSampleRate = frameRate;
	uint32_t fmtByteRate = frameRate * numChannels * bytesPerSample;
	uint16_t fmtByteAlign = numChannels * bytesPerSample;
	uint16_t fmtBPS = bytesPerSample * 8;
	uint32_t dataTag = 0x64617461;

	com::BreakBE(riffTag, header);
	com::BreakLE(riffSize, header + 4);
	com::BreakBE(waveTag, header + 8);
	com::BreakBE(fmtTag, header + 12);
	com::BreakLE(fmtSize, header + 16);
	com::BreakLE(fmtFormat, header + 20);
	com::BreakLE(fmtNumChannels, header + 22);
	com::BreakLE(fmtSampleRate, header + 24);
	com::BreakLE(fmtByteRate, header + 28);
	com::BreakLE(fmtByteAlign, header + 32);
	com::BreakLE(fmtBPS, header + 34);
	com::BreakBE(dataTag, header + 36);
	com::BreakLE(dataSize, header + 40);
	
	if(fwrite(header, sizeof(unsigned char), HEADER_SIZE, file) != HEADER_SIZE)
	{
		fclose(file);
		return "Could not write header";
	}

	if(i16)
	{
		const size_t NUM_BUF_SAMPLES = 256;
		int16_t buf[NUM_BUF_SAMPLES];

		for(size_t i = 0; i < numSamples; i += NUM_BUF_SAMPLES)
		{
			size_t write = numSamples - i < NUM_BUF_SAMPLES ? numSamples - i : NUM_BUF_SAMPLES;
		
			for(size_t j = 0; j < write; j++)
			{
				//float s = samples[i + j];
				float s = samples[(i + j) - ((i + j) % 4)]; // FIXME TEMP

				if(s >= 1.0f) // Clip and prevent 1.0 overflowing to negative
					buf[j] = 32767;
				else if(s <= -1.0f)
					buf[j] = -32768;
				else
					buf[j] = s * 32768;
			}

			if(fwrite(buf, sizeof(int16_t), write, file) != write)
			{
				fclose(file);
				return "Could not write samples";
			}
		}
	}
	else
	{
		if(fwrite(samples, sizeof(float), numSamples, file) != numSamples)
		{
			fclose(file);
			return "Could not write samples;";
		}
	}

	fclose(file);
	return 0;
}

/*
################################################################################################
	
	
	SOUND LUA


################################################################################################
*/

/*--------------------------------------
LUA	aud::FindSound

IN	sFileName
OUT	sndS
--------------------------------------*/
int aud::FindSound(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	if(Sound* s = FindSound(fileName))
	{
		s->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	aud::EnsureSound

IN	sFileName
OUT	snd
--------------------------------------*/
int aud::EnsureSound(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	Sound* snd = EnsureSound(fileName);

	if(!snd)
		luaL_error(l, "sound load failure");

	snd->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	aud::SndFileName (FileName)

IN	sndS
OUT	sFileName
--------------------------------------*/
int aud::SndFileName(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushstring(l, s->FileName());
	return 1;
}

/*--------------------------------------
LUA	aud::SndNumFrames (NumFrames)

IN	sndS
OUT	iNumFrames
--------------------------------------*/
int aud::SndNumFrames(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushinteger(l, s->NumFrames());
	return 1;
}

/*--------------------------------------
LUA	aud::SndNumChannels (NumChannels)

IN	sndS
OUT	iNumChannels
--------------------------------------*/
int aud::SndNumChannels(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushinteger(l, s->NumChannels());
	return 1;
}

/*--------------------------------------
LUA	aud::SndNumSamples (NumSamples)

IN	sndS
OUT	iNumSamples
--------------------------------------*/
int aud::SndNumSamples(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushinteger(l, s->NumSamples());
	return 1;
}

/*--------------------------------------
LUA	aud::SndFrameRate (FrameRate)

IN	sndS
OUT	iFrameRate
--------------------------------------*/
int aud::SndFrameRate(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushinteger(l, s->FrameRate());
	return 1;
}

/*--------------------------------------
LUA	aud::SndSampleRate (SampleRate)

IN	sndS
OUT	iSampleRate
--------------------------------------*/
int aud::SndSampleRate(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushinteger(l, s->SampleRate());
	return 1;
}

/*--------------------------------------
LUA	aud::SndSeconds (Seconds)

IN	sndS
OUT	nSeconds
--------------------------------------*/
int aud::SndSeconds(lua_State* l)
{
	Sound* s = Sound::CheckLuaTo(1);
	lua_pushnumber(l, s->Seconds());
	return 1;
}