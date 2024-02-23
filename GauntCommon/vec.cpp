// vec.cpp
// Martynas Ceicys

#include <stdlib.h>
#include <math.h>

#include "vec.h"

/*
################################################################################################


	2D VECTOR


################################################################################################
*/

com::fp com::Vec2::Mag() const
{
	return sqrt(x * x + y * y);
}

com::fp com::Vec2::MagSq() const
{
	return x * x + y * y;
}

com::Vec2 com::Vec2::Normalized(int* errOut) const
{
	if(fp mag = Mag())
		return *this / mag;

	if(errOut)
		*errOut = 1;

	return *this;
}

com::Vec2 com::Vec2::Resized(fp newMag, int* errOut) const
{
	if(fp mag = Mag())
		return *this * (newMag / mag);

	if(errOut)
		*errOut = 1;

	return *this;
}

bool com::Vec2::InEps(fp e) const
{
	return fabs(x) <= e && fabs(y) <= e;
}

void com::Vec2::StrToVec(const char* str, const char** endPtrOut)
{
	const char* end;
	x = COM_STR_TO_FP(str, &end);
	y = COM_STR_TO_FP(end, &end);

	if(endPtrOut)
		*endPtrOut = end;
}

// Range [-pi, pi]
com::fp com::Vec2::Yaw(fp defYaw) const
{
	if(x == (fp)0.0 && y == (fp)0.0)
		return defYaw;
	
	return atan2(y, x);
}

com::Vec2 com::Vec2::Rotated(fp yawDel) const
{
	fp c = cos(yawDel);
	fp s = sin(yawDel);
	return com::Vec2(x * c - y * s, x * s + y * c);
}

com::fp& com::Vec2::operator[](int i)
{
	switch(i)
	{
	case 0:
		return x;
	case 1:
	default:
		return y;
	}
}

com::fp com::Vec2::operator[](int i) const
{
	switch(i)
	{
	case 0:
		return x;
	case 1:
	default:
		return y;
	}
}

com::Vec2 com::Vec2::operator-() const
{
	return Vec2(-x, -y);
}

com::Vec2 com::Vec2::operator+(const Vec2& v) const
{
	return Vec2(x + v.x, y + v.y);
}

com::Vec2 com::Vec2::operator-(const Vec2& v) const
{
	return Vec2(x - v.x, y - v.y);
}

com::Vec2 com::Vec2::operator*(fp f) const
{
	return Vec2(x * f, y * f);
}

com::Vec2 com::Vec2::operator*(const Vec2& v) const
{
	return Vec2(x * v.x, y * v.y);
}

com::Vec2 com::Vec2::operator/(fp f) const
{
	return Vec2(x / f, y / f);
}

com::Vec2 com::Vec2::operator/(const Vec2& v) const
{
	return Vec2(x / v.x, y / v.y);
}

com::Vec2 com::Vec2::operator=(fp f)
{
	x = f;
	y = f;
	return *this;
}

void com::Vec2::operator+=(const Vec2& v)
{
	x += v.x;
	y += v.y;
}

void com::Vec2::operator-=(const Vec2& v)
{
	x -= v.x;
	y -= v.y;
}

void com::Vec2::operator*=(fp f)
{
	x *= f;
	y *= f;
}

void com::Vec2::operator*=(const Vec2& v)
{
	x *= v.x;
	y *= v.y;
}

void com::Vec2::operator/=(fp f)
{
	x /= f;
	y /= f;
}

void com::Vec2::operator/=(const Vec2& v)
{
	x /= v.x;
	y /= v.y;
}

bool com::Vec2::operator==(const Vec2& v) const
{
	return x == v.x && y == v.y;
}

bool com::Vec2::operator==(fp f) const
{
	return x == f && y == f;
}

bool com::Vec2::operator!=(const Vec2& v) const
{
	return x != v.x || y != v.y;
}

com::Vec2::Vec2() : x(0.0f), y(0.0f){}

com::Vec2::Vec2(fp f) : x(f), y(f){}

com::Vec2::Vec2(fp x, fp y) : x(x), y(y){}

com::Vec2::Vec2(const Vec3& v) : x(v.x), y(v.y){}

com::Vec2 com::operator*(fp f, const Vec2& v)
{
	return v * f;
}

com::fp com::Dot(const Vec2& u, const Vec2& v)
{
	return u.x * v.x + u.y * v.y;
}

com::Vec2 com::FixVec(const Vec2& v)
{
	return com::Vec2(FixFP(v.x), FixFP(v.y));
}

/*
################################################################################################


	3D VECTOR


################################################################################################
*/

com::fp com::Vec3::Mag() const
{
	return sqrt(x * x + y * y + z * z);
}

com::fp com::Vec3::MagSq() const
{
	return x * x + y * y + z * z;
}

com::Vec3 com::Vec3::Normalized(int* errOut) const
{
	if(fp mag = Mag())
		return *this / mag;

	if(errOut)
		*errOut = 1;

	return *this;
}

com::Vec3 com::Vec3::Resized(fp newMag, int* errOut) const
{
	if(fp mag = Mag())
		return *this * (newMag / mag);

	if(errOut)
		*errOut = 1;

	return *this;
}

bool com::Vec3::InEps(fp e) const
{
	return fabs(x) <= e && fabs(y) <= e && fabs(z) <= e;
}

void com::Vec3::StrToVec(const char* const str, const char** endPtrOut)
{
	const char* end;
	x = COM_STR_TO_FP(str, &end);
	y = COM_STR_TO_FP(end, &end);
	z = COM_STR_TO_FP(end, &end);

	if(endPtrOut)
		*endPtrOut = end;
}

// Range [-pi / 2, pi / 2]
com::fp com::Vec3::Pitch() const
{
	if(x == (fp)0.0 && y == (fp)0.0)
	{
		if(z > (fp)0.0)
			return -COM_PI * (fp)0.5;
		else if(z < (fp)0.0)
			return COM_PI * (fp)0.5;
		else
			return (fp)0.0;
	}
	
	return -atan(z / Vec2(x, y).Mag());
}

// Range [-pi, pi]
com::fp com::Vec3::Yaw(fp defYaw) const
{
	if(x == (fp)0.0 && y == (fp)0.0)
		return defYaw;
	
	return atan2(y, x);
}

void com::Vec3::PitchYaw(fp& pitchOut, fp& yawOut, fp defYaw) const
{
	if(x == (fp)0.0 && y == (fp)0.0)
	{
		yawOut = defYaw;

		if(z > (fp)0.0)
			pitchOut = -COM_PI * (fp)0.5;
		else if(z < (fp)0.0)
			pitchOut = COM_PI * (fp)0.5;
		else
			pitchOut = (fp)0.0;

		return;
	}
	
	pitchOut = -atan(z / Vec2(x, y).Mag());
	yawOut = atan2(y, x);
}

com::fp& com::Vec3::operator[](int i)
{
	switch(i)
	{
	case 0:
		return x;
	case 1:
		return y;
	case 2:
	default:
		return z;	
	}
}

com::fp com::Vec3::operator[](int i) const
{
	switch(i)
	{
	case 0:
		return x;
	case 1:
		return y;
	case 2:
	default:
		return z;
	}
}

com::Vec3 com::Vec3::operator-() const
{
	return Vec3(-x, -y, -z);
}

com::Vec3 com::Vec3::operator+(const Vec3& v) const
{
	return Vec3(x + v.x, y + v.y, z + v.z);
}

com::Vec3 com::Vec3::operator-(const Vec3& v) const
{
	return Vec3(x - v.x, y - v.y, z - v.z);
}

com::Vec3 com::Vec3::operator*(fp f) const
{
	return Vec3(x * f, y * f, z * f);
}

com::Vec3 com::Vec3::operator*(const Vec3& v) const
{
	return Vec3(x * v.x, y * v.y, z * v.z);
}

com::Vec3 com::Vec3::operator/(fp f) const
{
	return Vec3(x / f, y / f, z / f);
}

com::Vec3 com::Vec3::operator/(const Vec3& v) const
{
	return Vec3(x / v.x, y / v.y, z / v.z);
}

com::Vec3 com::Vec3::operator=(fp f)
{
	x = f;
	y = f;
	z = f;
	return *this;
}

void com::Vec3::operator+=(const Vec3& v)
{
	x += v.x;
	y += v.y;
	z += v.z;
}

void com::Vec3::operator-=(const Vec3& v)
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
}

void com::Vec3::operator*=(fp f)
{
	x *= f;
	y *= f;
	z *= f;
}

void com::Vec3::operator*=(const Vec3& v)
{
	x *= v.x;
	y *= v.y;
	z *= v.z;
}

void com::Vec3::operator/=(fp f)
{
	x /= f;
	y /= f;
	z /= f;
}

void com::Vec3::operator/=(const Vec3& v)
{
	x /= v.x;
	y /= v.y;
	z /= v.z;
}

bool com::Vec3::operator==(const Vec3& v) const
{
	return x == v.x && y == v.y && z == v.z;
}

bool com::Vec3::operator==(fp f) const
{
	return x == f && y == f && z == f;
}

bool com::Vec3::operator!=(const Vec3& v) const
{
	return x != v.x || y != v.y || z != v.z;
}

com::Vec3::Vec3() : x(0.0f), y(0.0f), z(0.0f){}

com::Vec3::Vec3(fp f) : x(f), y(f), z(f){}

com::Vec3::Vec3(fp x, fp y, fp z) : x(x), y(y), z(z){}

com::Vec3::Vec3(const Vec2& v) : x(v.x), y(v.y), z(0.0f){}

com::Vec3 com::operator*(fp f, const Vec3& v)
{
	return v * f;
}

com::fp com::Dot(const Vec3& u, const Vec3& v)
{
	return u.x * v.x + u.y * v.y + u.z * v.z;
}

com::Vec3 com::Cross(const Vec3& u, const Vec3& v)
{
	return Vec3(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}

com::Vec3 com::FixVec(const Vec3& v)
{
	return com::Vec3(FixFP(v.x), FixFP(v.y), FixFP(v.z));
}

com::Vec3 com::VecFromPitchYaw(fp pitch, fp yaw)
{
	fp cosPitch = cos(pitch);
	return com::Vec3(cos(yaw) * cosPitch, sin(yaw) * cosPitch, sin(-pitch));
}

void com::DirDelta(const Vec3& u, const Vec3& v, Vec3& axisOut, fp& angleOut)
{
	int err = 0;
	Vec3 un = u.Normalized(&err);
	Vec3 vn = v.Normalized(&err);

	if(err)
	{
		axisOut = Vec3((fp)1.0, (fp)0.0, (fp)0.0);
		angleOut = (fp)0.0;
		return;
	}

	axisOut = Cross(un, vn).Normalized(&err);
	fp dot = Dot(un, vn);

	if(dot > (fp)1.0)
		dot = (fp)1.0;
	else if(dot < (fp)-1.0)
		dot = (fp)-1.0;

	if(err)
	{
		// No definite perpendicular direction
		if(dot >= (fp)0.0)
		{
			// Facing same direction
			axisOut = Vec3((fp)1.0, (fp)0.0, (fp)0.0);
			angleOut = (fp)0.0;
		}
		else
		{
			// Facing opposite directions
			angleOut = COM_PI; // 180 degrees

			if(fabs(un.z) > (fp)0.999)
			{
				// Vectors are facing up and down, rotate on x axis
				axisOut = Vec3((fp)1.0, (fp)0.0, (fp)0.0);
			}
			else
			{
				// FIXME: test
				// Get an axis by projecting z+ vector onto the plane with normal un
				err = 0;
				axisOut = (un * Dot(Vec3((fp)0.0, (fp)0.0, (fp)1.0), un)).Normalized(&err);

				if(err) // Shouldn't happen
					axisOut = Vec3((fp)0.0, (fp)0.0, (fp)1.0); // Just rotate on z, whatever
			}
		}

		return;
	}

	angleOut = acos(dot);
}

com::fp com::DirAngle(const Vec3& u, const Vec3& v)
{
	int err = 0;
	Vec3 un = u.Normalized(&err);
	Vec3 vn = v.Normalized(&err);

	if(err)
		return (fp)0.0;

	fp dot = Dot(un, vn);

	if(dot > (fp)1.0)
		dot = (fp)1.0;
	else if(dot < (fp)-1.0)
		dot = (fp)-1.0;

	return acos(dot);
}