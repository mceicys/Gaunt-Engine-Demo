// spline.h
// Martynas Ceicys

#ifndef COM_SPLINE_H
#define COM_SPLINE_H

#include "array.h"
#include "math.h"
#include "vec.h"

namespace com
{

/*
################################################################################################
	CATMULL-ROM SPLINE
################################################################################################
*/

/*--------------------------------------
	com::CatmullRom

en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline
denkovacs.com/2016/02/catmull-rom-spline-derivatives/

FIXME: Test velOut
FIXME: Get second derivative
--------------------------------------*/
template <typename pt>
pt CatmullRom(fp t, const pt& p0, fp t0, const pt& p1, fp t1, const pt& p2, fp t2, const pt& p3,
	fp t3, pt* velOut = 0)
{
	fp f12 = (t - t1) / (t2 - t1);
	fp f02 = (t - t0) / (t2 - t0);
	fp f13 = (t - t1) / (t3 - t1);
	pt a1 = COM_LERP(p0, p1, (t - t0) / (t1 - t0));
	pt a2 = COM_LERP(p1, p2, f12);
	pt a3 = COM_LERP(p2, p3, (t - t2) / (t3 - t2));
	pt b1 = COM_LERP(a1, a2, f02);
	pt b2 = COM_LERP(a2, a3, f13);
	pt c = COM_LERP(b1, b2, f12);

	if(velOut)
	{
		pt a1p = (p1 - p0) / (t1 - t0);
		pt a2p = (p2 - p1) / (t2 - t1);
		pt a3p = (p3 - p2) / (t3 - t2);
		pt b1p = (a2 - a1) / (t2 - t0) + COM_LERP(a1p, a2p, f02);
		pt b2p = (a3 - a2) / (t3 - t1) + COM_LERP(a2p, a3p, f13);
		*velOut = (b2 - b1) / (t2 - t1) + COM_LERP(b1p, b2p, f12);
	}

	return c;
}

/*--------------------------------------
	com::CatmullRomTime
--------------------------------------*/
template <typename pt>
fp CatmullRomTime(const pt& a, const pt& b, fp alpha)
{
	return pow((b - a).Mag(), alpha);
}

/*
################################################################################################
	CUBIC SPLINE
################################################################################################
*/

/*======================================
	com::Spline
======================================*/
template <typename u>
class Spline
{
public:

struct spline_point
{
	u	val;
	fp	time;
};

Arr<spline_point>	pts;
size_t				numPts;
u					vi, vf; // Initial and final velocities, 0.0 means no constraint
								// FIXME: Do explicit flags instead, zero velocity is a valid constraint

/*--------------------------------------
	com::Spline::Spline
--------------------------------------*/
Spline() : numPts(0), numPolynomials(0) {}

/*--------------------------------------
	com::Spline::~Spline
--------------------------------------*/
~Spline()
{
	pts.Free();
	accs.Free();
	coefs.Free();
}

/*--------------------------------------
	com::Spline::SetTimesToChordLengths
--------------------------------------*/
void SetTimesToChordLengths()
{
	if(!numPts)
		return;

	pts[0].time = (fp)0.0;

	for(size_t i = 1; i < numPts; i++)
		pts[i].time = pts[i - 1].time + (pts[i].val - pts[i - 1].val).Mag();
}

/*--------------------------------------
	com::Spline::CalculateCoefficients
--------------------------------------*/
void CalculateCoefficients()
{
	if(numPts < 2)
		return;

	numPolynomials = numPts - 1;
	coefs.Ensure(numPolynomials * 4);
	u discard;
	bool useVI = vi != (fp)0.0, useVF = vf != (fp)0.0;
	PolynomialCoefficients(useVI, useVF, 1, (fp)0.0, (fp)0.0, (fp)0.0, discard);
}

/*--------------------------------------
	com::Spline::ClearCoefficients
--------------------------------------*/
void ClearCoefficients()
{
	numPolynomials = 0;
}

/*--------------------------------------
	com::Spline::Pos
--------------------------------------*/
spline_point Pos(size_t i, fp t) const
{
	if(i >= numPolynomials)
		return Zero();

	return PosResult(i, t);
}

/*--------------------------------------
	com::Spline::PosSearch
--------------------------------------*/
spline_point PosSearch(fp t) const
{
	size_t numActual = Min(numPolynomials, numPts ? numPts - 1 : 0);

	if(!numActual)
		return Zero();

	size_t i = 0;
	for(; i < numActual && pts[i].time <= t; i++);
	return PosResult(i, t);
}

// FIXME
spline_point Vel(size_t i, fp t) const;
spline_point VelSearch(fp t) const;
spline_point Acc(size_t i, fp t) const;
spline_point AccSearch(fp t) const;

const u*	Accs() const {return accs.o;}
const u*	Coefs() const {return coefs.o;}
size_t		NumPtsCalc() const {return numPolynomials + 1;}
size_t		NumPolynomials() const {return numPolynomials;}

private:

Arr<u>	accs; // Second derivative at each pt
Arr<u>	coefs; // Coefficients for each polynomial
size_t	numPolynomials;

/*--------------------------------------
	com::Spline::PosResult
--------------------------------------*/
spline_point PosResult(size_t i, fp t) const
{
	size_t coef = i * 4;
	fp tSq = t * t;
	fp tCb = tSq * t;

	spline_point result = {
		tCb * coefs[coef] + tSq * coefs[coef + 1] + t * coefs[coef + 2] + coefs[coef + 3],
		t
	};

	return result;
}

/*--------------------------------------
	com::Spline::Zero
--------------------------------------*/
spline_point Zero() const
{
	spline_point zero = {(fp)0.0, (fp)0.0};
	return zero;
}

/*--------------------------------------
	com::Spline::PolynomialCoefficients

FIXME: unwrap, don't do recursion

Recursively calculates coefficients of polynomials [1, numPolynomials].

en.wikiversity.org/wiki/Cubic_Spline_Interpolation
en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm

Note, i starts at 1 to match up with the notation in the first link above, equation 1 in
particular; the initial call to this function calculates the coefficients for polynomial C1
which spans from time[0] to time[1].

System of equations AM = B:
[b[0]	lambda[0]	0			0		0		0			]	[m[0]	]		[d[0]	]
[mu[1]	b[1]		lambda[1]	0		0		0			]	[m[1]	]		[d[1]	]
[0		...			...			...		0		0			]	[..		]	=	[..		]
[0		0			...			...		...		0			]	[..		]		[..		]
[0		0			0			mu[n-1]	b[n-1]	lambda[n-1]	]	[m[n-1]	]		[d[n-1] ]
[0		0			0			0		mu[n]	b[n]		]	[m[n]	]		[d[n]	]

b[0, n] = 2.0
--------------------------------------*/
void PolynomialCoefficients(bool useVI, bool useVF, size_t i, fp bPrimePrevPrev,
	fp lambdaPrevPrev, u dPrimePrevPrev, u& mPrevOut)
{
	const u START_ACCELERATION = (fp)0.0;
	const u END_ACCELERATION = (fp)0.0;

	fp hCur = pts[i].time - pts[i - 1].time; // h[i]

	// Get row i - 1 of matrix A in AM = B
	fp muPrev, lambdaPrev;

	if(i - 1)
	{
		fp hPrev = pts[i - 1].time - pts[i - 2].time; // h[i - 1]
		muPrev = hPrev / (hPrev + hCur);
		lambdaPrev = (fp)1.0 - muPrev;
	}
	else
	{
		muPrev = (fp)0.0; // Not used
		lambdaPrev = useVI ? (fp)1.0 : (fp)0.0;
	}

	// Get row i - 1 of matrix B
	u dPrev;

	if(i - 1)
	{
		// d[i] = 6 * DividedDifferences(pts[i - 1], pts[i], pts[i + 1])
		dPrev = (fp)6.0 *
			(
				(
					( pts[i].val - pts[i - 1].val ) /
					(
						( pts[i].time - pts[i - 1].time ) *
							( pts[i].time - pts[i - 2].time )
					)
				)
				-
				(
					( pts[i - 1].val - pts[i - 2].val ) /
					(
						( pts[i - 1].time - pts[i - 2].time ) *
							( pts[i].time - pts[i - 2].time )
					)
				)
			);
	}
	else
	{
		if(useVI)
		{
			// Calculate d[0] taking vi into account
			// d[0] = 6 * DividedDifferences(pts[0], pts[0], pts[1]) = 6/h[1] * (DividedDifferences(pts[0], pts[1]) - vi)
			dPrev = (fp)6.0 / hCur * ((pts[1].val - pts[0].val) / hCur - vi);
		}
		else
			dPrev = (fp)2.0 * START_ACCELERATION;
	}

	// Tridiagonal matrix algorithm starts here
	// Do part of forward sweep to get prime values in row i - 1 of A and B, kept on stack
	fp bPrimePrev;
	u dPrimePrev;

	if(i - 1)
	{
		//fp w = muPrev * (fp)0.5;
		fp w = muPrev / bPrimePrevPrev;
		bPrimePrev = (fp)2.0 - w * lambdaPrevPrev;
		dPrimePrev = dPrev - w * dPrimePrevPrev;
	}
	else
	{
		// Start of forward sweep, first row of A and B are not modified
		bPrimePrev = (fp)2.0;
		dPrimePrev = dPrev;
	}

	// Get row i of matrix M (polynomial's second derivative at end of its domain)
	u mCur;

	if(i == numPolynomials)
	{
		// We've forward swept to the last row
		if(useVF)
		{
			// Calculate d[n] taking vf into account
			// d[n] = 6 * DividedDifferences(pts[n - 1], pts[n], pts[n]) = 6/h[n] * (vf - DividedDifferences(pts[n - 1], pts[n]))
			u dCur = (fp)6.0 / hCur * (vf - (pts[numPolynomials].val - pts[numPolynomials - 1].val) / hCur);

			// Calculate dPrime[n] and bPrime[n]
			//fp muCur = (fp)1.0;
			fp w = (fp)1.0 /* muCur */ / bPrimePrev;
			fp bPrimeCur = (fp)2.0 - w * lambdaPrev;
			u dPrimeCur = dCur - w * dPrimePrev;

			// Calculate m[n]
			mCur = dPrimeCur / bPrimeCur;
		}
		else // m[n] has a constant value
			mCur = END_ACCELERATION;
	}
	else
		// Continue forward sweep and come back once we're doing back substitution
		PolynomialCoefficients(useVI, useVF, i + 1, bPrimePrev, lambdaPrev, dPrimePrev, mCur);

	/* Do part of back substitution to get row i - 1 of matrix M (polynomial's second derivative
	at start of its domain) */
	u mPrev;

	if(i - 1 || useVI)
		mPrev = (dPrimePrev - lambdaPrev * mCur) / bPrimePrev;
	else
		mPrev = START_ACCELERATION; // m[0] has a constant value

	mPrevOut = mPrev; // Output result to caller so it can continue back substitution

#if 0
	// FIXME TEMP: Check equations 5.1 and 5.2
	if(useVI && i == 1)
	{
		u left = 2.0 * mPrev + mCur;
		u right = (6.0 / (pts[1].time - pts[0].time)) * (((pts[1].val - pts[0].val) / (pts[1].time - pts[0].time)) - vi);
		u dif = left - right;
		int a = 0;
	}

	if(useVF && i == numPolynomials)
	{
		u left = mPrev + 2.0 * mCur;
		u right = (6.0 / (pts[i].time - pts[i - 1].time)) * (vf - ((pts[i].val - pts[i - 1].val) / (pts[i].time - pts[i - 1].time)));
		u dif = left - right;
		int a = 0;
	}
#endif

	/* Have second derivative at polynomial's boundaries, calculate coefficients

	i.e. simplify equation 1:

	C(x) =
		mPrev * (time[i] - t)^3 / (6 * hCur) +
		mCur * (t - time[i - 1])^3 / (6 * hCur) +
		(val[i - 1] - mPrev * hCur^2 / 6) * (time[i] - t) / hCur +
		(val[i] - mCur * hCur^2 / 6) * (t - time[i - 1]) / hCur
	*/

	size_t coef = (i - 1) * 4;
	const fp &tPrev = pts[i - 1].time, &tCur = pts[i].time;
	const u &valPrev = pts[i - 1].val, &valCur = pts[i].val;
	fp hInv = (fp)1.0 / hCur;
	fp h6Inv = (fp)1.0 / (hCur * (fp)6.0);
	fp hSqOver6 = (hCur * hCur) / (fp)6.0;
	u firstMultiplier = mPrev * h6Inv;
	u secondMultiplier = mCur * h6Inv;
	u thirdMultiplier = (valPrev - mPrev * hSqOver6) * hInv;
	u fourthMultiplier = (valCur - mCur * hSqOver6) * hInv;

	// t^3
	coefs[coef] =
		-firstMultiplier + // Contribution from first addend of C(x)
		secondMultiplier; // Second addend

	// t^2
	coefs[coef + 1] =
		(fp)3.0 * tCur * firstMultiplier + // First addend
		(fp)-3.0 * tPrev * secondMultiplier; // Second

	// t
	coefs[coef + 2] =
		(fp)-3.0 * tCur * tCur * firstMultiplier + // First addend
		(fp)3.0 * tPrev * tPrev * secondMultiplier + // Second
		-thirdMultiplier + // Third
		fourthMultiplier; // Fourth

	// constant
	coefs[coef + 3] =
		tCur * tCur * tCur * firstMultiplier + // First addend
		-tPrev * tPrev * tPrev * secondMultiplier + // Second
		tCur * thirdMultiplier + // Third
		-tPrev * fourthMultiplier; // Fourth
}
}; // com::Spline

}

#endif