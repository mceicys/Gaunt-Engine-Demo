// obj.h
// Martynas Ceicys

#ifndef COM_OBJ_H
#define COM_OBJ_H

#include "array.h"
#include "vec.h"

namespace com
{

/*
################################################################################################
	LOAD OBJ
################################################################################################
*/

#define COM_OBJ_P	0
#define COM_OBJ_PT	1
#define COM_OBJ_PN	2
#define COM_OBJ_PTN	3

struct obj_mtl
{
	char*	name;
	char*	mapKD;
	fp		ka[3], ke[3];
};

struct obj_tri
{
	size_t	p[3];
	size_t	t[3];
	size_t	n[3];
	size_t	c[3];
	int		mtlIndex; // -1 means none
};

struct obj_obj
{
	Arr<obj_tri>	tris;
	size_t			numTris;
};

struct obj_file
{
	Vec3*		ps;
	size_t		numPs;

	Vec2*		ts;
	size_t		numTs;

	Vec3*		ns;
	size_t		numNs;

	Vec3*		cs; // Not in OBJ spec; defined with "v x y z r g b"
	size_t		numCs;

	obj_obj*	objs;
	size_t		numObjs;

	obj_mtl*	mtls;
	size_t		numMtls;
};

const char*	OBJLoad(const char* filePath, unsigned type, obj_file& dataOut);
const char* MTLLoad(const char* filePath, obj_file& dataInOut);
void		OBJFree(Vec3* ps, Vec2* ts, Vec3* ns, Vec3* cs, obj_obj* objs, size_t numObjs,
			obj_mtl* mtls, size_t numMtls);
void		OBJFree(obj_file& data);

/*
################################################################################################
	SAVE OBJ
Intended for debugging.

FIXME: Make this a class so multiple OBJs can be written at once
################################################################################################
*/

bool	OBJSaveBegin(const char* fileName);
bool	OBJSaveObjBegin();
bool	OBJSavePolyBegin();
bool	OBJSaveVert(const Vec3& v);
bool	OBJSaveTexVert(const Vec3& v, const Vec2& u);
bool	OBJSavePolyEnd();
bool	OBJSavePoly(const Vec3* verts, size_t numVerts);
bool	OBJSaveEnd();

}

#endif