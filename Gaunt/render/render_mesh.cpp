// render_mesh.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"
#include "../console/console.h"
#include "../../GauntCommon/math.h"
#include "../../GauntCommon/obj.h" // FIXME TEMP
#include "../../GauntCommon/type.h"
#include "../mod/mod.h"
#include "../quaternion/qua_lua.h"

#if RND_CALC_MPT
#include "../../GauntCommon/cache.h"
#endif

namespace rnd
{
	// MESH
	MeshGL*		CreateMesh(const char* fileName);

	template <class T>
	void		TieExtrasToMesh(T* extras, uint32_t num, Mesh& mesh);
	template <class T>
	T*			FindExtra(T* extras, uint32_t num, const char* name);
	float		MissesPerTriangle(const void* indices, size_t numIndices, GLenum indexType,
				size_t numTris);
	template <typename vertex_place>
	void		BoundingBox(const vertex_place* vp, uint32_t numVerts, com::Vec3& boxMinOut,
				com::Vec3& boxMaxOut, float& radiusOut);

	// MESH FILE
	enum
	{
		FILE_FLAG_VOXELS = 1
	};

	const char*	LoadMeshFile(const char* filePath, void*& vpOut, vertex_mesh_tex*& vtOut,
				void*& indicesOut, GLenum& indexTypeOut, uint32_t& numFramesOut,
				uint32_t& numFrameTrisOut, uint32_t& numFrameVertsOut, uint32_t& frameRateOut,
				Socket*& socketsOut, uint32_t& numSocketsOut, Animation*& animationsOut,
				uint32_t& numAnimationsOut, GLfloat*& voxelsOut, GLfloat* voxelScaleOut,
				int32_t* voxelMinOut, uint32_t* voxelDimsOut);
	void		FreeMesh(void* vp, vertex_mesh_tex* vt, void* indices, Socket* sockets,
				Animation* animations, GLfloat* voxels);
	const char*	CloseMesh(FILE* file, void* vp, vertex_mesh_tex* vt, void* indices,
				Socket* sockets, Animation* animations, GLfloat* voxels, unsigned char* bytes,
				const char* err);
	void		MergeFloat(const unsigned char* curByte, GLfloat& fOut);

	template <class T>
	T*			SetExtraName(T* extsIO, uint32_t index, const unsigned char* nameByte,
				uint32_t maxNameSize, const char*& errOut);

	template <typename vertex_place>
	void		MergeVertexPlaces(uint32_t numFrames, uint32_t numFrameVerts,
				vertex_place* vpOut, unsigned char*& curByteIO);
}

/*
################################################################################################


	MESH


################################################################################################
*/

const char* const rnd::Mesh::METATABLE = "metaMesh";

/*--------------------------------------
	rnd::Mesh::Mesh
--------------------------------------*/
rnd::Mesh::Mesh(const char* fileName, uint16_t flags, uint32_t numFrames, uint32_t frameRate,
	Socket* sockets, uint32_t numSockets, Animation* animations, uint32_t numAnimations,
	const com::Vec3& boxMin, const com::Vec3& boxMax, float radius)
	: fileName(com::NewStringCopy(fileName)),
	flags(flags),
	numFrames(numFrames),
	frameRate(frameRate),
	numSockets(numSockets),
	numAnimations(numAnimations),
	sockets(sockets),
	animations(animations),
	bounds(boxMin, boxMax),
	radius(radius)
{
	EnsureLink();
	TieExtrasToMesh(sockets, numSockets, *this);
	TieExtrasToMesh(animations, numAnimations, *this);
}

/*--------------------------------------
	rnd::Mesh::~Mesh
--------------------------------------*/
rnd::Mesh::~Mesh()
{
	if(fileName)
		delete[] fileName;

	FreeMesh(0, 0, 0, sockets, animations, 0);
}

/*--------------------------------------
	rnd::Mesh::FindSocket
--------------------------------------*/
const rnd::Socket* rnd::Mesh::FindSocket(const char* name) const
{
	return FindExtra(sockets, numSockets, name);
}

const rnd::Socket* rnd::Mesh::FindSocket(uint32_t i) const
{
	if(i < numSockets)
		return &sockets[i];

	return 0;
}

/*--------------------------------------
	rnd::Mesh::FindAnimation
--------------------------------------*/
const rnd::Animation* rnd::Mesh::FindAnimation(const char* name) const
{
	return FindExtra(animations, numAnimations, name);
}

const rnd::Animation* rnd::Mesh::FindAnimation(uint32_t i) const
{
	if(i < numAnimations)
		return &animations[i];

	return 0;
}

/*--------------------------------------
	rnd::MeshGL::MeshGL
--------------------------------------*/
rnd::MeshGL::MeshGL(const char* fileName, uint32_t numFrames, uint32_t frameRate,
	Socket* sockets, uint32_t numSockets, Animation* animations, uint32_t numAnimations,
	const com::Vec3& boxMin, const com::Vec3& boxMax, float radius, GLint numFrameVerts,
	GLsizei numFrameIndices, GLenum indexType, const void* vp, const vertex_mesh_tex* vt,
	const void* indices, const GLfloat* voxels, GLfloat voxelScale, const int32_t* voxelMin,
	const uint32_t* voxelDims)
	: Mesh(fileName, 0, numFrames, frameRate, sockets, numSockets, animations, numAnimations,
	boxMin, boxMax, radius),
	numFrameVerts(numFrameVerts),
	numFrameIndices(numFrameIndices),
	indexType(indexType),
	voxelScale(voxelScale),
	texVoxels(0)
{
	// Send vertices to gl
	glGenBuffers(1, &vBufName);
	GLsizeiptr numVerts = (GLsizeiptr)numFrameVerts * numFrames;
	texDataSize = sizeof(vertex_mesh_tex) * numFrameVerts;
	size_t placeDataSize = sizeof(vertex_mesh_place) * numVerts;
	glBindBuffer(GL_ARRAY_BUFFER, vBufName);
	glBufferData(GL_ARRAY_BUFFER, texDataSize + placeDataSize, 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, texDataSize, vt);
	glBufferSubData(GL_ARRAY_BUFFER, texDataSize, placeDataSize, vp);

	// Send indices to gl
	glGenBuffers(1, &iBufName);
	GLsizeiptr numIndices;
	GLsizeiptr indexSize;

	/* Only upload indices for one frame; frame vertex offset is set in glVertexAttribPointer,
	not in glDrawElements */
	numIndices = numFrameIndices;

	if(indexType == GL_UNSIGNED_BYTE)
		indexSize = sizeof(GLubyte);
	else if(indexType == GL_UNSIGNED_SHORT)
		indexSize = sizeof(GLushort);
	else
		indexSize = sizeof(GLuint);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBufName);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, indices, GL_STATIC_DRAW);

	if(voxels)
	{
		flags |= VOXELS;

		// Send voxels to gl
		glGenTextures(1, &texVoxels);
		glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_3D, texVoxels);

		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glTexImage3D(GL_TEXTURE_3D, 0, GL_LUMINANCE8, voxelDims[0], voxelDims[1], voxelDims[2],
			0, GL_RED, GL_FLOAT, voxels);

		glBindTexture(GL_TEXTURE_3D, 0);

		// Calc/copy meta data
		for(size_t i = 0; i < 3; i++)
			voxelOrigin[i] = (GLfloat)voxelMin[i];

		for(size_t i = 0; i < 3; i++)
			voxelInvDims[i] = 1.0f / voxelDims[i];
	}
	else
	{
		for(size_t i = 0; i < 3; i++)
		{
			voxelOrigin[i] = 0.0f;
			voxelInvDims[i] = 0.0f;
		}
	}
}

/*--------------------------------------
	rnd::MeshGL::~MeshGL
--------------------------------------*/
rnd::MeshGL::~MeshGL()
{
	glDeleteBuffers(1, &vBufName);
	glDeleteBuffers(1, &iBufName);

	if(texVoxels)
		glDeleteTextures(1, &texVoxels);
}

/*--------------------------------------
	rnd::FindMesh
--------------------------------------*/
rnd::Mesh* rnd::FindMesh(const char* fileName)
{
	for(com::linker<Mesh>* it = Mesh::List().f; it; it = it->next)
	{
		if(!strcmp(it->o->FileName(), fileName))
			return it->o;
	}

	return 0;
}

/*--------------------------------------
	rnd::EnsureMesh

Either finds or loads the mesh with the given file name.
--------------------------------------*/
rnd::Mesh* rnd::EnsureMesh(const char* fileName)
{
	Mesh* m;
	return (m = FindMesh(fileName)) ? m : CreateMesh(fileName);
}

/*--------------------------------------
	rnd::CreateMesh
--------------------------------------*/
rnd::MeshGL* rnd::CreateMesh(const char* fileName)
{
	void* vp;
	vertex_mesh_tex* vt;
	void* indices;
	GLenum indexType;
	uint32_t numFrames, numFrameTris, numFrameVerts, frameRate, numSockets, numAnimations;
	Socket* sockets;
	Animation* animations;
	GLfloat* voxels;
	GLfloat voxelScale;
	int32_t voxelMin[3];
	uint32_t voxelDims[3];
	const char *err = 0, *path = mod::Path("meshes/", fileName, err);

	if(err || (err = LoadMeshFile(path, vp, vt, indices, indexType, numFrames,
	numFrameTris, numFrameVerts, frameRate, sockets, numSockets, animations, numAnimations,
	voxels, &voxelScale, voxelMin, voxelDims)))
	{
		con::LogF("Failed to load mesh '%s' (%s)", fileName, err);
		return 0;
	}

	GLsizei numFrameIndices = (GLsizei)numFrameTris * 3;
	uint32_t numVerts = numFrameVerts * numFrames;
	com::Vec3 boxMin, boxMax;
	float radius;
	BoundingBox((vertex_mesh_place*)vp, numVerts, boxMin, boxMax, radius);

	MeshGL* msh = new MeshGL(fileName, numFrames, frameRate, sockets, numSockets, animations,
		numAnimations, boxMin, boxMax, radius, numFrameVerts, numFrameIndices, indexType, vp,
		vt, indices, voxels, voxelScale, voxelMin, voxelDims);

#if RND_CALC_MPT
	con::LogF("%s MPT: " COM_FLT_PRNT, fileName, MissesPerTriangle(indices, numFrameIndices,
		indexType, numFrameTris));
#endif

	FreeMesh(vp, vt, indices, 0, 0, voxels);
	return msh;
}

/*--------------------------------------
	rnd::TieExtrasToMesh
--------------------------------------*/
template <class T> void rnd::TieExtrasToMesh(T* extras, uint32_t num, Mesh& mesh)
{
	for(uint32_t i = 0; i < num; i++)
	{
		extras[i].mesh.Set(&mesh);
		extras[i].AddLock(); // Permanent
	}
}

/*--------------------------------------
	rnd::FindExtra
--------------------------------------*/
template <class T> T* rnd::FindExtra(T* extras, uint32_t num, const char* name)
{
	if(!num)
		return 0;

	uint32_t start = 0, end = num;

	while(1)
	{
		uint32_t mid = start + (end - start) / 2;
		int cmp = strcmp(name, extras[mid].Name());

		if(cmp > 0)
		{
			start = mid + 1;

			if(start == end)
				return 0;
		}
		else if(cmp < 0)
		{
			end = mid;

			if(start == end)
				return 0;
		}
		else
			return &extras[mid];
	}

	return 0;
}

#if RND_CALC_MPT
/*--------------------------------------
	rnd::MissesPerTriangle
--------------------------------------*/
float rnd::MissesPerTriangle(const void* indices, size_t numIndices, GLenum indexType,
	size_t numTris)
{
	size_t numMisses = 0;

	if(indexType == GL_UNSIGNED_BYTE)
		numMisses = com::NumMissesFIFO<1, 32>((GLubyte*)indices, numIndices);
	else if(indexType == GL_UNSIGNED_SHORT)
		numMisses = com::NumMissesFIFO<1, 32>((GLushort*)indices, numIndices);
	else
		numMisses = com::NumMissesFIFO<1, 32>((GLuint*)indices, numIndices);

	return (float)numMisses / numTris;
}
#endif

/*--------------------------------------
	rnd::BoundingBox
--------------------------------------*/
template <typename vertex_place>
void rnd::BoundingBox(const vertex_place* vp, uint32_t numVerts, com::Vec3& boxMinOut,
	com::Vec3& boxMaxOut, float& radiusOut)
{
	com::VertBox(vp, numVerts, boxMinOut, boxMaxOut);
	radiusOut = 0.0f;

	for(uint32_t i = 0; i < numVerts; i++)
	{
		float mag = 0.0f;
		
		for(size_t j = 0; j < 3; j++)
			mag += vp[i].pos[j] * vp[i].pos[j];

		if(mag > radiusOut)
			radiusOut = mag;
	}

	radiusOut = sqrt(radiusOut);
}

/*--------------------------------------
	rnd::CreateSimpleMesh
--------------------------------------*/
rnd::simple_mesh* rnd::CreateSimpleMesh(const char* filePath)
{
	void* tempVP;
	vertex_mesh_tex* tempVT;
	void* indices;
	GLenum indexType;
	uint32_t numFrames, numFrameTris, numFrameVerts, frameRate, numSockets, numAnimations;
	Socket* sockets;
	Animation* animations;
	GLfloat* voxels;
	const char *err = 0;

	if(err || (err = LoadMeshFile(filePath, tempVP, tempVT, indices, indexType, numFrames,
	numFrameTris, numFrameVerts, frameRate, sockets, numSockets, animations, numAnimations,
	voxels, 0, 0, 0)))
	{
		con::LogF("Failed to load simple mesh '%s' (%s)", filePath, err);
		return 0;
	}

	GLsizei numFrameIndices = (GLsizei)numFrameTris * 3;
	simple_mesh* smsh = new simple_mesh;
	smsh->numVerts = numFrameVerts;
	smsh->numIndices = numFrameIndices;
	smsh->indexType = indexType;

	// Upload one frame of simplified vertices
	vertex_mesh_place* tempVMP = (vertex_mesh_place*)tempVP;
	vertex_simple* sVerts = new vertex_simple[numFrameVerts];

	for(uint32_t i = 0; i < numFrameVerts; i++)
		com::Copy(sVerts[i].pos, tempVMP[i].pos, 3);

	glGenBuffers(1, &smsh->vBufName);
	glBindBuffer(GL_ARRAY_BUFFER, smsh->vBufName);
	glBufferData(GL_ARRAY_BUFFER, numFrameVerts * sizeof(vertex_simple), sVerts,
		GL_STATIC_DRAW);
	delete[] sVerts;

	// Send indices to gl
	glGenBuffers(1, &smsh->iBufName);
	GLsizeiptr indexSize;

	if(indexType == GL_UNSIGNED_BYTE)
		indexSize = sizeof(GLubyte);
	else if(indexType == GL_UNSIGNED_SHORT)
		indexSize = sizeof(GLushort);
	else
		indexSize = sizeof(GLuint);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, smsh->iBufName);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, smsh->numIndices * indexSize, indices,
		GL_STATIC_DRAW);

#if RND_CALC_MPT
	con::LogF("%s MPT: " COM_FLT_PRNT, filePath, MissesPerTriangle(indices, numFrameIndices,
		indexType, numFrameTris));
#endif

	FreeMesh(tempVP, tempVT, indices, sockets, animations, voxels);
	return smsh;
}

/*
################################################################################################


	MESH LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::FindMesh

IN	sFileName
OUT	mshM
--------------------------------------*/
int rnd::FindMesh(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);

	if(Mesh* m = FindMesh(fileName))
	{
		m->LuaPush();
		return 1;
	}
	else
		return 0;
}

/*--------------------------------------
LUA	rnd::EnsureMesh

IN	sFileName
OUT	msh
--------------------------------------*/
int rnd::EnsureMesh(lua_State* l)
{
	const char* fileName = luaL_checkstring(l, 1);
	Mesh* m = EnsureMesh(fileName);

	if(!m)
		luaL_error(l, "mesh load failure");

	m->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	rnd::MshFileName (FileName)

IN	mshM
OUT	sFileName
--------------------------------------*/
int rnd::MshFileName(lua_State* l)
{
	Mesh* m = Mesh::CheckLuaTo(1);
	lua_pushstring(l, m->FileName());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshFlags (Flags)

IN	mshM
OUT	iFlags
--------------------------------------*/
int rnd::MshFlags(lua_State* l)
{
	lua_pushinteger(l, Mesh::CheckLuaTo(1)->Flags());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshNumFrames (NumFrames)

IN	mshM
OUT	iNumFrames
--------------------------------------*/
int rnd::MshNumFrames(lua_State* l)
{
	Mesh* m = Mesh::CheckLuaTo(1);
	lua_pushinteger(l, m->NumFrames());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshFrameRate (FrameRate)

IN	mshM
OUT	iRate
--------------------------------------*/
int rnd::MshFrameRate(lua_State* l)
{
	Mesh* m = Mesh::CheckLuaTo(1);
	lua_pushinteger(l, m->FrameRate());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshSocket (Socket)

IN	mshM, [sName | iIndex]
OUT	socS
--------------------------------------*/
int rnd::MshSocket(lua_State* l)
{
	Mesh* m = Mesh::CheckLuaTo(1);
	const Socket* s;

	if(lua_type(l, 2) == LUA_TSTRING)
		s = m->FindSocket(lua_tostring(l, 2));
	else
		s = m->FindSocket(luaL_checkinteger(l, 2));

	if(s)
	{
		s->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	rnd::MshNumSockets (NumSockets)

IN	mshM
OUT	iNum
--------------------------------------*/
int rnd::MshNumSockets(lua_State* l)
{
	lua_pushinteger(l, Mesh::CheckLuaTo(1)->NumSockets());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshAnimation (Animation)

IN	mshM, [sName | iIndex]
OUT	aniA
--------------------------------------*/
int rnd::MshAnimation(lua_State* l)
{
	Mesh* m = Mesh::CheckLuaTo(1);
	const Animation* a;

	if(lua_type(l, 2) == LUA_TSTRING)
		a = m->FindAnimation(lua_tostring(l, 2));
	else
		a = m->FindAnimation(luaL_checkinteger(l, 2));

	if(a)
	{
		a->LuaPush();
		return 1;
	}

	return 0;
}

/*--------------------------------------
LUA	rnd::MshNumAnimations (NumAnimations)

IN	mshM
OUT	iNum
--------------------------------------*/
int rnd::MshNumAnimations(lua_State* l)
{
	lua_pushinteger(l, Mesh::CheckLuaTo(1)->NumAnimations());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshBounds (Bounds)

IN	mshM
OUT	v3Min, v3Max
--------------------------------------*/
int rnd::MshBounds(lua_State* l)
{
	const Mesh* msh = Mesh::CheckLuaTo(1);
	vec::LuaPushVec(l, msh->Bounds().Min());
	vec::LuaPushVec(l, msh->Bounds().Max());
	return 6;
}

/*--------------------------------------
LUA	rnd::MshRadius (Radius)

IN	mshM
OUT	nRadius
--------------------------------------*/
int rnd::MshRadius(lua_State* l)
{
	lua_pushnumber(l, Mesh::CheckLuaTo(1)->Radius());
	return 1;
}

/*--------------------------------------
LUA	rnd::MshDelete (__gc)

IN	mshM
--------------------------------------*/
int rnd::MshDelete(lua_State* l)
{
	MeshGL* m = (MeshGL*)Mesh::UserdataLuaTo(1);

	if(!m)
		return 0;

	if(m->Locked())
		luaL_error(l, "Tried to delete locked mesh");

	delete m;
	return 0;
}

/*
################################################################################################


	MESH FILE


################################################################################################
*/

/*--------------------------------------
	rnd::LoadMeshFile

Loads filePath following the mesh format. On success, the out arguments are set and 0 is
returned. Otherwise, an error string is returned. All allocations are done with new.

vpOut has numFrameVertsOut * numFramesOut vertices. vtOut has numFrameVertsOut vertices.

indicesOut has numFrameTrisOut * 3 indices. Offset the indices by numFrameTrisOut * 3 to draw
the next frame.

indexTypeOut is set to GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT, indicating the
type of indicesOut. The selected type is the smallest which can hold the value
numFrameVertsOut - 1.

// Header
char SIG[4] = {0x69, 0x91, 'M', 's'}
le uint32_t version = 0
le uint32_t flags
	1 = VOXELS

// Meta data; loader can calculate remaining number of bytes from this info
le uint32_t numFrames
le uint32_t numFrameTris
le uint32_t numFrameVerts
le uint32_t frameRate
le uint32_t numSockets
le uint32_t numAnimations
le uint32_t nameSize // Including null terminator

if flags & VOXELS
	le float voxelScale
	le int32_t voxelMin[3]
	le int32_t voxelDims[3] // Elements must be > 0

// Data
triangles[numFrameTris]
	le uint32_t vertIndices[3]

texCoords[numFrameVerts]
	le float u, v

frames[numFrames]
	positions[numFrameVerts]
		le float x, y, z
	normals[numFrameVerts]
		le float x, y, z

sockets[numSockets] (sorted by name, ascending)
	char name[nameSize]
	placements[numFrames]
		le float x, y, z, qx, qy, qz, qw

animations[numAnimations] (sorted by name, ascending)
	char name[nameSize]
	le uint32_t start, end

if flags & VOXELS
	le float alpha[voxelDims[0] * voxelDims[1] * voxelDims[2]]

// Triangle vertex order is counter-clockwise from the front
--------------------------------------*/
#define LOAD_MESH_FILE_FAIL(err) \
	return CloseMesh(file, vp, vt, indices, sockets, animations, voxels, bytes, err)

const char* rnd::LoadMeshFile(const char* filePath, void*& vpOut, vertex_mesh_tex*& vtOut,
	void*& indicesOut, GLenum& indexTypeOut, uint32_t& numFramesOut, uint32_t& numFrameTrisOut,
	uint32_t& numFrameVertsOut, uint32_t& frameRateOut, Socket*& socketsOut,
	uint32_t& numSocketsOut, Animation*& animationsOut, uint32_t& numAnimationsOut,
	GLfloat*& voxelsOut, GLfloat* voxelScaleOut, int32_t* voxelMinOut, uint32_t* voxelDimsOut)
{
	void* vp = 0;
	vertex_mesh_tex* vt = 0;
	uint32_t* indices = 0;
	Socket* sockets = 0;
	Animation* animations = 0;
	GLfloat* voxels = 0;
	unsigned char* bytes = 0;

	FILE* file = fopen(filePath, "rb");

	if(!file)
		LOAD_MESH_FILE_FAIL("Could not open file");

	unsigned char temp[32];

	// Header
	const size_t HEADER_SIZE = 8; // FIXME: include flags in header bytes

	if(fread(temp, sizeof(unsigned char), HEADER_SIZE, file) != HEADER_SIZE)
		LOAD_MESH_FILE_FAIL("Could not read header");

	if(strncmp((char*)temp, "\x69\x91Ms", 4))
		LOAD_MESH_FILE_FAIL("Incorrect signature");

	uint32_t version;
	com::MergeLE(temp + 4, version);

	//if(version != 0)
	if(version > 1) // FIXME TEMP: reprocess all meshes with new format but reset version to 0
		LOAD_MESH_FILE_FAIL("Unknown mesh file version");

	uint32_t flags = 0;

	if(version >= 1) // FIXME TEMP
	{
		if(com::ReadLE(flags, file) != sizeof(flags))
			LOAD_MESH_FILE_FAIL("Could not read flags");
	}

	// Base meta data
	const size_t BASE_META_SIZE = 28;

	if(fread(temp, sizeof(unsigned char), BASE_META_SIZE, file) != BASE_META_SIZE)
		LOAD_MESH_FILE_FAIL("Could not read meta data");

	uint32_t numFrames, numFrameTris, numFrameVerts, frameRate, numSockets, numAnimations,
		nameSize;

	com::MergeLE(temp, numFrames);
	com::MergeLE(temp + 4, numFrameTris);
	com::MergeLE(temp + 8, numFrameVerts);
	com::MergeLE(temp + 12, frameRate);
	com::MergeLE(temp + 16, numSockets);
	com::MergeLE(temp + 20, numAnimations);
	com::MergeLE(temp + 24, nameSize);

	if(!numFrames)
		LOAD_MESH_FILE_FAIL("numFrames is 0");

	if(!numFrameTris)
		LOAD_MESH_FILE_FAIL("numFrameTris is 0");

	if(!numFrameVerts)
		LOAD_MESH_FILE_FAIL("numFrameVerts is 0");

	if(!nameSize && (numSockets || numAnimations))
		LOAD_MESH_FILE_FAIL("nameSize is 0 but mesh contains sockets/animations");

	size_t numVerts = numFrameVerts * numFrames;
	size_t numFrameIndices = numFrameTris * 3;

	// Voxel meta data
	float voxelScale = 0.0f;
	int32_t voxelFileMin[3] = {0, 0, 0}, voxelFileDims[3] = {0, 0, 0};
	size_t numFileVoxels = 0;
	int32_t voxelMin[3] = {0, 0, 0};
	uint32_t voxelDims[3] = {0, 0, 0};
	size_t numVoxels = 0;

	if(flags & FILE_FLAG_VOXELS)
	{
		const size_t VOXEL_META_SIZE = 28;

		if(fread(temp, sizeof(unsigned char), VOXEL_META_SIZE, file) != VOXEL_META_SIZE)
			LOAD_MESH_FILE_FAIL("Could not read voxel meta data");

		com::MergeLE(temp, voxelScale);
		com::MergeLE(temp + 4, voxelFileMin, 3);
		com::MergeLE(temp + 16, voxelFileDims, 3);

		if(voxelScale <= 0.0f)
			LOAD_MESH_FILE_FAIL("voxelScale is <= 0");

		if(voxelFileDims[0] <= 0 || voxelFileDims[1] <= 0 || voxelFileDims[2] <= 0)
			LOAD_MESH_FILE_FAIL("voxelDims component is <= 0");

		numFileVoxels = voxelFileDims[0] * voxelFileDims[1] * voxelFileDims[2];
	}

	// Data
	size_t numBytes =
		numFrameTris * sizeof(uint32_t) * 3 + // triangles
		numFrameVerts * sizeof(float) * 2 + // texCoords
		numVerts * sizeof(float) * 6 + // frames
		numSockets * (nameSize + numFrames * sizeof(float) * 7) + // sockets
		numAnimations * (nameSize + sizeof(uint32_t) * 2) + // animations
		numFileVoxels * sizeof(float); // voxels

	bytes = new unsigned char[numBytes];

	if(fread(bytes, sizeof(unsigned char), numBytes, file) != numBytes)
		LOAD_MESH_FILE_FAIL("Could not read all expected data");

	// Triangles
	unsigned char* curByte = bytes;
	indices = new uint32_t[numFrameIndices];

	for(size_t i = 0; i < numFrameIndices; i++)
	{
		com::MergeLE(curByte, indices[i]);

		if(indices[i] >= numFrameVerts)
			LOAD_MESH_FILE_FAIL("Out-of-bounds vertex index");

		 curByte += sizeof(uint32_t);
	}

	// Texture coordinates
	vt = new vertex_mesh_tex[numFrameVerts];

	for(uint32_t i = 0; i < numFrameVerts; i++)
	{
		for(size_t j = 0; j < 2; j++)
		{
			MergeFloat(curByte, vt[i].texCoord[j]);
			curByte += sizeof(float);
		}
	}

	// Frames
	vp = new vertex_mesh_place[numVerts];
	MergeVertexPlaces(numFrames, numFrameVerts, (vertex_mesh_place*)vp, curByte);

	// Sockets
	if(numSockets)
		sockets = new Socket[numSockets];

	for(uint32_t i = 0; i < numSockets; i++)
	{
		const char* err;
		Socket* s = SetExtraName(sockets, i, curByte, nameSize, err);

		if(!s)
			LOAD_MESH_FILE_FAIL(err);

		curByte += nameSize;
		s->AllocValues(numFrames);

		for(size_t j = 0; j < numFrames; j++)
		{
			com::place& p = s->Values()[j];

			for(size_t k = 0; k < 3; k++)
			{
				MergeFloat(curByte, p.pos[k]);
				curByte += sizeof(float);
			}

			for(size_t k = 0; k < 4; k++)
			{
				MergeFloat(curByte, p.ori[k]);
				curByte += sizeof(float);
			}
		}
	}

	// Animations
	if(numAnimations)
		animations = new Animation[numAnimations];

	for(size_t i = 0; i < numAnimations; i++)
	{
		const char* err;
		Animation* a = SetExtraName(animations, i, curByte, nameSize, err);

		if(!a)
			LOAD_MESH_FILE_FAIL(err);

		curByte += nameSize;
		com::MergeLE(curByte, &a->start, 1);
		curByte += sizeof(a->start);
		com::MergeLE(curByte, &a->end, 1);
		curByte += sizeof(a->end);

		if(a->start > a->end || a->start >= numFrames || a->end >= numFrames)
			LOAD_MESH_FILE_FAIL("Invalid animation frame range");
	}

	if(flags & FILE_FLAG_VOXELS)
	{
		// Voxels
		// Add a 0-value border for clamping and convert dimensions to powers of two for GPU
		for(size_t i = 0; i < 3; i++)
		{
			voxelMin[i] = voxelFileMin[i] - 1;
			voxelDims[i] = com::NextPow2((uint32_t)voxelFileDims[i] + 2);
		}

		numVoxels = voxelDims[0] * voxelDims[1] * voxelDims[2];
		voxels = new GLfloat[numVoxels];

		for(size_t i = 0; i < numVoxels; i++)
			voxels[i] = 0.0f;

		// Copy into voxel map by row since dimensions are different
		for(int32_t fz = 0; fz < voxelFileDims[2]; fz++)
		{
			uint32_t z = (uint32_t)fz + 1;

			for(int32_t fy = 0; fy < voxelFileDims[1]; fy++)
			{
				uint32_t y = (uint32_t)fy + 1;
				uint32_t rowStart = 1 + y * voxelDims[0] + z * voxelDims[0] * voxelDims[1];

				if(COM_EQUAL_TYPES(float, GLfloat))
				{
					com::MergeLE(curByte, voxels + rowStart, voxelFileDims[0]);
					curByte += voxelFileDims[0] * sizeof(float);
				}
				else
				{
					for(int32_t index = 0; index < voxelFileDims[0]; index++)
					{
						MergeFloat(curByte, voxels[rowStart + index]);
						curByte += sizeof(float);
					}
				}
			}
		}
	}

	// Done
	fclose(file);
	file = 0;
	delete[] bytes;

	static const uintmax_t
		NUM_GLUBYTE = com::PowU(256, sizeof(GLubyte)),
		NUM_GLUSHORT = com::PowU(256, sizeof(GLushort)),
		NUM_GLUINT = com::PowU(256, sizeof(GLuint));

	if(numFrameVerts <= NUM_GLUBYTE)
	{
		indexTypeOut = GL_UNSIGNED_BYTE;
		indicesOut = com::ConvertArray<GLubyte>(indices, numFrameIndices);
	}
	else if(numFrameVerts <= NUM_GLUSHORT)
	{
		indexTypeOut = GL_UNSIGNED_SHORT;
		indicesOut = com::ConvertArray<GLushort>(indices, numFrameIndices);
	}
	else if(numFrameVerts <= NUM_GLUINT)
	{
		indexTypeOut = GL_UNSIGNED_INT;
		indicesOut = com::ConvertArray<GLuint>(indices, numFrameIndices);
	}
	else
		LOAD_MESH_FILE_FAIL("Too many frame vertices");

	if(indices != indicesOut)
		delete[] indices;

	vpOut = vp;
	vtOut = vt;
	animationsOut = animations;
	numAnimationsOut = numAnimations;
	socketsOut = sockets;
	numSocketsOut = numSockets;
	numFramesOut = numFrames;
	numFrameTrisOut = numFrameTris;
	numFrameVertsOut = numFrameVerts;
	frameRateOut = frameRate;
	voxelsOut = voxels;

	if(voxelScaleOut)
		*voxelScaleOut = voxelScale;

	if(voxelMinOut)
		com::Copy(voxelMinOut, voxelMin, 3);

	if(voxelDimsOut)
		com::Copy(voxelDimsOut, voxelDims, 3);

	return 0;
}

/*--------------------------------------
	rnd::FreeMesh
--------------------------------------*/
void rnd::FreeMesh(void* vp, vertex_mesh_tex* vt, void* indices, Socket* sockets,
	Animation* animations, GLfloat* voxels)
{
	if(vp)
		delete[] (vertex_mesh_place*)vp;

	if(vt)
		delete[] vt;

	if(indices)
		delete[] indices;

	if(sockets)
		delete[] sockets;

	if(animations)
		delete[] animations;

	if(voxels)
		delete[] voxels;
}

/*--------------------------------------
	rnd::CloseMesh
--------------------------------------*/
const char* rnd::CloseMesh(FILE* file, void* vp, vertex_mesh_tex* vt, void* indices,
	Socket* sockets, Animation* animations, GLfloat* voxels, unsigned char* bytes,
	const char* err)
{
	if(file)
		fclose(file);

	FreeMesh(vp, vt, indices, sockets, animations, voxels);

	if(bytes)
		delete[] bytes;

	return err;
}

/*--------------------------------------
	rnd::MergeFloat
--------------------------------------*/
void rnd::MergeFloat(const unsigned char* curByte, GLfloat& fOut)
{
	if(COM_EQUAL_TYPES(GLfloat, float))
		com::MergeLE(curByte, fOut);
	else
	{
		float f;
		com::MergeLE(curByte, f);
		fOut = (GLfloat)f;
	}
}

/*--------------------------------------
	rnd::SetExtraName
--------------------------------------*/
template <class T> T* rnd::SetExtraName(T* extsIO, uint32_t index,
	const unsigned char* nameByte, uint32_t maxNameSize, const char*& errOut)
{
	const char* name = (const char*)nameByte;

	if(*(name + maxNameSize - 1))
	{
		errOut = "Mesh contains name that is not null terminated";
		return 0;
	}

	if(index)
	{
		int cmp = strcmp(name, extsIO[index - 1].Name());

		if(!cmp)
		{
			errOut = "Mesh contains duplicate name";
			return 0;
		}

		if(cmp < 0)
		{
			errOut = "Mesh contains unsorted name";
			return 0;
		}
	}

	extsIO[index].SetName(name);
	return &extsIO[index];
}

/*--------------------------------------
	rnd::MergeVertexPlaces
--------------------------------------*/
template <typename vertex_place>
void rnd::MergeVertexPlaces(uint32_t numFrames, uint32_t numFrameVerts, vertex_place* vpOut,
	unsigned char*& curByteIO)
{
	for(size_t i = 0; i < numFrames; i++)
	{
		// Positions
		for(size_t j = 0; j < numFrameVerts; j++)
		{
			for(size_t k = 0; k < 3; k++)
			{
				MergeFloat(curByteIO, vpOut[i * numFrameVerts + j].pos[k]);
				curByteIO += sizeof(float);
			}
		}

		// Normals
		for(size_t j = 0; j < numFrameVerts; j++)
		{
			for(size_t k = 0; k < 3; k++)
			{
				MergeFloat(curByteIO, vpOut[i * numFrameVerts + j].normal[k]);
				curByteIO += sizeof(float);
			}
		}
	}
}