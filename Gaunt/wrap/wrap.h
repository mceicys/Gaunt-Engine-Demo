// wrap.h -- OS abstraction
// Martynas Ceicys

#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdarg.h>

#include "../console/option.h"
#include "../../GauntCommon/io.h"

#define WRP_FATALF(format, ...) wrp::FatalF("%s: " format, COM_FUNC_NAME, __VA_ARGS__)
#define WRP_FATAL(str) wrp::FatalF("%s: " str, COM_FUNC_NAME)

namespace wrp
{

/*
################################################################################################
	GENERAL
################################################################################################
*/

extern con::Option showFPS;

void				Quit();
void				FatalF(const char* format, ...);
unsigned long long	Time();
unsigned long long	SystemClock();
const char*			RestrictedPath(const char* path);

/*
################################################################################################
	VIDEO
################################################################################################
*/

bool			SetVideo(unsigned width, unsigned height, bool fullScreen, bool vSync);
unsigned int	VideoWidth();
unsigned int	VideoHeight();

size_t			CreateResolutionArray();
size_t			ResolutionArraySize();
bool			CopyResolutionArray(const size_t size, unsigned int* resArrayOut);

/*
################################################################################################
	LOOP
################################################################################################
*/

extern con::Option
	tickMS,
	tickRate,
	minFrameMS,
	maxFrameRate,
	updateMS,
	updateFactor,
	baseTimeStep,
	origTimeStep,
	timeStep,
	timeStepFactor,
	varTickRate,
	maxTickMS;

// FIXME: make most these options
bool				ForceTimeStep(float f);
void				ResetTimeStep();
float				Fraction();
unsigned long long	NumFrames();
unsigned long long	NumTicks();
unsigned long long	SimTime();
float				SimTimeFraction();
bool				ForceFrame();
bool				ForceFrameStart();
void				ForceFrameEnd();

}

#endif