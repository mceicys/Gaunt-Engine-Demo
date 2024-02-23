// math.cpp
// Martynas Ceicys

#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "math.h"

/*--------------------------------------
	com::PowU
--------------------------------------*/
uintmax_t com::PowU(uintmax_t x, uintmax_t y)
{
	uintmax_t z = 1;

	for(uintmax_t i = 0; i < y; i++)
		z *= x;

	return z;
}

/*--------------------------------------
	com::Quadrant

Returns value where least significant bit is like x's sign bit and second bit is like y's sign
bit. 0.0 and -0.0 are considered positive.

 y+

1|0
--- x+
3|2
--------------------------------------*/
size_t com::Quadrant(fp x, fp y)
{
	if(x >= (fp)0.0)
		return y >= (fp)0.0 ? 0 : 2;
	else
		return y >= (fp)0.0 ? 1 : 3;
}

/*--------------------------------------
	com::Octant

Like com::Quadrant except the third bit also represents z sign.

      |     /
      z    y

   1------0
  /|     /|
 / |    / |
3------2  |  x--
|  |   |  |
|  5---|--4
| /    | /
|/     |/
7------6
--------------------------------------*/
size_t com::Octant(fp x, fp y, fp z)
{
	if(x >= (fp)0.0)
	{
		if(y >= (fp)0.0)
			return z >= (fp)0.0 ? 0 : 4;
		else
			return z >= (fp)0.0 ? 2 : 6;
	}
	else
	{
		if(y >= (fp)0.0)
			return z >= (fp)0.0 ? 1 : 5;
		else
			return z >= (fp)0.0 ? 3 : 7;
	}
}

/*--------------------------------------
	com::NextCycleValue
--------------------------------------*/
com::fp com::NextCycleValue(fp start, fp end, fp cur, fp advance, bool loop, bool* wrapOut,
	size_t* numSkippedOut)
{
	bool forward = advance >= 0.0f;

	if(wrapOut)
		*wrapOut = false;

	if(numSkippedOut)
		*numSkippedOut = 0;

	cur += advance;
	bool endCycle;
	if(forward)
		endCycle = cur >= end;
	else
		endCycle = cur <= start;

	if(endCycle)
	{
		if(loop)
		{
			if(wrapOut)
				*wrapOut = true;

			if(fp range = end - start)
			{
				fp over = cur - (forward ? end : start);
				fp offset = fmod(over, range);
				cur = (forward ? start : end) + offset;

				if(numSkippedOut)
					*numSkippedOut = (size_t)fabs(over / range);
			}
			else
				cur = forward ? start : end;
		}
		else
			cur = forward ? end : start;
	}

	return cur;
}

/*--------------------------------------
	com::WrappedAngle

Returns angle wrapped to [-pi, pi].
--------------------------------------*/
com::fp com::WrappedAngle(fp angle)
{
	// return fmod(angle, COM_PI) - (int(angle / COM_PI) % 2) * COM_PI;
	
	if(angle > COM_PI)
	{
		fp wrap = fmod(angle, COM_PI_2);
		
		if(wrap > COM_PI)
			return wrap - COM_PI_2;
		else
			return wrap;
	}	
	else if(angle < -COM_PI)
	{
		fp wrap = fmod(angle, COM_PI_2);
		
		if(wrap < -COM_PI)
			return wrap + COM_PI_2;
		else
			return wrap;
	}	
	else
		return angle;
}

/*--------------------------------------
	com::LerpedAngle
--------------------------------------*/
com::fp com::LerpedAngle(fp a, fp b, fp f)
{
	return WrappedAngle(a + COM_LERP((fp)0.0, WrappedAngle(b - a), f));
}

/*--------------------------------------
	com::Epsilon
--------------------------------------*/
com::fp com::Epsilon(fp f)
{
	return fabs(f) * COM_FP_EPSILON;
}

/*--------------------------------------
	com::VerticalFOV

aspect is width / height.
--------------------------------------*/
com::fp com::VerticalFOV(fp hFOV, fp aspect)
{
	return (fp)2.0 * atan(tan(hFOV * (fp)0.5) / aspect);
}

/*--------------------------------------
	com::HorizontalFOV
--------------------------------------*/
com::fp com::HorizontalFOV(fp vFOV, fp aspect)
{
	return (fp)2.0 * atan(tan(vFOV * (fp)0.5) * aspect);
}