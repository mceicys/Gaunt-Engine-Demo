// fp.h
// Martynas Ceicys

#ifndef COM_FP_H
#define COM_FP_H

#include <errno.h>
#include <float.h>
#include <stdlib.h>

#include "io.h"
#include "type.h"

namespace com
{

#ifdef COM_USE_DOUBLE
	typedef double fp;
	#define COM_FP_MIN DBL_MIN
	#define COM_FP_MAX DBL_MAX
	#define COM_FP_EPSILON DBL_EPSILON
	#define COM_FP_PRNT_LEN COM_DBL_PRNT_LEN
	#define COM_FP_PRNT COM_DBL_PRNT
	#define COM_STR_TO_FP(str, endPtr) strtod(str, endPtr)
#else
	typedef float fp;
	#define COM_FP_MIN FLT_MIN
	#define COM_FP_MAX FLT_MAX
	#define COM_FP_EPSILON FLT_EPSILON
	#define COM_FP_PRNT_LEN COM_FLT_PRNT_LEN
	#define COM_FP_PRNT COM_FLT_PRNT
	#define COM_STR_TO_FP(str, endPtr) StrToF(str, endPtr) // FIXME: make this a StrToFP in io.h, fp.h shouldn't include io.h
#endif

/*--------------------------------------
	com::FixFP

FIXME: float type causes warning on debug build, C4756: overflow in constant arithmetic
--------------------------------------*/
template <typename t> t FixFP(t f)
{
	if(COM_EQUAL_TYPES(t, float))
	{
		if(f > FLT_MAX)
			return FLT_MAX;
		else if(f < -FLT_MAX)
			return -FLT_MAX;
		else if(!(f >= -FLT_MAX))
			return 0.0f;
	}
	else if(COM_EQUAL_TYPES(t, double))
	{
		if(f > DBL_MAX)
			return (t)DBL_MAX;
		else if(f < -DBL_MAX)
			return (t)-DBL_MAX;
		else if(!(f >= -DBL_MAX))
			return 0.0f;
	}

	return f;
}

/*--------------------------------------
	com::Sign
--------------------------------------*/
template <typename t> int Sign(t f)
{
	if(f == (t)0.0)
		return 0;
	else if(f > (t)0.0)
		return 1;
	else
		return -1;
}

}

#endif