// vec.h
// Martynas Ceicys

// FIXME: change to arrays
// FIXME: allow inline by implementing in header?

#ifndef COM_VEC_H
#define COM_VEC_H

#include <float.h>

#include "fp.h"

#define COM_VEC2_PRNT COM_FP_PRNT " " COM_FP_PRNT
#define COM_VEC3_PRNT COM_FP_PRNT " " COM_FP_PRNT " " COM_FP_PRNT
#define COM_PI (com::fp)3.1415926535897932384626433832795

namespace com
{

/*
################################################################################################
	2D VECTOR
################################################################################################
*/

#define COM_VEC2_COMP(v) (v).x, (v).y

class Vec2;
class Vec3;

class Vec2
{
public:
	fp		x, y;

	fp		Mag() const;
	fp		MagSq() const;
	Vec2	Normalized(int* errOut = 0) const;
	Vec2	Resized(fp newMag, int* errOut = 0) const;
	bool	InEps(fp e) const;
	void	StrToVec(const char* str, const char** endPtrOut = 0); // FIXME: free function
	fp		Yaw(fp defYaw = 0.0) const;
	Vec2	Rotated(fp yawDel) const;
	void	Copy(const float* f) {x = (fp)f[0]; y = (fp)f[1];}
	void	Copy(const double* d) {x = (fp)d[0]; y = (fp)d[1];}
	void	CopyTo(float* f) const {f[0] = x; f[1] = y;}
	void	CopyTo(double* d) const {d[0] = x; d[1] = y;}

	fp&		operator[](int i);
	fp		operator[](int i) const;

	Vec2	operator-() const;

	Vec2	operator+(const Vec2& v) const;
	Vec2	operator-(const Vec2& v) const;
	Vec2	operator*(fp f) const;
	Vec2	operator*(const Vec2& v) const; // component-wise
	Vec2	operator/(fp f) const;
	Vec2	operator/(const Vec2& v) const; // component-wise

	Vec2	operator=(fp f);
	void	operator+=(const Vec2& v);
	void	operator-=(const Vec2& v);
	void	operator*=(fp f);
	void	operator*=(const Vec2& v);
	void	operator/=(fp f);
	void	operator/=(const Vec2& v);

	bool	operator==(const Vec2& v) const;
	bool	operator==(fp f) const;
	bool	operator!=(const Vec2& v) const;

	Vec2();
	Vec2(fp f);
	Vec2(fp x, fp y);
	Vec2(const Vec3& v);
};

Vec2	operator*(fp f, const Vec2& v);
fp		Dot(const Vec2& u, const Vec2& v);
Vec2	FixVec(const Vec2& v);

/*
################################################################################################
	3D VECTOR
################################################################################################
*/

#define COM_VEC3_COMP(v) (v).x, (v).y, (v).z

class Vec3
{
public:
	fp		x, y, z;

	fp		Mag() const;
	fp		MagSq() const;
	Vec3	Normalized(int* errOut = 0) const;
	Vec3	Resized(fp newMag, int* errOut = 0) const;
	bool	InEps(fp e) const;
	void	StrToVec(const char* const str, const char** endPtrOut = 0);
	fp		Pitch() const;
	fp		Yaw(fp defYaw = 0.0) const;
	void	PitchYaw(fp& pitchOut, fp& yawOut, fp defYaw = 0.0) const;
	void	Copy(const float* f) {x = (fp)f[0]; y = (fp)f[1]; z = (fp)f[2];}
	void	Copy(const double* d) {x = (fp)d[0]; y = (fp)d[1]; z = (fp)d[2];}
	void	CopyTo(float* f) const {f[0] = x; f[1] = y; f[2] = z;}
	void	CopyTo(double* d) const {d[0] = x; d[1] = y; d[2] = z;}

	fp&		operator[](int i);
	fp		operator[](int i) const;

	Vec3	operator-() const;

	Vec3	operator+(const Vec3& v) const;
	Vec3	operator-(const Vec3& v) const;
	Vec3	operator*(fp f) const;
	Vec3	operator*(const Vec3& v) const; // component-wise
	Vec3	operator/(fp f) const;
	Vec3	operator/(const Vec3& v) const; // component-wise

	Vec3	operator=(fp f);
	void	operator+=(const Vec3& v);
	void	operator-=(const Vec3& v);
	void	operator*=(fp f);
	void	operator*=(const Vec3& v);
	void	operator/=(fp f);
	void	operator/=(const Vec3& v);

	bool	operator==(const Vec3& v) const;
	bool	operator==(fp f) const;
	bool	operator!=(const Vec3& v) const;

	Vec3();
	Vec3(fp f);
	Vec3(fp x, fp y, fp z);
	Vec3(const Vec2& v);
};

Vec3	operator*(fp f, const Vec3& v);
fp		Dot(const Vec3& u, const Vec3& v);
Vec3	Cross(const Vec3& u, const Vec3& v);
Vec3	FixVec(const Vec3& v);
Vec3	VecFromPitchYaw(fp pitch, fp yaw);
void	DirDelta(const Vec3& u, const Vec3& v, Vec3& axisOut, fp& angleOut);
fp		DirAngle(const Vec3& u, const Vec3& v);

}

#endif