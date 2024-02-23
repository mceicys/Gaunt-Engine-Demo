// hit_private.h
// Martynas Ceicys

#define HIT_NUM_RESULT_ELEMENTS 7

namespace hit
{
	// MANAGED HULL
	extern com::list<Hull> hulls;

	void DeleteHull(Hull* h);

	// DESCENT
	class Descent;
	extern Descent gDesc;

	// LEAF TEST
	int		BehindBevel(const Hull& h, com::Vec3& aInOut, const com::Vec3& b,
			const com::Qua& ori, const com::Plane& bevel, com::Vec3& hitNormalOut);
	void	LeafExitVec(const com::Vec3& pos, const scn::WorldNode& leaf, Result& resOut);
	void	LeafExitVec(const Hull& h, const com::Vec3& pos, const com::Qua& ori,
			const scn::WorldNode& leaf, Result& resOut);
}