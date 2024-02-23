// scene.h -- Game space
// Martynas Ceicys

#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

#include "../flag/flag.h"
#include "../../GauntCommon/array.h"
#include "../../GauntCommon/link.h"
#include "../../GauntCommon/mark.h"
#include "../../GauntCommon/math.h"
#include "../../GauntCommon/poly.h"
#include "../../GauntCommon/qua.h"
#include "../../GauntCommon/vec.h"
#include "../hit/hull.h"
#include "../record/pairs.h"
#include "../render/mesh.h"
#include "../render/regs.h"
#include "../render/texture.h"
#include "../resource/resource.h"

namespace scn
{

class Entity;
class Bulb;
class WorldNode;
class Zone;

template <class T> struct obj_link
{
	T*		obj;
	size_t	reverseIndex; // Index of obj_link in obj that points to this link's owner
};

typedef obj_link<Entity>	ent_link;
typedef obj_link<Bulb>		bulb_link;
typedef obj_link<WorldNode>	leaf_link;
typedef obj_link<Zone>		zone_link;

/*
################################################################################################
	CAMERA
################################################################################################
*/

/*======================================
	scn::Camera
======================================*/
class Camera : public res::Resource<Camera>
{
public:
	enum
	{
		LERP_POS = 1 << 0,
		LERP_ORI = 1 << 1,
		LERP_FOV = 1 << 2
	};

				Camera(const com::Vec3& p, const com::Qua& o, float fov);

	com::Vec3	pos, oldPos;
	com::Qua	ori, oldOri;
	float		fov, oldFOV; // vertical
	com::Vec3	skewOrigin;
	com::Vec2	zSkew; // How much z is added to x and y
	uint32_t	flags;

	com::Vec3	FinalPos() const;
	com::Qua	FinalOri() const;
	float		FinalFOV() const;
	com::Vec3	FinalSkewOrigin() const;
	com::Vec2	FinalZSkew() const;
	void		SaveOld();

private:
				Camera();
};

Camera*	ActiveCamera();
void	SetActiveCamera(Camera* cam);

/*
################################################################################################
	ENTITY TYPE
################################################################################################
*/

enum ent_func
{
	ENT_FUNC_INIT,
	ENT_FUNC_TICK,
	ENT_FUNC_TERM,
	ENT_FUNC_SAVE,
	ENT_FUNC_LOAD,
	NUM_ENT_FUNCS
};

/*======================================
	scn::EntityType

Determines Entity's simulation functions.
======================================*/
class EntityType : public res::Resource<EntityType>
{
public:
	const char*					Name() const {return name;}
	const int*					Refs() const {return ref;}
	bool						HasRef(int func) const {return ref[func] != LUA_NOREF;}

	friend EntityType*			CreateEntityType(const char* name, int numFuncs);

private:
								EntityType(const char* name, int numFuncs);
								EntityType(const EntityType&);
								~EntityType();

	char*						name;
	int							ref[NUM_ENT_FUNCS]; // Function references
};

EntityType*	FindEntityType(const char* name);

/*
################################################################################################
	ENTITY
################################################################################################
*/

/*======================================
	scn::Entity

Movable object with collision hull and model.

FIXME: if the game's designed for variable tick rate, consider getting rid of all this lerp crap
======================================*/
class Entity : public res::Resource<Entity, true>
{
public:
	enum
	{
		VISIBLE = 1 << 0, // If not set, not drawn in the world nor in overlays
		WORLD_VISIBLE = 1 << 1, // If not set, not drawn in the world
		SHADOW = 1 << 2, // Cast a shadow, even if inVISIBLE
		REGULAR_GLASS = 1 << 3, // Adds brightness or divides light intensity
		AMBIENT_GLASS = 1 << 4, // Adds light intensity
		MINIMUM_GLASS = 1 << 5, // Sets a minimum light intensity
		WEAK_GLASS = 1 << 6, // Only recolor if frag has no light sub-palette
		CLOUD = 1 << 7,
		LERP_POS = 1 << 8,
		LERP_ORI = 1 << 9,
		LERP_SCALE = 1 << 10,
		LERP_UV = 1 << 11,
		LERP_OPACITY = 1 << 12,
		LERP_FRAME = 1 << 13,
		LOOP_ANIM = 1 << 14,
		ALT_HIT_TEST = 1 << 15 // Test descendant chain when doing an ALT type hit test
	};

	class Track;

	com::Vec3				oldPos;
	com::Qua				oldOri;
	float					oldScale;
	com::Vec2				uv, oldUV;
	EntityType*				type;
	flg::FlagSet			gFlags; // Game flags
	res::Ptr<rnd::Texture>	tex;
	int						subPalette; // FIXME: why an integer?
	float					opacity, oldOpacity;
	float					animSpeed; // Multiplies mesh's frame rate
	mutable unsigned		hitCode; // Already tested hit or cull if equal to mark code
	uint16_t				flags;

	void					DummyCopy(const Entity& src);
	const com::Vec3&		Pos() const {return pos;}
	void					SetPos(const com::Vec3& p);
	com::Vec3				FinalPos() const;
	const com::Qua&			Ori() const {return ori;}
	const com::Qua&			HullOri() const {return pFlags & P_ORI_HULL ? ori : com::QUA_IDENTITY;}
	void					SetOri(const com::Qua& o);
	com::Qua				FinalOri() const;
	void					SetPlace(const com::Vec3& p, const com::Qua& o);
	float					Scale() const {return scale;}
	void					SetScale(float s);
	float					FinalScale() const;
	void					SetScaledPlace(const com::Vec3& p, const com::Qua& o, float s);
	bool					OrientHull() const {return (pFlags & P_ORI_HULL) != 0;}
	void					SetOrientHull(bool o);
	com::Vec2				FinalUV() const;
	rnd::Mesh*				Mesh() const {return msh;}
	void					SetMesh(rnd::Mesh* m);
	hit::Hull*				Hull() const {return hull;}
	void					SetHull(hit::Hull* h);
	hit::Hull*				LinkHull() const {return linkHull;}
	void					SetLinkHull(hit::Hull* h);
	void					UnsetLinkingObjects();
	com::Vec3				Average() const;
	float					FinalOpacity() const;
	bool					Glass() const;
	void					SetAnimation(const rnd::Animation& a, float frame, float trans);
	void					UpdateAnimation();
	void					ClearAnimation();
	const rnd::Animation*	Animation() const {return anim;}
	float					Frame() const {return frame;}
	bool					AnimationPlaying() const;
	bool					UpcomingFrame(float target) const;
	float					TransitionTime() const {return transTime;}
	size_t					TransitionA() const {return midA;}
	size_t					TransitionB() const {return midB;}
	float					TransitionF() const {return midF;}
	int						Overlay() const;
	uint16_t				OverlayFlags() const {return (pFlags & (P_OVERLAY_0 | P_OVERLAY_1)) >> 5;}
	void					SetOverlay(int overlay);
	const leaf_link*		LeafLinks() const {return leafLinks.o;}
	size_t					NumLeafLinks() const {return numLeafLinks;}
	Entity*					Child() {return child;}
	const Entity*			Child() const {return child;}
	void					SetChild(Entity* other);
	bool					PointLink() {return (pFlags & P_POINT_LINK) != 0;}
	void					SetPointLink(bool point);
	bool					Sky() const {return (pFlags & P_SKY_OVERLAY) != 0;}
	void					SetSky(bool sky);
	void					MimicMesh(const Entity& orig);
	void					MimicScaledPlace(const Entity& orig);
	void					Transcribe();
	void					InterpretTranscript();
	void					UnsafeCall(int func, int numArgs = 0);
	void					Call(int func, int numArgs = 0);
	void					SaveOld();
	const char*				TypeName() const {return type ? type->Name() : "no type";}

	static unsigned			IncHitCode() {COM_INC_MARK_CODE_LIST(Entity, hitCode, List().f);}

private:
	enum
	{
		P_DYING = 1 << 0, // KillEntity has been called
		P_ORI_HULL = 1 << 1,
		P_CHILD = 1 << 2,
		P_POINT_LINK = 1 << 3, // Link entity as a point if it has no mesh or hull
		P_SKY_OVERLAY = 1 << 4,
		P_OVERLAY_0 = 1 << 5, // Update OverlayFlags if these constants change
		P_OVERLAY_1 = 1 << 6
	};

	com::Vec3				pos;
	com::Qua				ori;
	float					scale; // Only affects mesh
	com::Arr<leaf_link>		leafLinks;
	size_t					numLeafLinks;
	com::Arr<zone_link>		zoneLinks;
	size_t					numZoneLinks;
	res::Ptr<rnd::Mesh>		msh;
	res::Ptr<hit::Hull>		hull;
	res::Ptr<hit::Hull>		linkHull; // Prioritized hull for linking, never oriented
	res::Ptr<Entity>		child; // child does not link to world
	// FIXME: save + load animation stuff
	res::Ptr<const rnd::Animation> anim; // Currently played or transition target
	float					frame; // Relative; shown at start of tick if not in transition
	size_t					midA, midB; // Transition state
	float					midF, transTime; // In transition if transTime > 0.0
	uint16_t				pFlags; // Private flags

	static const size_t		DEF_NUM_LEAF_LINKS_ALLOC = 1;
	static const size_t		DEF_NUM_ZONE_LINKS_ALLOC = 1;

							Entity(EntityType* et, const com::Vec3& p, const com::Qua& o);
							~Entity();
	void					LinkWorld();
	void					UnlinkWorld();

public:
	friend Entity*			CreateEntity(EntityType* et, const com::Vec3& pos,
							const com::Qua& ori);
	friend void				KillEntity(Entity& ent);
	friend class			res::LicenseToDelete<Entity, true>;
};

void			ClearEntities();
void			InterpretEntities(com::PairMap<com::JSVar>& entities);

/*
################################################################################################
	WORLD

FIXME: remove experimental stuff
FIXME: encapsulate
FIXME: separate loading code from simulation code
################################################################################################
*/

// For texture querying via hit tests
struct leaf_triangle
{
	com::Vec3		positions[3];
	com::Vec2		texCoords[3];
	com::Plane		pln;
	rnd::Texture*	tex;
};

/*======================================
	scn::PortalSet
======================================*/
class PortalSet
{
public:
	Zone *front, *back;
	com::Poly* polys;
	uint32_t numPolys;

	PortalSet() : front(0), back(0), polys(0), numPolys(0) {}
	~PortalSet() {if(polys) delete[] polys;}
	Zone* Other(const Zone& z) const {return front == &z ? back : front;}
};

/*======================================
	scn::Zone
======================================*/
class Zone
{
public:
	enum
	{
		SUN_SOURCE = 1 << 0
	};

	uint32_t			id;

	uint32_t			flags;

	WorldNode**			leaves; // FIXME: useless after loading, remove
	uint32_t			numLeaves;

	PortalSet**			portals;
	uint32_t			numPortals;

	com::Arr<ent_link>	entLinks;
	size_t				numEntLinks;
	com::Arr<bulb_link>	bulbLinks;
	size_t				numBulbLinks;

	rnd::zone_reg*		rndReg; // FIXME: renderer can align its array with scene's

	mutable unsigned	drawCode;

	Zone() : id(0), flags(0), leaves(0), numLeaves(0), portals(0), numPortals(0),
		numEntLinks(0), numBulbLinks(0), rndReg(0), drawCode(0) {}

	~Zone() {if(leaves) delete[] leaves; if(portals) delete[] portals;}
};

/*======================================
	scn::Node
======================================*/
class Node
{
public:
	enum // Flags
	{
		SOLID = 1
	};

	uint32_t	id; // FIXME: make this an index?
	uint32_t	flags; // For leaves only

	Node*		left;
	Node*		right;
	Node*		parent;

	com::Plane*	planes;
	uint32_t	numPlanes;

	Node() : id(0), flags(0), left(0), right(0), parent(0), planes(0), numPlanes(0) {}
	~Node() { if(planes) delete[] planes; }
};

/*======================================
	scn::WorldNode
======================================*/
class WorldNode : public Node
{
public:
	bool				solid; // FIXME: use SOLID flag
	Zone*				zone;
	bool*				planeExits;
	hit::Convex*		hull;

	com::Plane*			bevels;
	uint32_t			numBevels;

	WorldNode**			pvls; // FIXME: remove
	uint32_t			numPVLs;

	leaf_triangle*		triangles;
	uint32_t			numTriangles;

	com::Arr<ent_link>	entLinks; // FIXME: links should share memory pool; com::Pool?
	size_t				numEntLinks; // FIXME: links should be const to other systems

	WorldNode() : solid(0), zone(0), planeExits(0), hull(0), bevels(0), numBevels(0), pvls(0),
		numPVLs(0), triangles(0), numTriangles(0), numEntLinks(0) {}

	~WorldNode() {
		if(planeExits)
			delete[] planeExits;
		
		if(hull)
			delete hull;

		if(bevels)
			delete[] bevels;

		if(pvls)
			delete[] pvls;

		if(triangles)
			delete[] triangles;

		if(entLinks.o)
			entLinks.Free();
	}

	const leaf_triangle* ClosestLeafTriangle(const com::Vec3& pos) const;
	void LuaPush(lua_State* l) const {lua_pushinteger(l, id - 1);}
};

// Temporary loading structures
// FIXME: put in source file

/*======================================
	scn::TriBatch
======================================*/
class TriBatch
{
public:
	struct vertex
	{
		float pos[3];
		float texCoords[2];
		float normal[3];
		float subPalette, ambient, darkness;
	};

	Zone*			zone;
	rnd::Texture*	tex;
	uint32_t		numVertices;
	vertex*			vertices;
	uint32_t		numTriangles;
	uint32_t*		elements;
	uint32_t		firstGlobalVertex, firstGlobalTriangle;

	TriBatch() : zone(0), tex(0), numVertices(0), vertices(0), numTriangles(0), elements(0),
		firstGlobalVertex(0), firstGlobalTriangle(0) {}

	~TriBatch()
	{
		if(vertices)
			delete[] vertices;

		if(elements)
			delete[] elements;
	}
};

/*======================================
	scn::ZoneExt
======================================*/
class ZoneExt
{
public:
	uint32_t	firstBatch, numBatches, numPortalsAlloc;

	ZoneExt() : firstBatch(0), numBatches(0), numPortalsAlloc(0) {}

	~ZoneExt() {
	}
};

bool					LoadWorld(const char* const fileName);
void					ClearWorld();

const char*				WorldFileName();
WorldNode*				WorldRoot();
const com::Vec3&		WorldMin();
const com::Vec3&		WorldMax();
WorldNode*				CheckLuaToWorldNode(lua_State* l, int index);
leaf_triangle*			CheckLuaToLeafTriangle(lua_State* l, int nodeIndex, int triIndex);
size_t					MaxNumTreeLevels();
const Zone*				Zones();
uint32_t				NumZones();
const PortalSet*		PortalSets();
size_t					NumPortalSets();
const Node*				PosToLeaf(const Node* root, const com::Vec3& pos);
Node*					PosToLeaf(Node* root, const com::Vec3& pos);
unsigned				IncZoneDrawCode();
const leaf_triangle*	NearbyTriangle(const WorldNode* root, const com::Vec3& pos,
						const WorldNode*& leafOut);

/*
################################################################################################
	LIGHT
################################################################################################
*/

/*======================================
	scn::Light
======================================*/
class Light
{
public:
	enum
	{
		LERP_INTENSITY = 1 << 0
	};

	uint32_t	flags;
	float		intensity, oldIntensity;
	int			subPalette;
	float		colorSmooth; // [0, 1], how random the color influence of each fragment is
	float		colorPriority; // Multiplies color influence

				Light(float intensity, int subPalette);
	float		FinalIntensity() const;
	void		TranscribeGlobals(com::PairMap<com::JSVar>& globalOut, const char* prefix);
	void		InterpretGlobals(const com::PairMap<com::JSVar>& global, const char* prefix,
				float defIntensity);
};

/*======================================
	scn::Sun
======================================*/
class Sun : public Light
{
public:
	enum
	{
		LERP_POS = 1 << 1
	};

	com::Vec3	pos, oldPos;

				Sun(const com::Vec3& pos, float intensity, int subPalette);
	com::Vec3	FinalPos() const;
};

extern Light ambient, sky, cloud;
extern Sun sun;

/*
################################################################################################
	BULB
################################################################################################
*/

/*======================================
	scn::Bulb
======================================*/
class Bulb : public Light, public res::Resource<Bulb>
{
public:
	enum
	{
		LERP_POS = 1 << 1,
		LERP_RADIUS = 1 << 2,
		LERP_EXPONENT = 1 << 3,
		LERP_ORI = 1 << 4,
		LERP_CUTOFF = 1 << 5
	};

	com::Vec3			oldPos;
	float				oldRadius;
	float				oldExponent, exponent;
	com::Qua			ori, oldOri; // Spotlight orientation
	float				outer, oldOuter, inner, oldInner; // Spotlight cutoff angles
	mutable unsigned	drawCode;

	const com::Vec3&	Pos() const {return pos;}
	void				SetPos(const com::Vec3& p);
	com::Vec3			FinalPos() const;
	const float			Radius() const {return radius;}
	void				SetRadius(float r);
	float				FinalRadius() const;
	float				FinalExponent() const;
	com::Qua			FinalOri() const;
	float				FinalOuter() const;
	float				FinalInner() const;
	void				SetPlace(const com::Vec3& p, float r);
	const zone_link*	ZoneLinks() const {return zoneLinks.o;}
	size_t				NumZoneLinks() const {return numZoneLinks;}
	void				SaveOld();
	void				Transcribe();
	void				InterpretTranscript();

	static unsigned		IncDrawCode() {COM_INC_MARK_CODE_LIST(Bulb, drawCode, List().f);}

private:
	com::Vec3			pos;
	float				radius;
	com::Arr<zone_link>	zoneLinks;
	size_t				numZoneLinks;

	static const size_t	DEF_NUM_ZONE_LINKS_ALLOC = 1;

						Bulb(const com::Vec3& pos, float radius, float exponent,
						float intensity, int subPalette, const com::Qua& ori, float outer,
						float inner);
						~Bulb();
	void				LinkWorld();
	void				UnlinkWorld();
	void				LinkZone(Zone& zone);

public:
	friend Bulb*		CreateBulb(const com::Vec3& pos, float radius, float exponent,
						float intensity, int subPalette,
						const com::Qua& ori = com::QUA_IDENTITY, float outer = COM_PI,
						float inner = COM_PI);
	friend void			DeleteBulb(Bulb* bulb);
};

void	ClearBulbs();
void	InterpretBulbs(com::PairMap<com::JSVar>& bulbs);

/*
################################################################################################
	OVERLAY
################################################################################################
*/

extern const size_t NUM_OVERLAYS;

/*======================================
	scn::Overlay
======================================*/
class Overlay : public res::Resource<Overlay>
{
public:
	enum
	{
		RELIT = 1 << 0 // Overlay lit separately so active camera's placement has no effect
	};

	res::Ptr<const scn::Camera>				cam;
	uint32_t								flags;

											Overlay();
											~Overlay();
	const res::Ptr<const scn::Entity>*		Entities() const {return ents.o;}
	size_t									NumEntities() const {return numEnts;}

private:
	com::Arr<res::Ptr<const scn::Entity>>	ents;
	size_t									numEnts;

	void									AddEntity(const scn::Entity& ent);
	void									RemoveEntity(const scn::Entity& ent);
	size_t									FindEntity(const scn::Entity& ent);

	friend class							Entity;
};

Overlay* Overlays();
Overlay& SkyOverlay();

/*
################################################################################################
	FOG
################################################################################################
*/

/*======================================
	scn::Fog

FIXME: saving and loading
======================================*/
class Fog : public res::Resource<Fog>
{
public:
	com::Vec3	center;
	float		radius, thinRadius, thinExponent;
	float		geoSubStartDist, geoSubHalfDist, geoSubFactor;
	int			geoSubPalette;
	float		fadeStartDist, fadeHalfDist, fadeAmount, fadeRampPos, fadeRampPosSun;
	float		addStartDist, addHalfDist, addAmount, addAmountSun;
	float		sunRadius, sunExponent;

				Fog();
	void		TranscribeGlobals(com::PairMap<com::JSVar>& globalOut, const char* prefix);
	void		InterpretGlobals(const com::PairMap<com::JSVar>& global, const char* prefix);
};

extern Fog fog;

/*
################################################################################################
	GENERAL
################################################################################################
*/

void	SaveSpace();
void	Init();
void	CallEntityFunctions(ent_func func);
void	SetGlobalPairs(com::PairMap<com::JSVar>& globalOut);
bool	InterpretGlobalPairs(const com::PairMap<com::JSVar>& global);

}

#endif