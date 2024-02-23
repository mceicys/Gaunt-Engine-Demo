// obj.cpp
// Martynas Ceicys

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "obj.h"
#include "array.h"
#include "io.h"
#include "math.h"

#define DEFAULT_NUM_VERTEX_DATA	128
#define DEFAULT_NUM_OBJECTS		1
#define DEFAULT_NUM_TRIANGLES	16
#define DEFAULT_NUM_MATERIALS	4

namespace com
{
	// LOAD OBJ
	const char*	OBJClose(FILE* f, Arr<Vec3>& ps, Arr<Vec2>& ts, Arr<Vec3>& ns, Arr<Vec3>& cs,
				Arr<obj_obj>& objs, size_t numObjs, Arr<obj_mtl>& mtls, size_t numMtls,
				const char* err);
	obj_obj*	NextObj(Arr<obj_obj>& objsInOut, size_t& numObjsInOut);
	bool		ParseVec2Tokens(const char* u, const char* v, Vec2& vecOut);
	bool		ParseVec3Tokens(const char* x, const char* y, const char* z, Vec3& vecOut);
	const char*	ParseFace(char** tok, size_t numPs, size_t numTs, size_t numNs, size_t numCs,
				unsigned type, obj_tri& triOut);
	const char*	ParseVertex(const char* str, size_t numPs, size_t numTs, size_t numNs,
				unsigned type, size_t& piOut, size_t& tiOut, size_t& niOut);
	const char*	ConvertIndex(long i, size_t num, size_t& iOut);
	obj_mtl*	ParseUseMaterial(const char* name, Arr<obj_mtl>& mtlsInOut,
				size_t& numMtlsInOut);
	const char*	ParseNumberSequence(const char* str, size_t num, size_t numReq, fp* fOut);

				template<char comp, typename comp_type>
				void OptimizeComponent(comp_type* compsInOut, size_t& numCompsInOut,
				size_t* redirectsInOut, obj_obj* objsInOut, size_t numObjs);
}

/*
################################################################################################


	LOAD OBJ


################################################################################################
*/

/*--------------------------------------
	com::OBJLoad

Loads vertex components and triangles from a Wavefront OBJ file.

type is either COM_OBJ_P, COM_OBJ_PT, COM_OBJ_PN, or COM_OBJ_PTN. P is position, T is texture
coordinate, and N is normal. It indicates which components must be defined; an error is returned
if they are not present. Components not in the type are optional; if they are not defined, they
are set to index 0. Before using optional components, make sure index 0 is not out of bounds.

Triangles have component indices which correspond to the component arrays. Triangles also have
material indices. The triangle has no material if the index is negative.

N-gons are triangulated into a fan with the first vertex as the center.

The output component arrays do not have duplicates.
	FIXME: Make this optional

Smooth groups are ignored. The file must include normals for smoothing to work.

Call OBJFree to free the output arrays.

Returns an error string if loading failed or no objects were specified, and 0 otherwise.
--------------------------------------*/
#define OBJ_LOAD_FAIL(err) return OBJClose(f, ps, ts, ns, cs, objs, numObjs, mtls, numMtls, err)

const char*	com::OBJLoad(const char* filePath, unsigned type, obj_file& d)
{
	d.objs = 0;
	d.mtls = 0;

	FILE* f = fopen(filePath, "r");

	if(!f)
		return "Could not open file";

	char* tok[7];

	size_t numObjs = 0, numPs = 0, numTs = 0, numNs = 0, numCs = 0, numMtls = 0;
	Arr<obj_obj> objs(DEFAULT_NUM_OBJECTS);
	Arr<Vec3> ps(DEFAULT_NUM_VERTEX_DATA);
	Arr<Vec2> ts(DEFAULT_NUM_VERTEX_DATA);
	Arr<Vec3> ns(DEFAULT_NUM_VERTEX_DATA);
	Arr<Vec3> cs;
	Arr<obj_mtl> mtls(DEFAULT_NUM_MATERIALS);

	const Vec3 DEFAULT_COLOR((fp)1.0, (fp)1.0, (fp)1.0);

	obj_obj* curObj = 0;
	obj_mtl* curMtl = 0;

	while(1)
	{
		char* buf = com::ReadLine(f, false);

		if(!buf)
			break;

		tok[0] = strtok(buf, " \n");

		if(!tok[0])
			continue;

		for(int i = 1; i < sizeof(tok) / sizeof(char*); i++)
			tok[i] = strtok(0, " \n");

		if(tok[0][0] == 'v')
		{
			switch(tok[0][1])
			{
			case 't': // TEXCOORD
				ts.Ensure(numTs + 1);

				if(!ParseVec2Tokens(tok[1], tok[2], ts[numTs]))
					OBJ_LOAD_FAIL("Could not parse texture coordinate");

				numTs++;
				break;
			case 'n': // NORMAL
				ns.Ensure(numNs + 1);

				if(!ParseVec3Tokens(tok[1], tok[2], tok[3], ns[numNs]))
					OBJ_LOAD_FAIL("Could not parse normal");

				numNs++;
				break;
			default: // POSITION
				ps.Ensure(numPs + 1);

				if(!ParseVec3Tokens(tok[1], tok[2], tok[3], ps[numPs]))
					OBJ_LOAD_FAIL("Could not parse position");

				numPs++;

				// Try COLOR
				if(tok[4])
				{
					numCs = numPs + 1; // + 1 so index 0 is always reserved for default color
					cs.Ensure(numCs, DEFAULT_COLOR);

					if(!ParseVec3Tokens(tok[4], tok[5], tok[6], cs[numCs - 1]))
						OBJ_LOAD_FAIL("Could not parse color after position");
				}

				break;
			}
		}
		else if(tok[0][0] == 'f') // FACE
		{
			if(!curObj)
				curObj = NextObj(objs, numObjs);

			int mtlIndex = curMtl ? curMtl - mtls.o: -1;

			do
			{
				curObj->tris.Ensure(curObj->numTris + 1);

				if(const char* err = ParseFace(&tok[1], numPs, numTs, numNs, numCs, type,
				curObj->tris[curObj->numTris]))
					OBJ_LOAD_FAIL(err);

				curObj->tris[curObj->numTris].mtlIndex = mtlIndex;
				curObj->numTris++;

				tok[2] = tok[3];
				tok[3] = strtok(0, " \n");
			} while(tok[3]);
		}
		else if(tok[0][0] == 'o' || tok[0][0] == 'g') // OBJECT/GROUP
			curObj = NextObj(objs, numObjs);
		else if(mtls.o && !strcmp(tok[0], "usemtl")) // MATERIAL
			curMtl = ParseUseMaterial(tok[1], mtls, numMtls);
	}

	if(!numObjs)
		OBJ_LOAD_FAIL("No objects");

	fclose(f);

	// Optimize component arrays
	if(size_t maxNumComps = Max(numPs, Max(numTs, Max(numNs, numCs))))
	{
		size_t* redirects = new size_t[maxNumComps];

		OptimizeComponent<'p'>(ps.o, numPs, redirects, objs.o, numObjs);
		OptimizeComponent<'t'>(ts.o, numTs, redirects, objs.o, numObjs);
		OptimizeComponent<'n'>(ns.o, numNs, redirects, objs.o, numObjs);
		OptimizeComponent<'c'>(cs.o, numCs, redirects, objs.o, numObjs);

		delete[] redirects;
	}

	d.ps = ps.o;
	d.numPs = numPs;

	d.ts = ts.o;
	d.numTs = numTs;

	d.ns = ns.o;
	d.numNs = numNs;

	d.cs = cs.o;
	d.numCs = numCs;

	d.objs = objs.o;
	d.numObjs = numObjs;

	d.mtls = mtls.o;
	d.numMtls = numMtls;
	
	return 0;
}

/*--------------------------------------
	com::MTLLoad

Loads info about materials that are already defined in the given obj_file.
--------------------------------------*/
const char* com::MTLLoad(const char* filePath, obj_file& d)
{
	FILE* f = fopen(filePath, "r");

	if(!f)
		return "Could not open mtl file";

	obj_mtl* curMtl = 0;

	while(1)
	{
		size_t len;
		char* line = ReadLine(f, true, &len);

		if(!line)
			break;

		char* mapPath = line + 7;

		if(!strncmp(line, "newmtl", 6))
		{
			if(len < 8)
				return "No material name given";

			// Check that obj_mtl exists
			for(curMtl = d.mtls; curMtl < d.mtls + d.numMtls; curMtl++)
			{
				if(!strcmp(curMtl->name, mapPath))
					break;
			}

			if(curMtl == d.mtls + d.numMtls)
				curMtl = 0; // This mtl is not used; skip it
		}
		else
		{
			if(!curMtl)
				continue; // No current mtl, skip line

			if(!strncmp(line, "map_Kd", 6)) // COLOR MAP
			{
				if(len < 8)
					return "No color map file path given";

				if(curMtl->mapKD)
					return "Material has multiple color maps";

				curMtl->mapKD = new char[strlen(mapPath) + 1];
				strcpy(curMtl->mapKD, mapPath);
			}
			else if(!strncmp(line, "Ka", 2))
			{
				if(ParseNumberSequence(line + 2, 3, 1, curMtl->ka))
					return "Material's ambient value not in format \"Ka r g b\"";
			}
			else if(!strncmp(line, "Ke", 2))
			{
				if(ParseNumberSequence(line + 2, 3, 1, curMtl->ke))
					return "Material's emit value not in format \"Ke r g b\"";
			}
		}
	}

	fclose(f);
	return 0;
}

/*--------------------------------------
	com::OBJFree
--------------------------------------*/
void com::OBJFree(Vec3* ps, Vec2* ts, Vec3* ns, Vec3* cs, obj_obj* objs, size_t numObjs,
	obj_mtl* mtls, size_t numMtls)
{
	if(ps)
		ArrFree(ps);

	if(ts)
		ArrFree(ts);

	if(ns)
		ArrFree(ns);

	if(cs)
		ArrFree(cs);

	if(objs)
	{
		for(size_t i = 0; i < numObjs; i++)
		{
			if(objs[i].tris.o)
				objs[i].tris.Free();
		}

		ArrFree(objs);
	}

	if(mtls)
	{
		for(size_t i = 0; i < numMtls; i++)
		{
			if(mtls[i].name)
				delete[] mtls[i].name;

			if(mtls[i].mapKD)
				delete[] mtls[i].mapKD;
		}

		ArrFree(mtls);
	}
}

void com::OBJFree(obj_file& d)
{
	OBJFree(d.ps, d.ts, d.ns, d.cs, d.objs, d.numObjs, d.mtls, d.numMtls);
}

/*--------------------------------------
	com::OBJClose

Always returns err.
--------------------------------------*/
const char* com::OBJClose(FILE* f, Arr<com::Vec3>& ps, Arr<com::Vec2>& ts, Arr<com::Vec3>& ns,
	Arr<Vec3>& cs, Arr<obj_obj>& objs, size_t numObjs, Arr<obj_mtl>& mtls, size_t numMtls,
	const char* err)
{
	if(f)
		fclose(f);

	OBJFree(ps.o, ts.o, ns.o, cs.o, objs.o, numObjs, mtls.o, numMtls);

	return err;
}

/*--------------------------------------
	com::NextObj
--------------------------------------*/
com::obj_obj* com::NextObj(Arr<obj_obj>& objsInOut, size_t& numObjsInOut)
{
	objsInOut.Ensure(numObjsInOut + 1);

	obj_obj* next = objsInOut.o + (numObjsInOut++);
	next->tris.Init(DEFAULT_NUM_TRIANGLES);
	next->numTris = 0;

	return next;
}

/*--------------------------------------
	com::ParseVec2Tokens
--------------------------------------*/
bool com::ParseVec2Tokens(const char* u, const char* v, Vec2& vecOut)
{
	if(!u || !v)
		return false;

	errno = 0;
	vecOut.x = COM_STR_TO_FP(u, 0);
	vecOut.y = COM_STR_TO_FP(v, 0);

	return !errno;
}

/*--------------------------------------
	com::ParseVec3Tokens
--------------------------------------*/
bool com::ParseVec3Tokens(const char* x, const char* y, const char* z, Vec3& vecOut)
{
	if(!x || !y || !z)
		return false;
	
	errno = 0;
	vecOut.x = COM_STR_TO_FP(x, 0);
	vecOut.y = COM_STR_TO_FP(y, 0);
	vecOut.z = COM_STR_TO_FP(z, 0);

	return !errno;
}

/*--------------------------------------
	com::ParseFace

Creates a face from 3 tokens in format specified by ParseVertex. Returns error string on
failure.
--------------------------------------*/
const char* com::ParseFace(char** tok, size_t numPs, size_t numTs, size_t numNs, size_t numCs,
	unsigned type, obj_tri& triOut)
{
	if(!tok[0] || !tok[1] || !tok[2])
		return "Face has less than 3 vertices";

	for(int i = 0; i < 3; i++)
	{
		if(const char* err = ParseVertex(tok[i], numPs, numTs, numNs, type, triOut.p[i],
		triOut.t[i], triOut.n[i]))
			return err;

		// Set color index based on position index
		size_t c = triOut.p[i] + 1;

		if(c >= numCs)
			c = 0; // Set to default

		triOut.c[i] = c;
	}

	return 0;
}

/*--------------------------------------
	com::ParseVertex

Takes str with format "p", "p/t", "p/t/n", or "p//n" and retrieves indices for the components
specified by type.

Returns error string if an index required by the type could not be retrieved or the index is
invalid.

Output indices are set to 0 by default.
--------------------------------------*/
const char* com::ParseVertex(const char* str, size_t numPs, size_t numTs, size_t numNs,
	unsigned type, size_t& piOut, size_t& tiOut, size_t& niOut)
{
	char* end;
	long pi, ti, ni;
	piOut = tiOut = niOut = 0;

	errno = 0;

	pi = strtol(str, &end, 10);

	if(str == end)
		return "Vertex has no position";

	if(*end == '/')
		str = end + 1;

	ti = strtol(str, &end, 10);

	if(str == end && (type == COM_OBJ_PT || type == COM_OBJ_PTN))
		return "Vertex has no texture coordinate";

	if(*end == '/')
		str = end + 1;

	ni = strtol(str, &end, 10);

	if(str == end && type >= COM_OBJ_PN)
		return "Vertex has no normal";

	if(errno == ERANGE)
		return "Component indices out of int range";

	const char* err;

	if((err = ConvertIndex(pi, numPs, piOut)) ||
	(err = ConvertIndex(ti, numTs, tiOut)) ||
	(err = ConvertIndex(ni, numNs, niOut)))
		return err;

	return 0;
}

/*--------------------------------------
	com::ConvertIndex

Converts the one-based, possibly relative (negative) index to a zero-based index. iOut is the
converted index if conversion succeeded. Returns error string if the converted index is out of
bounds. iOut is set to 0 if the given index is 0, even if num is 0.
--------------------------------------*/
const char* com::ConvertIndex(long i, size_t num, size_t& iOut)
{
	if(i == 0)
	{
		iOut = 0;
		return 0;
	}

	if(i > 0)
	{
		if((size_t)i > num)
			return "Index out of bounds";

		iOut = size_t(i - 1);

		return 0;
	}

	i = -i;

	if((size_t)i > num)
		return "Relative index out of bounds";

	iOut = num - (size_t)i;

	return 0;
}

/*--------------------------------------
	com::ParseUseMaterial
--------------------------------------*/
com::obj_mtl* com::ParseUseMaterial(const char* name, Arr<obj_mtl>& mtlsInOut,
	size_t& numMtlsInOut)
{
	for(size_t i = 0; i < numMtlsInOut; i++)
	{
		if(!strcmp(name, mtlsInOut[i].name))
			return &mtlsInOut[i];
	}

	mtlsInOut.Ensure(numMtlsInOut + 1);

	obj_mtl* m = mtlsInOut.o + (numMtlsInOut++);

	m->name = new char[strlen(name) + 1];
	strcpy(m->name, name);

	m->mapKD = 0;

	for(size_t i = 0; i < 3; i++)
		m->ka[i] = m->ke[i] = (fp)0.0;

	return m;
}

/*--------------------------------------
	com::ParseNumberSequence

Fills f with numbers from whitespace separated str. Returns error if there are less than numReq
numbers. Any unfilled elements beyond numReq are set to the last parsed value.
--------------------------------------*/
const char* com::ParseNumberSequence(const char* str, size_t num, size_t numReq, fp* f)
{
	const char *start = str, *end = str;

	for(size_t i = 0; i < num; i++)
	{
		f[i] = COM_STR_TO_FP(str, &end);

		if(start == end)
		{
			// FP parse failed
			if(i < numReq)
				return "Not enough numbers in sequence";

			size_t last = i ? i - 1 : 0;

			for(size_t j = i; j < num; j++)
				f[j] = f[last];

			break;
		}

		start = end;
	}

	return 0;
}

/*--------------------------------------
	com::OptimizeComponent

Removes duplicates from compsInOut and updates triangle indices for component indicated by the
comp char ('p', 't', 'n', or 'c'). redirectsInOut must be a size_t array with numCompsInOut
elements. Returns immediately if numCompsInOut is 0.
--------------------------------------*/
template<char comp, typename comp_type> void com::OptimizeComponent(comp_type* compsInOut,
	size_t& numCompsInOut, size_t* redirectsInOut, obj_obj* objsInOut, size_t numObjs)
{
	if(!numCompsInOut)
		return;

	size_t numOrig = Originals(compsInOut, numCompsInOut, redirectsInOut);
	RemoveDuplicates(compsInOut, numCompsInOut, redirectsInOut, numOrig);
	numCompsInOut = numOrig;

	for(size_t i = 0; i < numObjs; i++)
	{
		for(size_t j = 0; j < objsInOut[i].numTris; j++)
		{
			obj_tri& tri = objsInOut[i].tris[j];

			for(size_t k = 0; k < 3; k++)
			{
				switch(comp)
				{
				case 'p':
					tri.p[k] = redirectsInOut[tri.p[k]]; break;
				case 't':
					tri.t[k] = redirectsInOut[tri.t[k]]; break;
				case 'n':
					tri.n[k] = redirectsInOut[tri.n[k]]; break;
				case 'c':
					tri.c[k] = redirectsInOut[tri.c[k]]; break;
				}
			}
		}
	}
}

/*
################################################################################################


	SAVE OBJ


################################################################################################
*/

static FILE*	saveFile = 0;
static size_t	saveNumVerts = 0;
static size_t	saveNumTexVerts = 0;
static size_t	saveFirstVert = 0;
static size_t	saveFirstTexVert = 0;
static unsigned	saveNumObjs = 0;

/*--------------------------------------
	com::OBJSaveBegin
--------------------------------------*/
bool com::OBJSaveBegin(const char* fileName)
{
	if(saveFile)
		return false; // .OBJ already being written

	saveFile = fopen(fileName, "w");

	if(!saveFile)
		return false;

	return true;
}

/*--------------------------------------
	com::OBJSaveObjBegin
--------------------------------------*/
bool com::OBJSaveObjBegin()
{
	if(!saveFile)
		return false;

	fprintf(saveFile, "o obj%u\n", saveNumObjs);
	saveNumObjs++;
	return true;
}

/*--------------------------------------
	com::OBJSavePolyBegin
--------------------------------------*/
bool com::OBJSavePolyBegin()
{
	if(!saveFile)
		return false;

	if(saveFirstVert)
		return false; // Polygon already being written

	saveFirstVert = saveNumVerts + 1;
	saveFirstTexVert = saveNumTexVerts + 1;
	return true;
}

/*--------------------------------------
	com::OBJSaveVert
--------------------------------------*/
bool com::OBJSaveVert(const Vec3& v)
{
	if(!saveFile)
		return false;

	if(!saveNumObjs && !OBJSaveObjBegin())
		return false;

	saveNumVerts++;
	fprintf(saveFile, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
	return true;
}

/*--------------------------------------
	com::OBJSaveTexVert
--------------------------------------*/
bool com::OBJSaveTexVert(const Vec3& v, const Vec2& u)
{
	if(!saveFile)
		return false;

	if(!saveNumObjs && !OBJSaveObjBegin())
		return false;

	saveNumVerts++;
	saveNumTexVerts++;
	fprintf(saveFile, "v %.6f %.6f %.6f\nvt %.6f %.6f\n", v.x, v.y, v.z, u.x, u.y);
	return true;
}

/*--------------------------------------
	com::OBJSavePolyEnd
--------------------------------------*/
bool com::OBJSavePolyEnd()
{
	if(!saveFile)
		return false;

	if(saveNumVerts < saveFirstVert || saveNumVerts - saveFirstVert < 1)
	{
		// Not enough verts for a polygon or line
		saveFirstVert = 0;
		saveFirstTexVert = 0;
		return false;
	}

	// Use tex coords if the number of vt's equals the number of v's
	bool useTexCoords = saveNumTexVerts >= saveFirstTexVert &&
		saveNumTexVerts - saveFirstTexVert == saveNumVerts - saveFirstVert;

	fprintf(saveFile, "%s", "f ");

	if(useTexCoords)
	{
		for(size_t v = saveFirstVert, t = saveFirstTexVert; v <= saveNumVerts; v++, t++)
			fprintf(saveFile, "%u/%u/ ", (unsigned)v, (unsigned)t);
	}
	else
	{
		for(size_t i = saveFirstVert; i <= saveNumVerts; i++)
			fprintf(saveFile, "%u// ", (unsigned)i);
	}

	fputc('\n', saveFile);
	saveFirstVert = 0;
	saveFirstTexVert = 0;
	return true;
}

/*--------------------------------------
	com::OBJSavePoly
--------------------------------------*/
bool com::OBJSavePoly(const Vec3* verts, size_t numVerts)
{
	if(!OBJSavePolyBegin() || !verts)
		return false;

	for(size_t i = 0; i < numVerts; i++)
		OBJSaveVert(verts[i]);

	if(!OBJSavePolyEnd())
		return false;

	return true;
}

/*--------------------------------------
	com::OBJSaveEnd
--------------------------------------*/
bool com::OBJSaveEnd()
{
	if(!saveFile)
		return false;

	fclose(saveFile);
	saveFile = 0;
	saveNumVerts = 0;
	saveNumTexVerts = 0;
	saveFirstVert = 0;
	saveFirstTexVert = 0;
	saveNumObjs = 0;

	return true;
}