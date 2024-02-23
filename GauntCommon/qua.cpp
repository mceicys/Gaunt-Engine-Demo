// qua.cpp
// Martynas Ceicys

#include "qua.h"

// FIXME: function for initialization
const com::Qua com::QUA_IDENTITY = {(com::fp)0.0, (com::fp)0.0, (com::fp)0.0, (com::fp)1.0};

/*--------------------------------------
	com::Slerp

q and r are unit quaternions.
--------------------------------------*/
com::Qua com::Slerp(Qua q, const Qua& r, fp f)
{
	fp dot = Dot(q, r);

	if(dot < (fp)0.0)
	{
		dot = -dot;
		q = -q;
	}

	if(dot >= (fp)1.0)
		return q;

	fp angle = acos(dot);
	fp sa = sin(angle);

	if(sa == (fp)0.0)
		return q;

	return q * (sin(((fp)1.0 - f) * angle) / sa) + r * (sin(f * angle) / sa);
}