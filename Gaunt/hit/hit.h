// hit.h -- Collision detection
// Martynas Ceicys

#ifndef HIT_H
#define HIT_H

#include "hull.h"
#include "../flag/flag.h"
#include "../scene/scene.h"
#include "../script/script.h"

#define HIT_ERROR_BUFFER 0.05f
#define HIT_ERROR_BUFFER_SQ 0.0025f
#define HIT_GREATER_BUFFER_1 0.055f
#define HIT_LESSER_BUFFER_1 0.045f
#define HIT_LESSER_BUFFER_2 0.04f

namespace hit
{

/*
################################################################################################
	HULL
################################################################################################
*/

void		Span(const Hull& h, const com::Qua& ori, const com::Vec3& axis, float& minOut,
			float& maxOut);
com::Vec3	Average(const Hull& h);
void		DrawWire(const Hull& h, const com::Vec3& p, const com::Qua& o, int color,
			float time = 0.0f);

/*
################################################################################################
	MANAGED HULL
################################################################################################
*/

Hull*	FindHull(const char* name);
Hull*	EnsureHull(const char* filePath);

/*
################################################################################################
	RESULT
################################################################################################
*/

class Result
{
public:
	enum {
		NONE = 0,
		HIT,
		INTERSECT
	};

	int				contact;
	float			timeFirst;	// Time of first hit
	float			timeLast;	// Time of last hit
	com::Vec3		normal;		// Normal of first surface hit
	scn::Entity*	ent;		// Ptr to entity that was hit, 0 for level geometry

	// Minimum translation vector
	float			mtvMag;
	com::Vec3		mtvDir;

	void			LuaPush(lua_State* l);
};

void ResultTable(lua_State* l, int tableIndex, const Result* results, size_t numResults);

/*
################################################################################################
	HIT TEST
################################################################################################
*/

enum test_type
{
	ENTITIES = 1,
	TREE = 2,
	ALT = 4,
	ALL = ENTITIES | TREE,
	ENTITIES_ALT = ENTITIES | ALT,
	ALL_ALT = ENTITIES | TREE | ALT
};

Result	LineTest(scn::WorldNode* root, const com::Vec3& a, const com::Vec3& b,
		const test_type type, const flg::FlagSet& ignore, const scn::Entity** entIgnores,
		size_t numEntIgnores);
Result	LineTest(scn::WorldNode* root, const com::Vec3& a, const com::Vec3& b,
		const test_type type, const flg::FlagSet& ignore, const scn::Entity* entIgnore);
Result	HullTest(scn::WorldNode* root, const Hull& hull, const com::Vec3& a, const com::Vec3& b,
		const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
		const scn::Entity** entIgnores, size_t numEntIgnores);
Result	HullTest(scn::WorldNode* root, const Hull& hull, const com::Vec3& a, const com::Vec3& b,
		const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
		const scn::Entity* entIgnore);

/*
################################################################################################
	HULL TEST
################################################################################################
*/

bool	TestLineHull(const com::Vec3& lineA, const com::Vec3& lineB, const Hull& h,
		const com::Vec3& hullPos, const com::Qua& hullOri, Result& resInOut);
Result	TestLineHull(const com::Vec3& lineA, const com::Vec3& lineB, const Hull& h,
		const com::Vec3& hullPos, const com::Qua& hullOri);
bool	TestHullHull(const Hull& hullA, const com::Vec3& oldPosA, const com::Vec3& posA,
		const com::Qua& oriA, const Hull& hullB, const com::Vec3& posB, const com::Qua& oriB,
		Result& resInOut);
Result	TestHullHull(const Hull& hullA, const com::Vec3& oldPosA, const com::Vec3& posA,
		const com::Qua& oriA, const Hull& hullB, const com::Vec3& posB, const com::Qua& oriB);

/*
################################################################################################
	DESCENT
################################################################################################
*/

enum desc_op
{
	// Line descent operations
	ROOT,
	LEFT,
	COPY,
	RIGHT,
	SPLIT,

	// Hull descent operations
//	ROOT,
	DISCARD = LEFT,
//	COPY,
	CLIP = RIGHT
};

struct desc_elem
{
	scn::Node*	node;
	com::Vec3	a, b;
	com::Vec3	hitNormal; // Possible hit-surface normal
	desc_op		op; // Last operation done to get this segment

	/* True if segment contains the original start point; used to prevent false intersection
	when segment is clipped near original start point */
	bool		hasStart;
};	

// Stack array used for BSP tree descent
// A descent other than the global can be created if two concurrent descents need to be done
class Descent : public res::Resource<Descent>
{
public:
	com::Arr<desc_elem>	s;
	size_t				numStacked;

						Descent();
						~Descent();
	desc_elem*			CheckLuaToDescElem(lua_State* l, int index);
	static Descent*		CheckLuaToNotEmpty(int index);
	void				FitLargestTree();
	size_t				Begin(scn::Node* root, const com::Vec3& a, const com::Vec3& b);
	size_t				LineDescend();
	size_t				Descend(const Hull& h, const com::Qua& ori);
	size_t				SphereDescend(float radius);
};

Descent& GlobalDescent();

/*
################################################################################################
	COLLISION RESPONSE
################################################################################################
*/

com::Vec3	RespondStop(const Result& r, const com::Vec3& a, const com::Vec3& b,
			float* trOut = 0);
com::Vec3	MoveStop(const Hull& hull, const com::Vec3& a, const com::Vec3& b,
			const com::Qua& ori, test_type type, const flg::FlagSet& ignore,
			const scn::Entity* entIgnore, Result* rOut, float* tmOut = 0);
com::Vec3	MoveClimb(const Hull& hull, const com::Vec3& a, const com::Vec3& b,
			const com::Qua& ori, float height, float floorZ, test_type type,
			const flg::FlagSet& ignore, const scn::Entity* entIgnore, bool& climbedOut,
			Result* resultsOut, size_t* numResultsOut, float* tmiOut = 0, float* tmoOut = 0);
com::Vec3	MoveSlide(const Hull& hull, com::Vec3 pos, com::Vec3& velIO, const com::Qua& ori,
			float& timeIO, Result* resultsIO, size_t& numResultsIO, float climbHeight,
			float floorZ, test_type type, const flg::FlagSet& ignore,
			const scn::Entity* entIgnore);

/*
################################################################################################
	GENERAL
################################################################################################
*/

void Init();
void CleanupBegin();
void CleanupEnd();

}

#endif