// render_vis.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"

namespace rnd
{
	struct port_desc_elem
	{
		com::Poly			clip;
		const scn::Zone*	zone;
		uint32_t			set, poly;
		size_t				parent, planeStart;
		unsigned			fullDrawCode;
	};

	/*======================================
		rnd::PortalDescent
	======================================*/
	class PortalDescent
	{
	public:
		com::Arr<port_desc_elem>	stack;
		com::Arr<com::Plane>		planes;

									PortalDescent(size_t alloc = 0)
										: stack(alloc), planes(alloc * 4), numStack(0),
										numPlanes(0), fullDrawCode(0) {}
									~PortalDescent() {stack.Free(); planes.Free();}
		port_desc_elem&				Push(const scn::Zone* zone, size_t parent);
		void						Pop() {numPlanes -= stack[--numStack].clip.numVerts;}
		void						Clear() {numStack = numPlanes = 0;}
		size_t						NumStack() const {return numStack;}
		size_t						NumPlanes() const {return numPlanes;}
		port_desc_elem&				Top() {return stack[numStack - 1];}
		void						PushPlanes(size_t n);
		unsigned					FullDrawCode() const {return fullDrawCode;}
		unsigned					IncFullDrawCode();

	private:
		size_t						numStack, numPlanes;
		unsigned					fullDrawCode;
	};

	// Camera visibility
	com::Arr<const scn::Zone*>		visZones;
	com::Arr<const scn::Entity*>	visEnts, visCloudEnts, visGlassEnts;
	com::Arr<const scn::Bulb*>		visBulbs, visSpots;
	size_t							numVisZones = 0,
									numVisEnts = 0,
									numVisCloudEnts = 0,
									numVisGlassEnts = 0,
									numVisBulbs = 0,
									numVisSpots = 0;
	PortalDescent					pd(8);
	hit::Frustum					camHull;

	// Sun visibility
	com::Vec3		prevSunDir;
	PortalDescent	spd(8);

	void	CameraFrustum(const scn::Camera& cam, hit::Frustum& frsOut, com::Plane* plnsOut,
			com::Poly& clipOut);
	bool	ClipPortal(com::Poly& clipIO, const com::Plane* plns, size_t num);
	void	GeneratePerspectivePlanes(const com::Vec3& pos, const com::Poly& clip,
			com::Plane* planesIO);
	void	GenerateOrthographicPlanes(const com::Vec3& dir, const com::Poly& clip,
			com::Plane* planesIO);
	template <bool orthographic>
	bool	ClipSphere(const com::Vec3& pos, float radius, const com::Plane* plns,
			size_t numPlns, const com::Poly& clip, const com::Vec3* dirs);
	void	AddVisibleZoneObjects(const scn::Zone& zone, const com::Poly& clip,
			const com::Plane* plns, const com::Vec3& cam, unsigned entCode, unsigned bulbCode);
	void	AddVisibleEntity(const scn::Entity& ent);
	void	AddVisibleBulb(const scn::Bulb& bulb);
	void	MinConeSphere(float origRadius, float angle, float& radiusOut, float& offsetOut);
	int		CmpEntityModels(const void* a, const void* b);
	void	DrawPortalLines(const com::Poly& poly, int color, float time);
	void	DrawEntityLines(const scn::Entity& ent, int color, float time);
	void	DrawBulbLines(const scn::Bulb& bulb, float time);
}

/*--------------------------------------
	rnd::PortalDescent::Push

Set parent to -1 for root zone.
--------------------------------------*/
rnd::port_desc_elem& rnd::PortalDescent::Push(const scn::Zone* zone, size_t parent)
{
	stack.Ensure(numStack + 1);
	port_desc_elem& s = stack[numStack];
	s.zone = zone;
	s.set = s.poly = 0;
	s.parent = parent;
	s.planeStart = numPlanes;
	s.clip.numVerts = 0;
	numStack++;
	return s;
}

/*--------------------------------------
	rnd::PortalDescent::PushPlanes
--------------------------------------*/
void rnd::PortalDescent::PushPlanes(size_t n)
{
	numPlanes += n;
	planes.Ensure(numPlanes);
}

/*--------------------------------------
	rnd::PortalDescent::IncFullDrawCode
--------------------------------------*/
unsigned rnd::PortalDescent::IncFullDrawCode()
{
	fullDrawCode++;

	if(!fullDrawCode)
	{
		for(size_t i = 0; i < numStack; i++)
			stack[i].fullDrawCode = 0;

		fullDrawCode = 1;
	}

	return fullDrawCode;
}

/*--------------------------------------
	rnd::FloodVisible

Floods zones thru portals starting from active camera's position to populate vis arrays. After
calling, visible zones have a drawCode equal to zoneCode.
--------------------------------------*/
void rnd::FloodVisible(unsigned& zoneCode)
{
	numVisZones = numVisEnts = numVisCloudEnts = numVisGlassEnts = numVisBulbs = numVisSpots = 0;
	static scn::Camera cam(0.0f, com::QUA_IDENTITY, 1.0f);

	if(!lockCullCam.Bool())
		cam = *scn::ActiveCamera();

	com::Vec3 camPos = cam.FinalPos();
	const scn::WorldNode* start = (scn::WorldNode*)scn::PosToLeaf(scn::WorldRoot(), camPos);

	if(!start || !start->zone)
		return;

	int lineColor = (int)portalColor.Float();
	zoneCode = scn::IncZoneDrawCode();
	unsigned entCode = scn::Entity::IncHitCode();
	unsigned bulbCode = scn::Bulb::IncDrawCode();
	pd.Push(start->zone, -1);
	pd.PushPlanes(4);
	CameraFrustum(cam, camHull, pd.planes.o, pd.Top().clip);

	if(cameraColor.Float() >= 0.0f)
		camHull.DrawWire(camPos, cam.FinalOri(), (int)cameraColor.Float(), false);

	visZones.Ensure(numVisZones + 1);
	visZones[numVisZones++] = start->zone;
	start->zone->drawCode = zoneCode;
	AddVisibleZoneObjects(*start->zone, pd.Top().clip, pd.planes.o, camPos, entCode, bulbCode);

	while(pd.NumStack())
	{
		size_t curIndex = pd.NumStack() - 1;
		port_desc_elem* cur = &pd.stack[curIndex];
		bool pushed = false;

		for(; cur->set < cur->zone->numPortals; cur->set++, cur->poly = 0)
		{
			const scn::PortalSet& set = *cur->zone->portals[cur->set];
			const scn::Zone* other = set.Other(*cur->zone);

			// Don't go into zone if it's already on the stack or portal is facing wrong way
			if(!cur->poly)
			{
				if(PortalGap(set, camPos, other == set.front) < 0.0f)
					continue;

				size_t i = 0;
				for(; i < pd.NumStack() && pd.stack[i].zone != other; i++);

				if(i != pd.NumStack())
					continue;
			}

			for(; cur->poly < set.numPolys; cur->poly++)
			{
				port_desc_elem& next = pd.Push(other, curIndex);
				cur = &pd.stack[curIndex]; // Stack may have been reallocated
				next.clip = set.polys[cur->poly];

				if(!ClipPortal(next.clip, pd.planes.o + cur->planeStart, cur->clip.numVerts))
				{
					pd.Pop();
					continue;
				}

				if(other == set.front)
					next.clip.Flip(); // Make polygon face camera

				DrawPortalLines(next.clip, lineColor, false);

				if(other->drawCode != zoneCode)
				{
					visZones.Ensure(numVisZones + 1);
					visZones[numVisZones++] = other;
					other->drawCode = zoneCode;
				}
				
				pd.PushPlanes(next.clip.numVerts);
				GeneratePerspectivePlanes(camPos, next.clip, pd.planes.o + next.planeStart);
				AddVisibleZoneObjects(*other, next.clip, pd.planes.o + next.planeStart, camPos,
					entCode, bulbCode);

				cur->poly++;
				pushed = true;
				break;
			}

			if(pushed)
			{
				if(cur->poly == set.numPolys)
				{
					cur->set++;
					cur->poly = 0;
				}

				break;
			}
		}

		if(!pushed)
			pd.Pop();
	}

	// Minimize model-pass state changes
	qsort(visEnts.o, numVisEnts, sizeof(const scn::Entity*), CmpEntityModels);
	qsort(visCloudEnts.o, numVisCloudEnts, sizeof(const scn::Entity*), CmpEntityModels);
	qsort(visGlassEnts.o, numVisGlassEnts, sizeof(const scn::Entity*), CmpEntityModels);
}

/*--------------------------------------
	rnd::FloodSunVisible

Floods zones thru portals starting from sun zones to clip portals to their visible portions.
--------------------------------------*/
void rnd::FloodSunVisible()
{
	com::Vec3 dir = scn::sun.FinalPos().Normalized();

	if(dir == prevSunDir)
		return;

	prevSunDir = dir;
	spd.Clear();

	for(uint32_t z = 0; z < scn::NumZones(); z++)
	{
		const scn::Zone& src = scn::Zones()[z];

		if((src.flags & src.SUN_SOURCE) == 0)
			continue;

		spd.Push(&src, -1);

		for(size_t curIndex = spd.NumStack() - 1; curIndex < spd.NumStack(); curIndex++)
		{
			port_desc_elem* cur = &spd.stack[curIndex];

			for(uint32_t s = 0; s < cur->zone->numPortals; s++)
			{
				const scn::PortalSet& set = *cur->zone->portals[s];
				const scn::Zone* other = set.Other(*cur->zone);

				// Don't go into zone if it's already on branch or portal is facing wrong way
				if(PortalDot(set, dir, other == set.front) <= 0.0f)
					continue;

				size_t up = curIndex;
				for(; up != -1 && spd.stack[up].zone != other; up = spd.stack[up].parent);

				if(up != -1)
					continue;

				for(uint32_t p = 0; p < set.numPolys; p++)
				{
					port_desc_elem& next = spd.Push(other, curIndex);
					cur = &spd.stack[curIndex]; // Stack may have been reallocated
					next.clip = set.polys[p];

					if(!ClipPortal(next.clip, spd.planes.o + cur->planeStart, cur->clip.numVerts))
					{
						spd.Pop();
						continue;
					}

					if(other == set.front)
						next.clip.Flip(); // Make polygon face sun

					spd.PushPlanes(next.clip.numVerts);
					GenerateOrthographicPlanes(dir, next.clip, spd.planes.o + next.planeStart);
				}
			}
		}
	}
}

/*--------------------------------------
	rnd::TrimVisibleSunBranches

Marks which sun-portal-descent's elements are in zones or descend from zones with a drawCode
equal to zoneVisCode. Returns false if no element is marked.
--------------------------------------*/
bool rnd::TrimVisibleSunBranches(unsigned zoneVisCode)
{
	unsigned fullDrawCode = spd.IncFullDrawCode();
	bool drawSun = false;

	for(size_t i = 0; i < spd.NumStack(); i++)
	{
		port_desc_elem& cur = spd.stack[spd.NumStack() - 1 - i];

		if(cur.fullDrawCode == fullDrawCode)
			continue; // This element and ancestors already marked for full drawing

		if(cur.zone->drawCode == zoneVisCode)
		{
			// Camera sees this sun-lit zone, mark this element for full drawing
			cur.fullDrawCode = fullDrawCode;
			drawSun = true;

			// Also mark all ancestors since their entities may cast a shadow into this zone
			for(size_t pi = cur.parent; pi != -1;)
			{
				port_desc_elem& parent = spd.stack[pi];

				if(parent.fullDrawCode == fullDrawCode)
					break; // Reached an already marked ancestor, stop marking

				parent.fullDrawCode = fullDrawCode;
				pi = parent.parent;
			}
		}
	}

	return drawSun;
}

/*--------------------------------------
	rnd::CascadeDrawLists
--------------------------------------*/
void rnd::CascadeDrawLists(const com::Vec3& pos, float pitch, float yaw, const com::Vec3& fMin,
	const com::Vec3& fMax, com::Arr<const scn::Zone*>& litZones, size_t& numLitZones,
	com::Arr<const scn::Entity*>& litEnts, size_t& numLitEnts)
{
	static com::Arr<com::Plane> tempPlanes(8);
	static com::Poly tempClip, frsClip;
	int portalLineColor = (int)sunPortalColor.Float();
	int entLineColor = (int)sunEntityColor.Float();
	com::Vec3 fwd = com::VecFromPitchYaw(pitch, yaw);
	com::Vec3 left = com::VecFromPitchYaw(0.0f, yaw + COM_HALF_PI);
	com::Vec3 down = com::VecFromPitchYaw(pitch + COM_HALF_PI, yaw);
	com::Vec3 dir = -fwd;

	com::Plane frs[6] = {
		com::Plane(left, fMax.y + com::Dot(left, pos)), // left
		com::Plane(-left, -fMin.y + com::Dot(-left, pos)), // right
		com::Plane(down, -fMin.z + com::Dot(down, pos)), // bottom
		com::Plane(-down, fMax.z + com::Dot(-down, pos)), // top
		com::Plane(dir, -fMin.x + com::Dot(dir, pos)), // near
		com::Plane(fwd, fMax.x + com::Dot(fwd, pos)) // far
	};

	com::Vec3 nearVec = dir * -fMin.x;
	com::Vec3 leftVec = left * fMax.y;
	com::Vec3 rightVec = left * fMin.y;
	com::Vec3 downVec = down * -fMin.z;
	com::Vec3 upVec = down * -fMax.z;
	frsClip.SetVerts(0, 4);
	frsClip.verts[0] = pos + nearVec + leftVec + upVec;
	frsClip.verts[1] = pos + nearVec + leftVec + downVec;
	frsClip.verts[2] = pos + nearVec + rightVec + downVec;
	frsClip.verts[3] = pos + nearVec + rightVec + upVec;
	unsigned zoneCode = scn::IncZoneDrawCode();
	unsigned entCode = scn::Entity::IncHitCode();
	numLitZones = numLitEnts = 0;

	for(size_t cur = 0; cur < spd.NumStack(); cur++)
	{
		const port_desc_elem& s = spd.stack[cur];
		const com::Poly* cullPoly;
		const com::Plane* cullPlanes;
		size_t numCullPlanes;

		if(s.parent == -1)
		{
			cullPlanes = frs;
			numCullPlanes = 6;
			cullPoly = &frsClip;
		}
		else
		{
			tempClip = s.clip;

			if(!ClipPortal(tempClip, frs, 6))
				continue;

			tempPlanes.Ensure(tempClip.numVerts);
			GenerateOrthographicPlanes(dir, tempClip, tempPlanes.o);
			cullPlanes = tempPlanes.o;
			numCullPlanes = tempClip.numVerts;
			cullPoly = &tempClip;
		}

		DrawPortalLines(*cullPoly, portalLineColor, false);
		const scn::Zone& zone = *s.zone;

		if(zone.drawCode != zoneCode)
		{
			zone.drawCode = zoneCode;
			litZones.Ensure(numLitZones + 1);
			litZones[numLitZones++] = s.zone;
		}

		if(s.fullDrawCode != spd.FullDrawCode())
			continue;

		for(size_t j = 0; j < zone.numEntLinks; j++)
		{
			const scn::Entity* chain = zone.entLinks[j].obj;

			do
			{
				const scn::Entity& ent = *chain;

				if(ent.hitCode == entCode)
					continue;

				if(!(ent.flags & ent.SHADOW) || !ent.Mesh() || !ent.tex)
				{
					ent.hitCode = entCode;
					continue;
				}

				if(!ClipSphere<true>(ent.FinalPos(), ent.Mesh()->Radius() * ent.FinalScale(),
				cullPlanes, numCullPlanes, *cullPoly, &dir))
					continue;

				litEnts.Ensure(numLitEnts + 1);
				litEnts[numLitEnts++] = &ent;
				ent.hitCode = entCode;
				DrawEntityLines(ent, entLineColor, false);
			} while(chain = chain->Child());
		}
	}

	// Minimize state changes
	qsort(litEnts.o, numLitEnts, sizeof(const scn::Entity*), CmpShadowEntities);
}

/*--------------------------------------
	rnd::BulbDrawList

Makes list of entity shadows to draw for a bulb. overlays flags are OR'd if the bulb possibly
lights an overlay entity.
--------------------------------------*/
void rnd::BulbDrawList(const scn::Bulb& bulb, float radius, const com::Vec3& pos,
	com::Arr<const scn::Entity*>& litEnts, size_t& numLitEnts, uint16_t& overlays)
{
	numLitEnts = 0;
	unsigned entCode = scn::Entity::IncHitCode();

	for(size_t i = 0; i < bulb.NumZoneLinks(); i++)
	{
		const scn::Zone& zone = *bulb.ZoneLinks()[i].obj;

		for(size_t j = 0; j < zone.numEntLinks; j++)
		{
			const scn::Entity& top = *zone.entLinks[j].obj;

			if(top.hitCode == entCode)
				continue;

			top.hitCode = entCode;
			const scn::Entity* chain = &top;
			
			do
			{
				const scn::Entity& ent = *chain;
				const Mesh* msh = ent.Mesh();

				if(!msh || !ent.tex)
					continue; // Entity has no model

				float distSq = (ent.FinalPos() - pos).MagSq();
				float comp = msh->Radius() + radius;

				if(distSq > comp * comp)
					continue; // Mesh is not in bulb's radius

				// The entity may be lit by this bulb
				overlays |= ent.OverlayFlags();

				if(!(ent.flags & ent.SHADOW))
					continue; // Entity has no shadow

				// The entity may cast a shadow from this bulb
				litEnts.Ensure(numLitEnts + 1);
				litEnts[numLitEnts++] = &ent;
			} while(chain = chain->Child());
		}
	}

	// Minimize state changes
	qsort(litEnts.o, numLitEnts, sizeof(const scn::Entity*), CmpShadowEntities);
}

/*--------------------------------------
	rnd::BulbLitFlags

Like BulbDrawLists but only checks if bulb lights any entities with special lighting flags. For
bulbs that don't draw shadows.
--------------------------------------*/
void rnd::BulbLitFlags(const scn::Bulb& bulb, float radius, const com::Vec3& pos,
	uint16_t& overlays)
{
	unsigned entCode = scn::Entity::IncHitCode();

	for(size_t i = 0; i < bulb.NumZoneLinks(); i++)
	{
		const scn::Zone& zone = *bulb.ZoneLinks()[i].obj;

		for(size_t j = 0; j < zone.numEntLinks; j++)
		{
			const scn::Entity& top = *zone.entLinks[j].obj;

			if(top.hitCode == entCode)
				continue;

			top.hitCode = entCode;
			const scn::Entity* chain = &top;
			
			do
			{
				const scn::Entity& ent = *chain;
				uint16_t flags = ent.OverlayFlags();

				if(!flags)
					continue; // Entity has no lighting flags

				const Mesh* msh = ent.Mesh();

				if(!msh || !ent.tex)
					continue; // Entity has no model

				float distSq = (ent.FinalPos() - pos).MagSq();
				float comp = msh->Radius() + radius;

				if(distSq > comp * comp)
					continue; // Mesh is not in bulb's radius

				// The entity may be lit by this bulb
				overlays |= flags;
			} while(chain = chain->Child());
		}
	}
}

/*--------------------------------------
	rnd::PortalGap
--------------------------------------*/
float rnd::PortalGap(const scn::PortalSet& set, const com::Vec3& pos, bool fwd)
{
	float dist = com::PointPlaneDistance(pos, set.polys[0].pln);
	return fwd ? -dist : dist;
}

/*--------------------------------------
	rnd::PortalDot
--------------------------------------*/
float rnd::PortalDot(const scn::PortalSet& set, const com::Vec3& dir, bool fwd)
{
	float dist = com::Dot(set.polys[0].pln.normal, dir);
	return fwd ? -dist : dist;
}

/*--------------------------------------
	rnd::CameraFrustum

FIXME: need to support infinite far plane
--------------------------------------*/
void rnd::CameraFrustum(const scn::Camera& cam, hit::Frustum& frs, com::Plane* plns,
	com::Poly& clipOut)
{
	com::Vec3 camPos = cam.FinalPos();
	com::Qua camOri = cam.FinalOri();
	float camFOV = cam.FinalFOV();
	float vAngle = camFOV * 0.5f;
	float aspect = (float)wrp::VideoWidth() / wrp::VideoHeight();
	float hAngle = com::HorizontalFOV(camFOV, aspect) * 0.5f;
	frs.Set(hAngle, vAngle, nearClip.Float(), farClip.Float());
	const com::Vec3* axes = frs.Axes();

	for(size_t i = 0; i < 4; i++)
		plns[i] = com::PointPlane(com::VecRot(axes[i], camOri).Normalized(), camPos);

	const com::ClimbVertex* fVerts = frs.Vertices();
	clipOut.SetVerts(0, 4);
	clipOut.verts[0] = com::VecRot(fVerts[5].pos, camOri) + camPos;
	clipOut.verts[1] = com::VecRot(fVerts[7].pos, camOri) + camPos;
	clipOut.verts[2] = com::VecRot(fVerts[3].pos, camOri) + camPos;
	clipOut.verts[3] = com::VecRot(fVerts[1].pos, camOri) + camPos;
	clipOut.pln.normal = com::VecRot(com::Vec3(-1.0f, 0.0f, 0.0f), camOri);
	clipOut.pln.offset = com::Dot(clipOut.pln.normal, clipOut.verts[0]);
}

/*--------------------------------------
	rnd::ClipPortal
--------------------------------------*/
bool rnd::ClipPortal(com::Poly& clip, const com::Plane* plns, size_t num)
{
	for(size_t j = 0; j < num; j++)
	{
		int cut = com::CutPoly(clip, plns[j], clip);

		if(cut == 1)
		{
			clip.numVerts = 0;
			return false;
		}
	}
	
	return true;
}

/*--------------------------------------
	rnd::GeneratePerspectivePlanes
--------------------------------------*/
void rnd::GeneratePerspectivePlanes(const com::Vec3& pos, const com::Poly& clip,
	com::Plane* planes)
{
	for(size_t i = 0; i < clip.numVerts; i++)
	{
		com::Vec3 dir = pos - clip.verts[i];

		planes[i] = com::EdgePlane(clip.verts[i], clip.verts[(i + 1) % clip.numVerts], dir,
			false);
	}
}

/*--------------------------------------
	rnd::GenerateOrthographicPlanes
--------------------------------------*/
void rnd::GenerateOrthographicPlanes(const com::Vec3& dir, const com::Poly& clip,
	com::Plane* planes)
{
	for(size_t i = 0; i < clip.numVerts; i++)
	{
		planes[i] = com::EdgePlane(clip.verts[i], clip.verts[(i + 1) % clip.numVerts], dir,
			false);
	}
}

/*--------------------------------------
	rnd::ClipSphere

dirs is an array of normalized vectors pointing from each clip vertex to the viewer/light. If
orthographic is true, dirs is one vector.
--------------------------------------*/
template <bool orthographic>
bool rnd::ClipSphere(const com::Vec3& pos, float radius, const com::Plane* plns, size_t numPlns,
	const com::Poly& clip, const com::Vec3* dirs)
{
	// Check edge planes first, most likely to cull sphere
	for(size_t i = 0; i < clip.numVerts; i++)
	{
		if(com::PointPlaneDistance(pos, plns[i]) > radius)
			return false;
	}

	// Check portal's plane, may cull if zone we're looking into surrounds the camera
	if(com::PointPlaneDistance(pos, clip.pln) > radius)
		return false;

	// Clip with corner planes to cull spheres closest to an edge of the projected portal
	bool inFront = true;

	for(size_t i = 0; i < clip.numVerts; i++)
	{
		com::Plane temp;
		const com::Vec3& dir = dirs[orthographic ? 0 : i];
		float distToVertAlongDir = com::Dot(dir, clip.verts[i] - pos);
		temp.normal = (pos + dir * distToVertAlongDir - clip.verts[i]).Normalized();
		temp.offset = com::Dot(temp.normal, clip.verts[i]);

		if(distToVertAlongDir >= 0.0f)
			inFront = false;

		// Skip plane if it's facing the clipping polygon
		if(com::Dot(temp.normal, clip.verts[(i + 1) % clip.numVerts]) - temp.offset >= 0.0f ||
		com::Dot(temp.normal, clip.verts[COM_PREV(i, clip.numVerts)]) - temp.offset >= 0.0f)
			continue;

		if(com::Dot(pos, temp.normal) - temp.offset > radius)
			return false;
	}

	if(inFront)
	{
		// Sphere's center is in front of each vertex from camera's perspective
		/* The sphere may only be touching the space between the camera and portal, or is behind
		the camera but still behind all planes tested so far */
		// Get closest point on clip's edges and create a plane between it and sphere's center
		size_t bestVert = 0;
		com::Vec3 bestDiff;
		float bestSqDist = FLT_MAX;

		for(size_t i = 0; i < clip.numVerts; i++)
		{
			size_t head = (i + 1) % clip.numVerts;
			float t = com::ClosestSegmentFactor(clip.verts[i], clip.verts[head], pos);
			com::Vec3 diff = pos - COM_LERP(clip.verts[i], clip.verts[head], t);
			float sqDist = diff.MagSq();

			if(sqDist < bestSqDist)
			{
				bestSqDist = sqDist;
				bestDiff = diff;
				bestVert = t <= 0.5f ? i : head;
			}
		}

		if(bestSqDist > 0.0f)
		{
			float dist = sqrt(bestSqDist);

			if(dist > radius)
			{
				// Sphere and portal are separated by plane
				// Check if plane is valid, i.e. not facing the clipping polygon
					// Otherwise, the sphere is actually closest to the interior of the poly
				com::Plane temp = com::PointPlane(bestDiff / dist, clip.verts[bestVert]);

				if(com::Dot(temp.normal, clip.verts[(bestVert + 1) % clip.numVerts]) - temp.offset < 0.0f &&
				com::Dot(temp.normal, clip.verts[COM_PREV(bestVert, clip.numVerts)]) - temp.offset < 0.0f)
					return false;
			}
		}
	}

	return true;
}

/*--------------------------------------
	rnd::AddVisibleZoneObjects
--------------------------------------*/
void rnd::AddVisibleZoneObjects(const scn::Zone& zone, const com::Poly& clip,
	const com::Plane* plns, const com::Vec3& cam, unsigned entCode, unsigned bulbCode)
{
	int entLineColor = (int)entityColor.Float();
	static com::Arr<com::Vec3> dirs(8);
	dirs.Ensure(clip.numVerts);

	for(size_t i = 0; i < clip.numVerts; i++)
		dirs[i] = (cam - clip.verts[i]).Normalized();

	for(size_t i = 0; i < zone.numEntLinks; i++)
	{
		const scn::Entity* chain = zone.entLinks[i].obj;

		do
		{
			const scn::Entity& ent = *chain;

			if(ent.hitCode == entCode)
				continue;

			if(((ent.flags & (ent.VISIBLE | ent.WORLD_VISIBLE)) !=
			(ent.VISIBLE | ent.WORLD_VISIBLE)) || !ent.Mesh() || !ent.tex)
			{
				ent.hitCode = entCode;
				continue;
			}

			if(!ClipSphere<false>(ent.FinalPos(), ent.Mesh()->Radius() * ent.FinalScale(), plns,
			clip.numVerts, clip, dirs.o))
				continue;

			AddVisibleEntity(ent);
			ent.hitCode = entCode;
			DrawEntityLines(ent, entLineColor, false);
		} while(chain = chain->Child());
	}

	for(size_t i = 0; i < zone.numBulbLinks; i++)
	{
		const scn::Bulb& bulb = *zone.bulbLinks[i].obj;

		if(bulb.drawCode == bulbCode)
			continue;

		float radius = bulb.FinalRadius();
		float outer;

		if(!radius || !bulb.FinalIntensity() || !(outer = bulb.FinalOuter()))
		{
			bulb.drawCode = bulbCode;
			continue;
		}
		
		com::Vec3 spherePos = bulb.FinalPos();
		float sphereRadius = radius;

		if(!ClipSphere<false>(spherePos, sphereRadius, plns, clip.numVerts, clip, dirs.o))
			continue;

		if(outer < COM_PI && tighterSpotCull.Bool())
		{
			// Clip a second, smaller but forward-offset sphere for spot lights
			float sphereOffset;
			MinConeSphere(radius, outer, sphereRadius, sphereOffset);
			spherePos += bulb.FinalOri().Dir() * sphereOffset;

			if(!ClipSphere<false>(spherePos, sphereRadius, plns, clip.numVerts, clip, dirs.o))
				continue;
		}

		AddVisibleBulb(bulb);
		bulb.drawCode = bulbCode;

		DrawSphereLines(spherePos, com::QUA_IDENTITY, sphereRadius, (int)bulbColor.Float(),
			false);
	}
}

/*--------------------------------------
	rnd::AddVisibleEntity
--------------------------------------*/
void rnd::AddVisibleEntity(const scn::Entity& ent)
{
	if(ent.Glass())
	{
		visGlassEnts.Ensure(numVisGlassEnts + 1);
		visGlassEnts[numVisGlassEnts++] = &ent;
	}
	else if((ent.flags & ent.CLOUD) && (ent.Mesh()->Flags() & Mesh::VOXELS))
	{
		visCloudEnts.Ensure(numVisCloudEnts + 1);
		visCloudEnts[numVisCloudEnts++] = &ent;
	}
	else if(ent.FinalOpacity() != 0.0f)
	{
		visEnts.Ensure(numVisEnts + 1);
		visEnts[numVisEnts++] = &ent;
	}
}

/*--------------------------------------
	rnd::AddVisibleBulb
--------------------------------------*/
void rnd::AddVisibleBulb(const scn::Bulb& bulb)
{
	if(bulb.FinalOuter() >= COM_PI)
	{
		visBulbs.Ensure(numVisBulbs + 1);
		visBulbs[numVisBulbs++] = &bulb;
	}
	else
	{
		visSpots.Ensure(numVisSpots + 1);
		visSpots[numVisSpots++] = &bulb;
	}
}

/*--------------------------------------
	rnd::MinConeSphere
--------------------------------------*/
void rnd::MinConeSphere(float origRadius, float angle, float& radius, float& offset)
{
	if(angle >= COM_HALF_PI)
	{
		radius = origRadius;
		offset = 0.0f;
		return;
	}

	float s = sin(angle);

	if(angle >= COM_PI * 0.25f)
	{
		radius = origRadius * s;
		offset = origRadius * sqrt(1.0f - s * s); // * cos(angle);
	}
	else if(angle > 0.0f)
		offset = radius = origRadius * s / sin(angle * 2.0f); // Isosceles triangle in a circle
	else
		offset = radius = origRadius * 0.5f;
}

/*--------------------------------------
	rnd::CmpEntityModels

Sort by mesh then texture.
--------------------------------------*/
int rnd::CmpEntityModels(const void* av, const void* bv)
{
	const scn::Entity* a = *(const scn::Entity**)av;
	const MeshGL* aMsh = (MeshGL*)a->Mesh();
	const TextureGL* aTex = (TextureGL*)a->tex.Value();
	const scn::Entity* b = *(const scn::Entity**)bv;
	const MeshGL* bMsh = (MeshGL*)b->Mesh();
	const TextureGL* bTex = (TextureGL*)b->tex.Value();

	if(aMsh->vBufName == bMsh->vBufName)
	{
		if(aTex->texName == bTex->texName)
			return 0;

		return aTex->texName < bTex->texName ? -1 : 1;
	}
	else
		return aMsh->vBufName < bMsh->vBufName ? -1 : 1;
}

/*--------------------------------------
	rnd::DrawPortalLines
--------------------------------------*/
void rnd::DrawPortalLines(const com::Poly& poly, int color, float time)
{
	if(color < 0)
		return;

	for(size_t j = 0; j < poly.numVerts; j++)
	{
		DrawLine(poly.verts[j], poly.verts[(j + 1) % poly.numVerts], color, time);
		DrawLine(poly.verts[j], poly.verts[j] + poly.pln.normal * 16.0f, color, time);
	}
}

/*--------------------------------------
	rnd::DrawEntityLines
--------------------------------------*/
void rnd::DrawEntityLines(const scn::Entity& ent, int color, float time)
{
	if(color < 0)
		return;

	if(ent.Mesh())
	{
		DrawSphereLines(ent.FinalPos(), com::QUA_IDENTITY,
			ent.Mesh()->Radius() * ent.FinalScale(), color, time);
	}
}

/*--------------------------------------
	rnd::DrawBulbLines
--------------------------------------*/
void rnd::DrawBulbLines(const scn::Bulb& bulb, float time)
{
	int color = (int)bulbColor.Float();

	if(color < 0)
		return;

	DrawSphereLines(bulb.FinalPos(), com::QUA_IDENTITY, bulb.FinalRadius(), color, time);
}