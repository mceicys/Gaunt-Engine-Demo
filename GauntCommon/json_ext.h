// json_ext.h
// Martynas Ceicys

#ifndef COM_JSON_EXT_H
#define COM_JSON_EXT_H

#include "json.h"
#include "qua.h"
#include "type.h"
#include "vec.h"

namespace com
{

#define COM_JSON_EXT_GETTER_VARIANT(name, type, func) \
inline int name(const JSVar& var, type& valOut, size_t offset = 0) \
{ \
	int err = 0; \
	valOut = func(var, offset, &err); \
	return err; \
}

/*--------------------------------------
	com::JSVarSetVec
--------------------------------------*/
inline void JSVarSetVec(JSVar& varOut, const Vec2& vec)
{
	Arr<JSVar>& arr = *varOut.SetArray();
	varOut.SetNumElems(2);
	
	for(size_t i = 0; i < 2; i++)
		arr[i].SetNumber(vec[i]);
}

inline void JSVarSetVec(JSVar& varOut, const Vec3& vec)
{
	Arr<JSVar>& arr = *varOut.SetArray();
	varOut.SetNumElems(3);
	
	for(size_t i = 0; i < 3; i++)
		arr[i].SetNumber(vec[i]);
}

/*--------------------------------------
	com::JSVarSetQua
--------------------------------------*/
inline void JSVarSetQua(JSVar& varOut, const Qua& qua)
{
	Arr<JSVar>& arr = *varOut.SetArray();
	varOut.SetNumElems(4);
	
	for(size_t i = 0; i < 4; i++)
		arr[i].SetNumber(qua[i]);
}

/*--------------------------------------
	com::JSVarToVec2
--------------------------------------*/
inline Vec2 JSVarToVec2(const JSVar& var, size_t offset = 0, int* errOut = 0)
{
	const Arr<JSVar>* arr = var.Array();
	if(!arr)
	{
		if(errOut)
			*errOut = 1;

		return (fp)0.0;
	}

	Vec2 vec((fp)0.0);
	for(size_t i = 0; i < 2; i++)
	{
		if(i + offset >= var.NumElems())
		{
			if(errOut)
				*errOut = 1;

			break;
		}

		vec[i] = (*arr)[i + offset].FP(errOut);
	}

	return vec;
}

COM_JSON_EXT_GETTER_VARIANT(ErrJSVarToVec2, Vec2, JSVarToVec2)

/*--------------------------------------
	com::JSVarToVec3
--------------------------------------*/
inline Vec3 JSVarToVec3(const JSVar& var, size_t offset = 0, int* errOut = 0)
{
	const Arr<JSVar>* arr = var.Array();

	if(!arr)
	{
		if(errOut)
			*errOut = 1;

		return (fp)0.0;
	}

	Vec3 vec((fp)0.0);

	for(size_t i = 0; i < 3; i++)
	{
		if(i + offset >= var.NumElems())
		{
			if(errOut)
				*errOut = 1;

			break;
		}

		vec[i] = (*arr)[i + offset].FP(errOut);
	}

	return vec;
}

COM_JSON_EXT_GETTER_VARIANT(ErrJSVarToVec3, Vec3, JSVarToVec3)

/*--------------------------------------
	com::JSVarToQua
--------------------------------------*/
inline Qua JSVarToQua(const JSVar& var, size_t offset = 0, int* errOut = 0)
{
	const Arr<JSVar>* arr = var.Array();
	if(!arr)
	{
		if(errOut)
			*errOut = 1;

		return QUA_IDENTITY;
	}

	Qua qua = QUA_IDENTITY;
	for(size_t i = 0; i < 4; i++)
	{
		if(i + offset >= var.NumElems())
		{
			if(errOut)
				*errOut = 1;

			break;
		}

		qua[i] = (*arr)[i + offset].FP(errOut);
	}

	return qua;
}

COM_JSON_EXT_GETTER_VARIANT(ErrJSVarToQua, Qua, JSVarToQua)

/*--------------------------------------
	com::JSVarToEuler
--------------------------------------*/
inline Qua JSVarToEuler(const JSVar& var, size_t offset = 0, int* errOut = 0)
{
	const Arr<JSVar>* arr = var.Array();
	if(!arr)
	{
		if(errOut)
			*errOut = 1;

		return QUA_IDENTITY;
	}

	fp eul[3] = {(fp)0.0, (fp)0.0, (fp)0.0};

	for(size_t i = 0; i < 3; i++)
	{
		if(i + offset >= var.NumElems())
		{
			if(errOut)
				*errOut = 1;

			break;
		}

		eul[i] = (*arr)[i + offset].FP(errOut);
	}

	return QuaEuler(eul[0], eul[1], eul[2]);
}

COM_JSON_EXT_GETTER_VARIANT(ErrJSVarToEuler, Qua, JSVarToEuler)

/*--------------------------------------
	com::JSVarToOri
--------------------------------------*/
inline Qua JSVarToOri(const JSVar& var, size_t offset = 0, int* errOut = 0)
{
	int err = 0;
	Qua qua = JSVarToQua(var, offset, &err);

	if(err)
	{
		err = 0;
		qua = JSVarToEuler(var, offset, &err);
	}

	if(err && errOut)
		*errOut = 1;

	return qua;
}

COM_JSON_EXT_GETTER_VARIANT(ErrJSVarToOri, Qua, JSVarToOri)

}

#endif