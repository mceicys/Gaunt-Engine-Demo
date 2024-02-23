// mesh.h
// Martynas Ceicys

#ifndef RND_MESH_H
#define RND_MESH_H

#include <stdint.h>
#include "../../GauntCommon/math.h"
#include "../hit/hull.h"
#include "../resource/resource.h"

namespace scn
{
	class Entity;
}

namespace rnd
{

class Socket;
class Animation;

/*======================================
	rnd::Mesh

Multiple frames of 3D vertices with texture coordinates and normals
======================================*/
class Mesh : public res::Resource<Mesh>
{
public:
	enum
	{
		VOXELS = 1
	};

	const char*						FileName() const {return fileName;}
	uint16_t						Flags() const {return flags;}
	uint32_t						NumFrames() const {return numFrames;}
	uint32_t						FrameRate() const {return frameRate;}
	const Socket*					Sockets() const {return sockets;}
	const Socket*					FindSocket(const char* name) const;
	const Socket*					FindSocket(uint32_t i) const;
	uint32_t						NumSockets() const {return numSockets;}
	const Animation*				Animations() const {return animations;}
	const Animation*				FindAnimation(const char* name) const;
	const Animation*				FindAnimation(uint32_t i) const;
	uint32_t						NumAnimations() const {return numAnimations;}
	const hit::Hull&				Bounds() const {return bounds;}
	float							Radius() const {return radius;}

protected:
	char*							fileName;
	uint32_t						numFrames, frameRate, numSockets, numAnimations;
	Socket*							sockets;
	Animation*						animations;
	hit::Hull						bounds;
	float							radius;
	uint16_t						flags;

									Mesh(const char* fileName, uint16_t flags,
									uint32_t numFrames, uint32_t frameRate, Socket* sockets,
									uint32_t numSockets, Animation* animations,
									uint32_t numAnimations, const com::Vec3& boxMin,
									const com::Vec3& boxMax, float radius);
									~Mesh();

private:
									Mesh();
									Mesh(const Mesh&);
};

/*======================================
	rnd::MeshExtra
======================================*/
template <class Extra> class MeshExtra : public res::Resource<Extra>
{
public:
	res::Ptr<Mesh> mesh;

				MeshExtra() : name(0) {}
				~MeshExtra() {SetName(0);}

	const char*	Name() const {return name;}

	void SetName(const char* n)
	{
		if(name)
			delete[] name;

		name = 0;

		if(n)
		{
			name = new char[strlen(n) + 1];
			strcpy(name, n);
		}
	}

	// IN	extE
	// OUT	mshM
	static int CLuaMesh(lua_State* l)
	{
		Extra* e = Extra::CheckLuaTo(1);

		if(e->mesh)
			e->mesh->LuaPush();
		else
			lua_pushnil(l);

		return 1;
	}

	// IN	extE
	// OUT	sName
	static int CLuaName(lua_State* l)
	{
		Extra* e = Extra::CheckLuaTo(1);

		if(e->Name())
			lua_pushstring(l, e->Name());
		else
			lua_pushnil(l);

		return 1;
	}

protected:
	char*		name;

				MeshExtra(const MeshExtra&) {}
				MeshExtra& operator=(const MeshExtra&) {}
};

/*======================================
	rnd::MeshExtraArray
======================================*/
template <class Extra, class Val> class MeshExtraArray : public MeshExtra<Extra>
{
public:
				MeshExtraArray() : values(0), numValues(0) {}
				~MeshExtraArray() {AllocValues(0);}

	void AllocValues(size_t num)
	{
		if(values)
			delete[] values;

		numValues = num;

		if(!num)
		{
			values = 0;
			return;
		}

		values = new Val[num];
	}

	Val*		Values() {return values;}
	const Val*	Values() const {return values;}
	size_t		NumValues() const {return numValues;}

	// IN	extE
	// OUT	iNumValues
	static int CLuaNumValues(lua_State* l)
	{
		Extra* e = Extra::CheckLuaTo(1);
		lua_pushinteger(l, e->NumValues());
		return 1;
	}

protected:
	Val*		values;
	size_t		numValues;
};

/*======================================
	rnd::Animation
======================================*/
class Animation : public MeshExtra<Animation>
{
public:
	uint32_t start, end;
	Animation() : start(0), end(0) {}
};

struct ani_frame
{
	const rnd::Animation* ani;
	uint32_t frame;
};

/*======================================
	rnd::Socket
======================================*/
class Socket : public MeshExtraArray<Socket, com::place>
{
public:
	com::place	Place(const Animation* ani, float frame) const;
	com::place	PlaceEntity(const scn::Entity& ent, float fraction) const;
	com::place	PlaceAlign(const com::Vec3& p, const com::Qua& o, const scn::Entity& ent,
				float time) const;

private:
	bool CheckAnimation(const Animation* ani) const;
};

}

#endif