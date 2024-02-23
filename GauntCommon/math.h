// math.h
// Martynas Ceicys

#ifndef COM_MATH_H
#define COM_MATH_H

#include <stdint.h>

#include "fp.h"

namespace com
{

#define COM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define COM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define COM_LERP(x0, x1, f) (((x1) - (x0)) * (f) + (x0))
#define COM_PI (com::fp)3.1415926535897932384626433832795
#define COM_PI_2 (com::fp)6.283185307179586476925286766559
#define COM_RAD_TO_DEG (com::fp)57.295779513082320876798154814105

uintmax_t	PowU(uintmax_t x, uintmax_t y);
size_t		Quadrant(fp x, fp y);
size_t		Octant(fp x, fp y, fp z);
fp			NextCycleValue(fp start, fp end, fp cur, fp advance, bool loop = false,
			bool* wrapOut = 0, size_t* numSkippedOut = 0);
fp			WrappedAngle(fp angle);
fp			LerpedAngle(fp a, fp b, fp f);
fp			Epsilon(fp f);
fp			VerticalFOV(fp hFOV, fp aspect);
fp			HorizontalFOV(fp vFOV, fp aspect);

/*--------------------------------------
	com::Min
--------------------------------------*/
template <typename t> t Min(t a, t b) {return a <= b ? a : b;}

/*--------------------------------------
	com::Max
--------------------------------------*/
template <typename t> t Max(t a, t b) {return a >= b ? a : b;}

/*--------------------------------------
	com::Lerp
--------------------------------------*/
template <typename t> t Lerp(t x0, t x1, fp f) {return COM_LERP(x0, x1, f);}

/*--------------------------------------
	com::Clamp
--------------------------------------*/
template <typename t> t Clamp(t v, t l, t h) {return Min(Max(v, l), h);}

/*--------------------------------------
	com::Multiply4x4

Does column-dot-row multiplication, a * b.
--------------------------------------*/
template <typename t> void Multiply4x4(const t* a, const t* b, t* cOut)
{
	cOut[0] = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
	cOut[1] = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
	cOut[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	cOut[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

	cOut[4] = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
	cOut[5] = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
	cOut[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
	cOut[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

	cOut[8] = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
	cOut[9] = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
	cOut[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
	cOut[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

	cOut[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
	cOut[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
	cOut[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
	cOut[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

template <typename t> void Multiply4x4Vec4(const t* v, const t* m, t* cOut)
{
	cOut[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2] + v[3] * m[3];
	cOut[1] = v[0] * m[4] + v[1] * m[5] + v[2] * m[6] + v[3] * m[7];
	cOut[2] = v[0] * m[8] + v[1] * m[9] + v[2] * m[10] + v[3] * m[11];
	cOut[3] = v[0] * m[12] + v[1] * m[13] + v[2] * m[14] + v[3] * m[15];
}

/*--------------------------------------
	com::Identity4x4
--------------------------------------*/
template <typename t> void Identity4x4(t* mOut)
{
	mOut[0] = mOut[5] = mOut[10] = mOut[15] = (t)1.0;
	mOut[1] = mOut[2] = mOut[3] = mOut[4] = mOut[6] = mOut[7] = mOut[8] = mOut[9] = mOut[11] =
		mOut[12] = mOut[13] = mOut[14] = (t)0.0;
}

/* FIXME: Consider flipping secondary and tertiary if axis is 1 to make the orientations of the
other axes relative to the given axis consistent with the order. */

/*--------------------------------------
	com::SecAxis
--------------------------------------*/
template <typename t> t SecAxis(t axis)
{
	return !axis;
}

/*--------------------------------------
	com::TerAxis
--------------------------------------*/
template <typename t> t TerAxis(t axis)
{
	return axis == 2 ? 1 : 2;
}

/*--------------------------------------
	com::ShortestDiff
--------------------------------------*/
template <typename t> t ShortestDiff(t a, t b, t min, t max)
{
	t diff = a - b;
	t range = max - min;
	t half = range / (t)2;

	if(diff > half)
		return diff - range;
	else if(diff < -half)
		return diff + range;
	else
		return diff;
}

/*--------------------------------------
	com::IsPow2
--------------------------------------*/
template <typename t> bool IsPow2(t i)
{
	return !(i & (i - 1)) && i;
}

/*--------------------------------------
	com::HighestSetBit

FIXME: why is this not like NextPow2 without a shift at the end?
--------------------------------------*/
template <typename t> t HighestSetBit(t i)
{
	i |= i >> 1;
	i |= i >> 2;
	i |= i >> 4;
	if(sizeof(t) > 1)
		i |= i >> 8;
	if(sizeof(t) > 2)
		i |= i >> 16;
	if(sizeof(t) > 4)
		i |= i >> 32;
	if(sizeof(t) > 8)
		i |= i >> 64;
	if(sizeof(t) > 16)
		i |= i >> 128;

	if(sizeof(t) > 32)
	{
		for(size_t j = 256; j < sizeof(t) * 8; j <<= 1)
			i |= i >> j;
	}

	i &= ~(i >> 1);
	return i;
}

/*--------------------------------------
	NextPow2
--------------------------------------*/
template <typename t> t NextPow2(t n)
{
	if(!(n & (n - 1)))
		return n;

	while(n & (n - 1))
		n &= n - 1;

	n <<= 1;
	return n;
}

/*--------------------------------------
	com::FixedBits
--------------------------------------*/
template <typename t> t FixedBits(t lhs, unsigned rhs, bool on)
{
	return on ? lhs | rhs : lhs & ~rhs;
}

}

#endif