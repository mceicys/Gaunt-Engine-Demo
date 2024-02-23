// render_curve.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"

namespace rnd
{
	size_t	FindCurvePoint(float x, size_t& insertOut);
	bool	GoodPointIndex(size_t i);
	float	PolynomialInterpolate(size_t i, float x);
	void	PolynomialCoefficients(size_t i, float lambdaPrev, float dPrimePrev,
			float& mNextOut);
	void	CalculateCoefficients();
}

static com::Arr<com::Vec2>	curvePoints;
static size_t				numCurvePoints; // FIXME: limit to prevent stack overflow
static size_t				numPolynomials;
static com::Arr<GLfloat>	coefficients;

// Curve texture input and output, [0, 1]
static com::Arr<GLfloat>	curveSegmentsX, // x is stored so drawing the curve is easier
							curveSegmentsY;

static bool curveUpdated = true;

/*--------------------------------------
	rnd::SetCurvePoint

x is input intensity, y is output intensity.
--------------------------------------*/
void rnd::SetCurvePoint(float x, float y)
{
	size_t insert;
	size_t i = FindCurvePoint(x, insert);

	if(i == -1)
	{
		curvePoints.Ensure(numCurvePoints + 1);
		com::ShiftCopy(curvePoints.o, insert, numCurvePoints, 1);
		curvePoints[insert] = com::Vec2(x, y);
		numCurvePoints++;
	}
	else
		curvePoints[i].y = y;

	curveUpdated = true;
}

/*--------------------------------------
	rnd::CurvePoint
--------------------------------------*/
com::Vec2 rnd::CurvePoint(size_t i)
{
	if(!GoodPointIndex(i))
		return com::Vec2(FLT_MAX);

	return curvePoints[i];
}

/*--------------------------------------
	rnd::NumCurvePoints
--------------------------------------*/
size_t rnd::NumCurvePoints()
{
	return numCurvePoints;
}

/*--------------------------------------
	rnd::FindCurvePoint
--------------------------------------*/
size_t rnd::FindCurvePoint(float x, size_t& insertOut)
{
	size_t beg = 0, end = numCurvePoints;
	size_t mid;
	insertOut = 0;

	while(beg != end)
	{
		mid = beg + (end - beg) / 2;

		if(curvePoints[mid].x == x)
			return mid;
		else if(x < curvePoints[mid].x)
			end = insertOut = mid;
		else
			beg = insertOut = mid + 1;
	}

	return -1;
}

size_t rnd::FindCurvePoint(float x)
{
	size_t insert;
	return FindCurvePoint(x, insert);
}

/*--------------------------------------
	rnd::DeleteCurvePoint
--------------------------------------*/
void rnd::DeleteCurvePoint(size_t i)
{
	if(!GoodPointIndex(i))
		return;

	com::ShiftCopy(curvePoints.o, i + 1, numCurvePoints, -1);
	numCurvePoints--;
	curveUpdated = true;
}

/*--------------------------------------
	rnd::ClearCurvePoints
--------------------------------------*/
void rnd::ClearCurvePoints()
{
	numCurvePoints = 1;
	curvePoints.Ensure(1);
	curvePoints[0] = com::Vec2(0.0f, 0.0f);
	curveUpdated = true;
}

/*--------------------------------------
	rnd::CheckCurveUpdates
--------------------------------------*/
void rnd::CheckCurveUpdates()
{
	if(curveUpdated)
		CalculateCurveTexture();
}

/*--------------------------------------
	rnd::CalculateCurveTexture
--------------------------------------*/
void rnd::CalculateCurveTexture()
{
	if(!numCurvePoints)
		SetCurvePoint(0.0f, 0.0f);

	curveUpdated = false;
	int numSegs = numCurveSegments.Integer();
	curveSegmentsX.Ensure(numSegs);
	curveSegmentsY.Ensure(numSegs);
	GLfloat intensity = CurrentMaxIntensity();
	GLfloat span = intensity / (GLfloat)(numSegs - 1);
	bool simple = !lightCurve.Bool() || numCurvePoints == 1;

	if(simple)
	{
		GLfloat a = 0.0f;

		if(lightCurve.Bool())
			a = curvePoints[0].y / intensity;

		GLfloat b = a + 1.0f / intensity;

		for(size_t seg = 0; seg < numSegs; seg++)
		{
			GLfloat x = (GLfloat)seg * span;
			GLfloat f = x - curvePoints[0].x;
			curveSegmentsX[seg] = x / intensity;
			curveSegmentsY[seg] = COM_LERP(a, b, f);
		}
	}
	else
	{
		CalculateCoefficients();
		size_t pt = 0; // Index of point after x

		for(size_t seg = 0; seg < numSegs; seg++)
		{
			GLfloat x = (GLfloat)seg * span;
			for(; pt < numCurvePoints && x > curvePoints[pt].x; pt++);
			GLfloat y;

			if(pt)
			{
				if(pt < numCurvePoints)
					y = PolynomialInterpolate(pt, x);
				else
					y = curvePoints[pt - 1].y;

#if 0
				// Lerp
				GLfloat f = (x - curvePoints[pt - 1].x) /
					(curvePoints[pt].x - curvePoints[pt - 1].x);

				y = COM_LERP(curvePoints[pt - 1].y, curvePoints[pt].y, f);
#endif
			}
			else
				y = curvePoints[pt].y;

			curveSegmentsX[seg] = x / intensity;
			curveSegmentsY[seg] = y / intensity;
		}
	}

	glActiveTexture(RND_CURVE_TEXTURE_UNIT);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_INTENSITY16, numCurveSegments.Integer(), 0, GL_LUMINANCE,
		GL_FLOAT, curveSegmentsY.o);
}

/*--------------------------------------
	rnd::DrawCurve
--------------------------------------*/
void rnd::DrawCurve(int color)
{
	float width = wrp::VideoWidth();
	float height = wrp::VideoHeight();
	float intensity = CurrentMaxIntensity();
	com::Vec2 scale(width / intensity, height / intensity);

	// Draw grid
	for(float a = 0.0f; a < intensity; a += 1.0f)
	{
		float x = a * scale.x;
		float y = a * scale.y;
		Draw2DLine(com::Vec2(x, 0.0f), com::Vec2(x, height), color);
		Draw2DLine(com::Vec2(0.0f, y), com::Vec2(width, y), color);
	}

	// Draw curve points
	const float PT_SIZE = 16.0f;

	for(size_t i = 0; i < numCurvePoints; i++)
	{
		com::Vec2 pt = com::Vec2(curvePoints[i].x, curvePoints[i].y) * scale;
		Draw2DLine(pt - com::Vec2(PT_SIZE, PT_SIZE), pt + com::Vec2(PT_SIZE, PT_SIZE), color);
		Draw2DLine(pt - com::Vec2(PT_SIZE, -PT_SIZE), pt + com::Vec2(PT_SIZE, -PT_SIZE), color);
	}

	// Draw curve segments
	int numSegs = COM_MIN(numCurveSegments.Integer(), curveSegmentsX.n);

	for(size_t seg = 1; seg < numSegs; seg++)
	{
		Draw2DLine(
			com::Vec2(curveSegmentsX[seg - 1], curveSegmentsY[seg - 1]) * intensity * scale,
			com::Vec2(curveSegmentsX[seg], curveSegmentsY[seg]) * intensity * scale,
			color
		);
	}
}

/*--------------------------------------
	rnd::GoodPointIndex
--------------------------------------*/
bool rnd::GoodPointIndex(size_t i)
{
	if(i >= numCurvePoints)
	{
		CON_ERRORF("Invalid curve point index ("COM_UMAX_PRNT") >= numCurvePoints ("
			COM_UMAX_PRNT")", (uintmax_t)i, (uintmax_t)numCurvePoints);

		return false;
	}

	return true;
}

/*--------------------------------------
	rnd::PolynomialInterpolate
--------------------------------------*/
float rnd::PolynomialInterpolate(size_t i, float x)
{
	size_t coef = (i - 1) * 4;
	float xSq = x * x;
	float xCb = xSq * x;

	return xCb * coefficients[coef] +
		xSq * coefficients[coef + 1] +
		x * coefficients[coef + 2] +
		coefficients[coef + 3];
}

/*--------------------------------------
	rnd::PolynomialCoefficients

Recursively calculates coefficients of polynomials [i, numPolynomials] and outputs polynomial
i's second derivative at the start of its domain. Polynomial indices start from 1.

en.wikiversity.org/wiki/Cubic_Spline_Interpolation
en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm

FIXME FIXME: use com::Spline class, the w calculation here is incorrect
--------------------------------------*/
void rnd::PolynomialCoefficients(size_t i, float lambdaPrev, float dPrimePrev, float& mStartOut)
{
	const float START_BOUNDARY = 0.0f;
	const float END_BOUNDARY = 0.0f;

	// Get row i - 1 of matrix A in AM = B
	const com::Vec2* pts = curvePoints.o;
	float hCur = pts[i].x - pts[i - 1].x; // h(i)
	float mu, lambda;

	if(i - 1)
	{
		float hPrev = pts[i - 1].x - pts[i - 2].x; // h(i - 1)
		mu = hPrev / (hPrev + hCur);
		lambda = 1.0f - mu;
	}
	else
	{
		mu = 0.0f; // Not used
		lambda = 0.0f;
	}

	// Get row i - 1 of matrix B
	float d;

	if(i - 1)
	{
		d = 6.0f *
			(
				(
					( pts[i].y - pts[i - 1].y ) /
						( ( pts[i].x - pts[i - 1].x ) * ( pts[i].x - pts[i - 2].x ) )
				)
				-
				(
					( pts[i - 1].y - pts[i - 2].y ) /
						( ( pts[i - 1].x - pts[i - 2].x ) * ( pts[i].x - pts[i - 2].x ) )
				)
			);
	}
	else
		d = 2.0f * START_BOUNDARY;

	// Do part of forward sweep to get prime values in row i - 1 of A and B, kept on stack
	float bPrime;
	float dPrime;

	if(i - 1)
	{
		float w = mu * 0.5f;
		bPrime = 2.0f - w * lambdaPrev;
		dPrime = d - w * dPrimePrev;
	}
	else
	{
		// Start of forward sweep, first row of A is not modified
		bPrime = 2.0f;
		dPrime = d;
	}

	// Get row i of matrix M (polynomial's second derivative at end of its domain)
	float mEnd;

	if(i == numPolynomials)
	{
		// We've forward swept to the last row where M has a constant value
		mEnd = END_BOUNDARY; // Boundary condition
	}
	else
		// Continue forward sweep and come back once we're doing back substitution
		PolynomialCoefficients(i + 1, lambda, dPrime, mEnd);

	/* Do part of back substitution to get row i - 1 of matrix M (polynomial's second derivative
	at start of its domain) */
	float mStart;

	if(i - 1)
		mStart = (dPrime - lambda * mEnd) / bPrime;
	else
		mStart = START_BOUNDARY; // Boundary condition

	mStartOut = mStart; // Output result to caller so it can continue back substitution

	/* Have second derivative at polynomial's boundaries, calculate coefficients

	i.e. simplify this:

	C(x) =
		mStart * (pts[i].x - x)^3 / (6 * hCur) +
		mEnd * (x - pts[i - 1].x)^3 / (6 * hCur) +
		(pts[i - 1].y - mStart * hCur^2 / 6) * (pts[i].x - x) / hCur +
		(pts[i].y - mEnd * hCur^2 / 6) * (x - pts[i - 1].x) / hCur
	*/

	size_t coef = (i - 1) * 4;
	const float &xStart = pts[i - 1].x, &xEnd = pts[i].x;
	const float &yStart = pts[i - 1].y, &yEnd = pts[i].y;
	float hInv = 1.0f / hCur;
	float h6Inv = 1.0f / (hCur * 6.0f);
	float hSqOver6 = (hCur * hCur) / 6.0f;
	float firstMultiplier = mStart * h6Inv;
	float secondMultiplier = mEnd * h6Inv;
	float thirdMultiplier = (yStart - mStart * hSqOver6) * hInv;
	float fourthMultiplier = (yEnd - mEnd * hSqOver6) * hInv;

	// x^3
	coefficients[coef] =
		-firstMultiplier + // Contribution from first addend of C(x)
		secondMultiplier; // Second addend

	// x^2
	coefficients[coef + 1] =
		3.0f * xEnd * firstMultiplier + // First addend
		-3.0f * xStart * secondMultiplier; // Second

	// x
	coefficients[coef + 2] =
		-3.0f * xEnd * xEnd * firstMultiplier + // First addend
		3.0f * xStart * xStart * secondMultiplier + // Second
		-thirdMultiplier + // Third
		fourthMultiplier; // Fourth

	// constant
	coefficients[coef + 3] =
		xEnd * xEnd * xEnd * firstMultiplier + // First addend
		-xStart * xStart * xStart * secondMultiplier + // Second
		xEnd * thirdMultiplier + // Third
		-xStart * fourthMultiplier; // Fourth
}

/*--------------------------------------
	rnd::CalculateCoefficients
--------------------------------------*/
void rnd::CalculateCoefficients()
{
	numPolynomials = numCurvePoints - 1;
	coefficients.Ensure(numPolynomials * 4);
	float discard;
	PolynomialCoefficients(1, 0.0f, 0.0f, discard);
}

/*
################################################################################################


	CURVE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::SetCurvePoint

IN	v2Set
--------------------------------------*/
int rnd::SetCurvePoint(lua_State* l)
{
	SetCurvePoint(luaL_checknumber(l, 1), luaL_checknumber(l, 2));
	return 0;
}

/*--------------------------------------
LUA	rnd::CurvePoint

IN	iIndex
OUT	v2CurvePoint
--------------------------------------*/
int rnd::CurvePoint(lua_State* l)
{
	vec::LuaPushVec(l, CurvePoint(luaL_checkinteger(l, 1)));
	return 2;
}

/*--------------------------------------
LUA	rnd::NumCurvePoints

OUT	iNum
--------------------------------------*/
int rnd::NumCurvePoints(lua_State* l)
{
	lua_pushinteger(l, NumCurvePoints());
	return 1;
}

/*--------------------------------------
LUA	rnd::FindCurvePoint

IN	xFind
OUT	[iIndex]
--------------------------------------*/
int rnd::FindCurvePoint(lua_State* l)
{
	size_t index = FindCurvePoint(luaL_checknumber(l, 1));

	if(index == -1)
		return 0;

	lua_pushinteger(l, index);
	return 1;
}

/*--------------------------------------
LUA	rnd::DeleteCurvePoint

IN	iIndex
--------------------------------------*/
int rnd::DeleteCurvePoint(lua_State* l)
{
	DeleteCurvePoint(luaL_checkinteger(l, 1));
	return 0;
}

/*--------------------------------------
LUA	rnd::ClearCurvePoints
--------------------------------------*/
int rnd::ClearCurvePoints(lua_State* l)
{
	ClearCurvePoints();
	return 0;
}