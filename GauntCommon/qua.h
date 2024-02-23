// qua.h
// Martynas Ceicys

// FIXME: don't inline functions?

#ifndef COM_QUA_H
#define COM_QUA_H

#include <math.h>

#include "math.h" // FIXME TEMP
#include "fp.h"
#include "vec.h"

#define COM_QUA_PRNT COM_FP_PRNT " " COM_FP_PRNT " " COM_FP_PRNT " " COM_FP_PRNT
#define COM_QUA_COMP(u) (u).q[0], (u).q[1], (u).q[2], (u).q[3]
#define COM_PI (com::fp)3.1415926535897932384626433832795
#define COM_HALF_PI (com::fp)1.5707963267948966192313216916398

namespace com
{

class Qua;

extern const Qua QUA_IDENTITY; // FIXME: function

Qua QuaAxisAngle(const Vec3& v, fp angle);

/*======================================
	com::Qua
======================================*/
class Qua
{
public:
	fp		q[4]; // x, y, z, w

	fp&		operator[](int i) {return q[i];}
	fp		operator[](int i) const {return q[i];}

	Qua&	operator+=(const Qua& r) {return *this = *this + r;}
	Qua&	operator-=(const Qua& r) {return *this = *this - r;}
	Qua&	operator*=(fp f) {return *this = *this * f;}
	Qua&	operator*=(const Qua& r) {return *this = *this * r;}
	Qua&	operator/=(fp f) {return *this = *this / f;}
	fp		Norm() const {return sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);}

	bool operator==(const Qua& r) const
	{
		return q[0] == r[0] && q[1] == r[1] && q[2] == r[2] && q[3] == r[3];
	}

	bool operator!=(const Qua& r) const
	{
		return q[0] != r[0] || q[1] != r[1] || q[2] != r[2] || q[3] != r[3];
	}

	Qua operator-() const
	{
		Qua s = {-q[0], -q[1], -q[2], -q[3]};
		return s;
	}

	Qua operator+(const Qua& r) const
	{
		Qua s = {q[0] + r[0], q[1] + r[1], q[2] + r[2], q[3] + r[3]};
		return s;
	}

	Qua operator-(const Qua& r) const
	{
		Qua s = {q[0] - r[0], q[1] - r[1], q[2] - r[2], q[3] - r[3]};
		return s;
	}

	Qua operator*(fp f) const
	{
		Qua s = {q[0] * f, q[1] * f, q[2] * f, q[3] * f};
		return s;
	}

	Qua operator*(const Qua& r) const
	{
		Qua s =
		{
			q[0] * r[3] + q[3] * r[0] - q[2] * r[1] + q[1] * r[2],
			q[1] * r[3] + q[2] * r[0] + q[3] * r[1] - q[0] * r[2],
			q[2] * r[3] - q[1] * r[0] + q[0] * r[1] + q[3] * r[2],
			q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2]
		};

		return s;
	}

	Qua operator*(const Vec3& v) const
	{
		Qua s =
		{
			q[3] * v[0] - q[2] * v[1] + q[1] * v[2],
			q[2] * v[0] + q[3] * v[1] - q[0] * v[2],
			-q[1] * v[0] + q[0] * v[1] + q[3] * v[2],
			-q[0] * v[0] - q[1] * v[1] - q[2] * v[2]
		};

		return s;
	}

	Qua operator/(fp f) const
	{
		Qua s = {q[0] / f, q[1] / f, q[2] / f, q[3] / f};
		return s;
	}

	Qua Normalized() const
	{
		if(fp n = Norm())
			return *this / n;
		
		return *this;
	}

	Qua Conjugate() const
	{
		Qua r = {-q[0], -q[1], -q[2], q[3]};
		return r;
	}

	Qua Inverse() const
	{
		if(fp nn = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
			return Conjugate() / nn;
		
		return *this;
	}

	bool Identity() const {return q[3] == (fp)1.0;} // Assumes normalized

	// Range [-pi, pi]
	fp Roll() const
	{
		fp y = q[1] * q[2] + q[0] * q[3];
		fp x = (fp)0.5 - (q[0] * q[0] + q[1] * q[1]);

		if(fabs(y) < (fp)0.00001 && fabs(x) < (fp)0.00001)
			return (fp)0.0;
		else
			return atan2(y, x);
	}

	// Range [-pi / 2, pi / 2]
	fp Pitch() const
	{
		fp x = (fp)-2.0 * (q[0] * q[2] - q[1] * q[3]);

		if(x >= (fp)1.0)
			return COM_HALF_PI;
		else if(x <= (fp)-1.0)
			return -COM_HALF_PI;
		else
			return asin(x);
	}

	// Range [-pi, pi]
	fp Yaw() const
	{
		fp y = q[0] * q[1] + q[2] * q[3];
		fp x = (fp)0.5 - (q[1] * q[1] + q[2] * q[2]);

		if(fabs(y) < (fp)0.00001 && fabs(x) < (fp)0.00001)
		{
			fp a = (fp)2.0 * atan2(q[0], q[3]);

			if(q[0] * q[2] - q[1] * q[3] >= (fp)0.0)
				return a;
			else
				return -a;
		}
		else
			return atan2(y, x);
	}

	Vec3 Dir() const
	{
		return Vec3(
			(fp)1.0 - (fp)2.0 * (q[1] * q[1] + q[2] * q[2]),
			(fp)2.0 * (q[0] * q[1] + q[2] * q[3]),
			(fp)2.0 * (q[0] * q[2] - q[1] * q[3])
		);
	}

	Vec3 Left() const
	{
		return Vec3(
			(fp)2.0 * (q[0] * q[1] - q[2] * q[3]),
			(fp)2.0 * (q[1] * q[1] + q[3] * q[3]) - (fp)1.0,
			(fp)2.0 * (q[0] * q[3] + q[1] * q[2])
		);
	}

	Vec3 Up() const
	{
		return Vec3(
			(fp)2.0 * (q[1] * q[3] + q[0] * q[2]),
			(fp)2.0 * (q[1] * q[2] - q[0] * q[3]),
			(fp)2.0 * (q[2] * q[2] + q[3] * q[3]) - (fp)1.0
		);
	}

	fp Angle() const
	{
		return fp(2.0) * acos(q[3]);
	}

	void AxisAngle(Vec3& axisOut, fp& angleOut) const
	{
		fp s = sqrt((fp)1.0 - q[3] * q[3]); // sin(acos(q[3]));

		if(!s)
		{
			axisOut = (fp)0.0;
			angleOut = (fp)0.0;
			return;
		}

		s = (fp)1.0 / s;
		axisOut[0] = q[0] * s;
		axisOut[1] = q[1] * s;
		axisOut[2] = q[2] * s;
		angleOut = (fp)2.0 * acos(q[3]);
	}

	Qua AngleSum(fp angle) const
	{
		return HalfAngleSet(acos(q[3]) + angle * (fp)0.5);
	}

	Qua AngleApproach(fp target, fp delta) const
	{
		target *= (fp)0.5;
		fp hang = acos(q[3]);
		fp dif = WrappedHalfAngle(target - hang);
		delta *= (fp)0.5;

		if(dif > (fp)0.0)
		{
			if(delta < dif)
				hang += delta;
			else
				hang = target;
		}
		else if(dif < (fp)0.0)
		{
			if(delta < -dif)
				hang -= delta;
			else
				hang = target;
		}
		else
			return *this;

		return HalfAngleSet(hang);
	}

	Qua AngleClamp(fp max) const
	{
		max *= (fp)0.5;
		fp hang = acos(q[3]);
		
		if(hang > max)
			hang = max;
		else
			return *this;

		return HalfAngleSet(hang);
	}

	Qua AngleSet(fp angle) const
	{
		return HalfAngleSet(angle * (fp)0.5);
	}

	// Returns smallest delta quaternion to face given vector
	Qua LookDelta(const Vec3& v) const
	{
		Vec3 axis;
		fp angle;
		LookDeltaAxisAngle(v, axis, angle);
		return QuaAxisAngle(axis, angle);

		/*
		int err = 0;
		Vec3 vn = v.Normalized(&err);

		if(err)
			return QUA_IDENTITY;

		Vec3 dir = Dir();
		Vec3 cross = Cross(dir, vn).Normalized(&err);
		fp dot = Dot(dir, vn);

		if(dot > (fp)1.0)
			dot = (fp)1.0;
		else if(dot < (fp)-1.0)
			dot = (fp)-1.0;

		if(err)
		{
			// No definite perpendicular direction
			if(dot >= (fp)0.0)
				return QUA_IDENTITY; // Facing same direction
			else
			{
				// Facing opposite directions
				// FIXME: is this correct? should be equivalent to QuaAxis(Up(), COM_PI)
				const Qua FLIPPED = {(fp)0.0, (fp)0.0, (fp)1.0, (fp)0.0}; // Relatively yaw 180
				return FLIPPED;
			}
		}

		return QuaAxis(cross, acos(dot));
		*/
	}

	// Same as LookDelta but outputs axis and angle
	void LookDeltaAxisAngle(const Vec3& v, Vec3& axisOut, fp& angleOut) const
	{
		int err = 0;
		Vec3 vn = v.Normalized(&err);

		if(err)
		{
			axisOut = Vec3((fp)1.0, (fp)0.0, (fp)0.0);
			angleOut = (fp)0.0;
			return;
		}

		Vec3 dir = Dir();
		axisOut = Cross(dir, vn).Normalized(&err);
		fp dot = Dot(dir, vn);

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
				axisOut = Up(); // Relative yaw
				angleOut = COM_PI; // 180 degrees
			}

			return;
		}

		angleOut = acos(dot);
	}

private:

	// FIXME: template WrappedAngle?
	fp WrappedHalfAngle(fp hang) const
	{
		if(hang > COM_HALF_PI)
		{
			fp wrap = fmod(hang, COM_PI);
		
			if(wrap > COM_HALF_PI)
				return wrap - COM_PI;
			else
				return wrap;
		}	
		else if(hang < -COM_HALF_PI)
		{
			fp wrap = fmod(hang, COM_PI);
		
			if(wrap < -COM_HALF_PI)
				return wrap + COM_PI;
			else
				return wrap;
		}	
		else
			return hang;
	}

	// FIXME: make a public version to set the angle
	Qua HalfAngleSet(fp halfAng) const
	{
		fp s = sqrt((fp)1.0 - q[3] * q[3]); // sin(acos(q[3]));

		if(!s)
			return *this;

		fp f = sin(halfAng) / s;
		Qua r = {q[0] * f, q[1] * f, q[2] * f, cos(halfAng)};
		return r;
	}
};

/*--------------------------------------
	com::Dot
--------------------------------------*/
inline fp Dot(const Qua& q, const Qua& r)
{
	return q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3];
}

/*--------------------------------------
	com::QuaAxisAngle

Returns a unit quaternion q where q * u * q.Conjugate() gives pure quaternion u rotated around
unit vector v by angle.
--------------------------------------*/
inline Qua QuaAxisAngle(const Vec3& v, fp angle)
{
	angle *= (fp)0.5;
	fp s = sin(angle);
	Qua q = {s * v[0], s * v[1], s * v[2], cos(angle)};
	return q;
}

/*--------------------------------------
	com::QuaAngularVelocity

Like QuaAxisAngle but vel's magnitude is the rotation's angle.
--------------------------------------*/
inline Qua QuaAngularVelocity(const Vec3& vel, fp time = (fp)1.0)
{
	fp mag = vel.Mag();

	if(!mag)
		return QUA_IDENTITY;

	return QuaAxisAngle(vel / mag, mag * time);
}

/*--------------------------------------
	com::QuaEuler

Returns a unit quaternion q where q * u * q.Conjugate() gives pure quaternion u rotated to given
euler orientation (extrinsic order XYZ; roll, pitch, yaw).
--------------------------------------*/
inline Qua QuaEulerYaw(fp z)
{
	Qua qz = {(fp)0.0, (fp)0.0, sin(z * (fp)0.5), cos(z * (fp)0.5)};
	return qz;
};

inline Qua QuaEulerPitchYaw(fp y, fp z)
{
	fp sy = sin(y * (fp)0.5), cy = cos(y * (fp)0.5);
	fp sz = sin(z * (fp)0.5), cz = cos(z * (fp)0.5);

	Qua qzy =
	{
		-sz * sy,
		cz * sy,
		sz * cy,
		cz * cy
	};

	return qzy;
};

inline Qua QuaEuler(fp x, fp y, fp z)
{
	/*
	Qua qx = {sin(x * (fp)0.5), (fp)0.0, (fp)0.0, cos(x * (fp)0.5)};
	Qua qy = {(fp)0.0, sin(y * (fp)0.5), (fp)0.0, cos(y * (fp)0.5)};
	Qua qz = {(fp)0.0, (fp)0.0, sin(z * (fp)0.5), cos(z * (fp)0.5)};

	return qz * qy * qx;
	*/

	Qua qzy = QuaEulerPitchYaw(y, z);
	fp sx = sin(x * (fp)0.5), cx = cos(x * (fp)0.5);

	Qua qzyx =
	{
		qzy[0] * cx + qzy[3] * sx,
		qzy[1] * cx + qzy[2] * sx,
		qzy[2] * cx - qzy[1] * sx,
		qzy[3] * cx - qzy[0] * sx,
	};

	return qzyx;
};

/*--------------------------------------
	com::QuaEulerReverse

Extrinsic order ZYX; yaw, pitch, roll.
--------------------------------------*/
inline Qua QuaEulerReverse(fp x, fp y, fp z)
{
	/*
	Qua qx = {sin(x * (fp)0.5), (fp)0.0, (fp)0.0, cos(x * (fp)0.5)};
	Qua qy = {(fp)0.0, sin(y * (fp)0.5), (fp)0.0, cos(y * (fp)0.5)};
	Qua qz = {(fp)0.0, (fp)0.0, sin(z * (fp)0.5), cos(z * (fp)0.5)};

	return qx * qy * qz;
	*/

	fp sx = sin(x * (fp)0.5), cx = cos(x * (fp)0.5);
	fp sy = sin(y * (fp)0.5), cy = cos(y * (fp)0.5);

	Qua qxy =
	{
		sx * cy,
		cx * sy,
		sx * sy,
		cx * cy
	};

	fp sz = sin(z * (fp)0.5), cz = cos(z * (fp)0.5);

	Qua qxyz =
	{
		qxy[0] * cz + qxy[1] * sz,
		qxy[1] * cz - qxy[0] * sz,
		qxy[2] * cz + qxy[3] * sz,
		qxy[3] * cz - qxy[2] * sz,
	};

	return qxyz;
};

/*--------------------------------------
	com::QuaLook

FIXME: remove?
FIXME: make this take an up vector
--------------------------------------*/
inline Qua QuaLook(const Vec3& v)
{
	fp vMag = v.Mag();
	if(!vMag)
		return com::QUA_IDENTITY;

	//const Vec3 UNIT((fp)1.0, (fp)0.0, (fp)0.0);
	fp unitDot = v.x / vMag; // Dot(UNIT, v.Normalized())
	Vec3 axis((fp)0.0, -v.z, v.y); // Cross(UNIT, v)
	fp axisMag = axis.Mag();

	if(!axisMag)
	{
		if(unitDot >= (fp)0.0)
			return com::QUA_IDENTITY;
		else
		{
			const Qua FLIPPED = {(fp)0.0, (fp)0.0, (fp)1.0, (fp)0.0};
			return FLIPPED;
		}
	}

	axis /= axisMag;
	fp angle = acos(unitDot);
	return QuaAxisAngle(axis, angle);
}

/*--------------------------------------
	com::QuaLookUpright
--------------------------------------*/
inline Qua QuaLookUpright(const Vec3& v)
{
	// FIXME: calc with less trig funcs
		// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/

	fp pitch, yaw;
	v.PitchYaw(pitch, yaw);
	return QuaEulerPitchYaw(pitch, yaw);
}

/*--------------------------------------
	com::Delta

Returns quaternion that applying to a will result in b:
b = delta * a
--------------------------------------*/
inline Qua Delta(const Qua& a, const Qua& b)
{
	return b * a.Conjugate();
}

/*--------------------------------------
	com::LocalDelta

Delta to b in local space of a.
b = a * delta
--------------------------------------*/
inline Qua LocalDelta(const Qua& a, const Qua& b)
{
	return a.Conjugate() * b;
}

/*--------------------------------------
	com::GlobalToLocal

Converts a global delta to a delta local to ref.

globalDelta * ref = ref * localDelta
--------------------------------------*/
inline Qua GlobalToLocal(const Qua& ref, const Qua& globalDelta)
{
	return ref.Conjugate() * globalDelta * ref;
}

/*--------------------------------------
	com::LocalToGlobal

Converts a delta local to ref to a global delta.

ref * localDelta = globalDelta * ref
--------------------------------------*/
inline Qua LocalToGlobal(const Qua& ref, const Qua& localDelta)
{
	return ref * localDelta * ref.Conjugate();
}

/*--------------------------------------
	com::AngleDelta

Returns smallest angle.
--------------------------------------*/
inline fp AngleDelta(const Qua& q, const Qua& r)
{
	fp dot = Dot(q, r);

	if(dot < (fp)0.0)
		dot = -dot;

	if(dot > (fp)1.0)
		dot = (fp)1.0;

	return acos(dot) * (fp)2.0;
}

/*--------------------------------------
	com::VecQua
--------------------------------------*/
inline Vec3 VecQua(const Qua& q)
{
	return Vec3(q[0], q[1], q[2]);
}

/*--------------------------------------
	com::VecMultipliedQua
--------------------------------------*/
inline Vec3 VecMultipliedQua(const Qua& q, const Qua& r)
{
	return Vec3(
		q[0] * r[3] + q[3] * r[0] - q[2] * r[1] + q[1] * r[2],
		q[1] * r[3] + q[2] * r[0] + q[3] * r[1] - q[0] * r[2],
		q[2] * r[3] - q[1] * r[0] + q[0] * r[1] + q[3] * r[2]
	);
}

/*--------------------------------------
	com::VecRot
--------------------------------------*/
inline Vec3 VecRot(const Vec3& v, const Qua& q)
{
	return VecMultipliedQua(q * v, q.Conjugate());
}

/*--------------------------------------
	com::VecRotInv
--------------------------------------*/
inline Vec3 VecRotInv(const Vec3& v, const Qua& q)
{
	return VecMultipliedQua(q.Conjugate() * v, q);
}

/*--------------------------------------
	com::StrToQua
--------------------------------------*/
inline Qua StrToQua(const char* str, const char** endPtrOut = 0)
{
	const char* end;

	Qua q = 
	{
		COM_STR_TO_FP(str, &end),
		COM_STR_TO_FP(end, &end),
		COM_STR_TO_FP(end, &end),
		COM_STR_TO_FP(end, &end)
	};

	if(endPtrOut)
		*endPtrOut = end;

	return q;
}

/*--------------------------------------
	com::MatQua
--------------------------------------*/
inline void MatQua(const Qua& q, fp (&matOut)[16])
{
	fp xx = q[0] * q[0];
	fp xy = q[0] * q[1];
	fp xz = q[0] * q[2];
	fp xw = q[0] * q[3];
	fp yy = q[1] * q[1];
	fp yz = q[1] * q[2];
	fp yw = q[1] * q[3];
	fp zz = q[2] * q[2];
	fp zw = q[2] * q[3];
	fp ww = q[3] * q[3];

	matOut[0] = 1.0f - 2.0f * (yy + zz);
	matOut[1] = 2.0f * (xy - zw);
	matOut[2] = 2.0f * (xz + yw);
	matOut[3] = 0.0f;

	matOut[4] = 2.0f * (xy + zw);
	matOut[5] = 1.0f - 2.0f * (xx + zz);
	matOut[6] = 2.0f * (yz - xw);
	matOut[7] = 0.0f;

	matOut[8] = 2.0f * (xz - yw);
	matOut[9] = 2.0f * (yz + xw);
	matOut[10] = 1.0f - 2.0f * (xx + yy);
	matOut[11] = 0.0f;

	matOut[12] = 0.0f;
	matOut[13] = 0.0f;
	matOut[14] = 0.0f;
	matOut[15] = 1.0f;
}

Qua Slerp(Qua q, const Qua& r, fp f);

}

#endif