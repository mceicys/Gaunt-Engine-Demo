// scene_entity.cpp
// Martynas Ceicys

#include "scene.h"
#include "scene_lua.h"
#include "scene_private.h"
#include "../../GauntCommon/json_ext.h"
#include "../hit/hit.h"
#include "../render/render.h"

/*
################################################################################################


	ENTITY


################################################################################################
*/

const char* const res::Resource<scn::Entity, true>::METATABLE = "metaEntity";

/*--------------------------------------
	scn::Entity::Entity

Initializes entity's placement and old placement to given values. Creates a userdata and creates
a reference to it in the Lua registry.

NOTE: If default values change here, change save code, too.
--------------------------------------*/
scn::Entity::Entity(EntityType* et, const com::Vec3& p, const com::Qua& o)
{
	type = et;
	oldPos = p;
	oldOri = o;
	oldScale = scale = 1.0f;
	uv = oldUV = 0.0f;
	subPalette = 0;
	opacity = oldOpacity = 1.0f;
	animSpeed = 1.0f;

	flags = VISIBLE | WORLD_VISIBLE | SHADOW | LERP_POS | LERP_ORI | LERP_SCALE | LERP_UV |
		LERP_OPACITY | LERP_FRAME;

	pFlags = 0;
	leafLinks.Init(Entity::DEF_NUM_LEAF_LINKS_ALLOC);
	zoneLinks.Init(Entity::DEF_NUM_ZONE_LINKS_ALLOC);
	numLeafLinks = numZoneLinks = 0;
	frame = 0.0f;
	midA = midB = 0;
	midF = transTime = 0.0f;
	hitCode = 0;
	SetPlace(p, o);
	gFlags.AddLock(); // Permanent lock
	LuaPush(); // Create userdata
	lua_createtable(scr::state, 0, 0); // Create uservalue table
	lua_setuservalue(scr::state, -2);
	lua_pop(scr::state, 1); // Pop entity
}

/*--------------------------------------
	scn::Entity::~Entity

Must only be called thru RemoveLock, and only after this has been KillEntity'd
--------------------------------------*/
scn::Entity::~Entity()
{
	leafLinks.Free();
	zoneLinks.Free();
}

/*--------------------------------------
	scn::Entity::DummyCopy

Does an incomplete copy of src to help create/update a prediction dummy. This is a hack for
dummy.lua.

FIXME: remove?
--------------------------------------*/
void scn::Entity::DummyCopy(const Entity& src)
{
	static const uint16_t COPY_P_FLAGS = P_ORI_HULL | P_POINT_LINK;
	const Entity& s = (Entity&)src;

	UnsetLinkingObjects();

	// Public
	oldPos = src.oldPos;
	oldOri = src.oldOri;
	// Not copying type
	gFlags = src.gFlags;
	tex.Set(src.tex);
	subPalette = src.subPalette;
	opacity = src.opacity;
	oldOpacity = src.oldOpacity;
	animSpeed = src.animSpeed;
	// Not copying hitCode
	flags = src.flags & ~(LERP_POS | LERP_ORI | LERP_OPACITY | LERP_FRAME);

	// Private
	pos = src.pos;
	ori = src.ori;
	// Not copying leafLinks, numLeafLinks, zoneLink, numZoneLinks
	msh = src.msh;
	hull = src.hull;
	linkHull = src.linkHull;
	// Not copying child
	anim = src.anim;
	frame = src.frame;
	midA = src.midA;
	midB = src.midB;
	midF = src.midF;
	transTime = src.transTime;
	pFlags = (pFlags & ~COPY_P_FLAGS) | (src.pFlags & COPY_P_FLAGS);

	LinkWorld();
}

/*--------------------------------------
	scn::Entity::SetPos
--------------------------------------*/
void scn::Entity::SetPos(const com::Vec3& p)
{
	if(pos == p)
		return;

	pos = p;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Entity::FinalPos
--------------------------------------*/
com::Vec3 scn::Entity::FinalPos() const
{
	if(flags & LERP_POS)
		return COM_LERP(oldPos, pos, wrp::Fraction());
	else
		return pos;
}

/*--------------------------------------
	scn::Entity::SetOri
--------------------------------------*/
void scn::Entity::SetOri(const com::Qua& o)
{
	// FIXME: link mesh-only entities with an AABB to avoid relink here
	if(!linkHull && ((pFlags & P_ORI_HULL) != 0 || (!hull && msh)))
	{
		if(ori == o)
			return;

		ori = o;
		UnlinkWorld();
		LinkWorld();
	}
	else
		ori = o;
}

/*--------------------------------------
	scn::Entity::FinalOri
--------------------------------------*/
com::Qua scn::Entity::FinalOri() const
{
	if(flags & LERP_ORI && oldOri != ori)
		return com::Slerp(oldOri, ori, wrp::Fraction()).Normalized();
	else
		return ori;
}

/*--------------------------------------
	scn::Entity::SetPlace

Allows both position and orientation to be set without doing two world links.
--------------------------------------*/
void scn::Entity::SetPlace(const com::Vec3& p, const com::Qua& o)
{
	if(pos == p && (linkHull || !hull || ((pFlags & P_ORI_HULL) == 0) || ori == o))
	{
		ori = o;
		return;
	}

	pos = p;
	ori = o;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Entity::SetScale
--------------------------------------*/
void scn::Entity::SetScale(float s)
{
	if(s < 0.0f)
		s = 0.0f;

	if(scale == s)
		return;

	scale = s;

	if(!linkHull && !hull) // Relink if using mesh's hull
	{
		UnlinkWorld();
		LinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::FinalScale
--------------------------------------*/
float scn::Entity::FinalScale() const
{
	if(flags & LERP_SCALE)
		return COM_LERP(oldScale, scale, wrp::Fraction());
	else
		return scale;
}

/*--------------------------------------
	scn::Entity::SetScaledPlace
--------------------------------------*/
void scn::Entity::SetScaledPlace(const com::Vec3& p, const com::Qua& o, float s)
{
	if(s < 0.0f)
		s = 0.0f;

	if(pos == p &&
	(linkHull || !hull || ((pFlags & P_ORI_HULL) == 0) || ori == o) &&
	(linkHull || hull || !msh || scale == s))
	{
		ori = o;
		scale = s;
		return;
	}

	pos = p;
	ori = o;
	scale = s;
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Entity::SetOrientHull
--------------------------------------*/
void scn::Entity::SetOrientHull(bool o)
{
	pFlags = com::FixedBits(pFlags, P_ORI_HULL, o);

	if(!linkHull && hull)
	{
		UnlinkWorld();
		LinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::FinalUV
--------------------------------------*/
com::Vec2 scn::Entity::FinalUV() const
{
	if(flags & LERP_UV)
		return COM_LERP(oldUV, uv, wrp::Fraction());
	else
		return uv;
}

/*--------------------------------------
	scn::Entity::SetMesh
--------------------------------------*/
void scn::Entity::SetMesh(rnd::Mesh* m)
{
	if(m == msh)
		return;

	ClearAnimation();
	msh.Set(m);

	if(!linkHull && !hull)
	{
		UnlinkWorld();
		LinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::SetHull
--------------------------------------*/
void scn::Entity::SetHull(hit::Hull* h)
{
	if(h == hull)
		return;

	hull.Set(h);

	if(!linkHull)
	{
		UnlinkWorld();
		LinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::SetLinkHull
--------------------------------------*/
void scn::Entity::SetLinkHull(hit::Hull* h)
{
	if(h == linkHull)
		return;

	linkHull.Set(h);
	UnlinkWorld();
	LinkWorld();
}

/*--------------------------------------
	scn::Entity::UnsetLinkingObjects
--------------------------------------*/
void scn::Entity::UnsetLinkingObjects()
{
	linkHull.Set(0);
	hull.Set(0);
	ClearAnimation();
	msh.Set(0);
	UnlinkWorld();
}

/*--------------------------------------
	scn::Entity::Average
--------------------------------------*/
com::Vec3 scn::Entity::Average() const
{
	if(!hull)
		return pos;

	if(pFlags & P_ORI_HULL && !ori.Identity())
		return com::VecRot(hit::Average(*hull), ori) + pos;
	else
		return hit::Average(*hull) + pos;
}

/*--------------------------------------
	scn::Entity::FinalOpacity
--------------------------------------*/
float scn::Entity::FinalOpacity() const
{
	if(flags & LERP_OPACITY)
		return COM_LERP(oldOpacity, opacity, wrp::Fraction());
	else
		return opacity;
}

/*--------------------------------------
	scn::Entity::Glass
--------------------------------------*/
bool scn::Entity::Glass() const
{
	return (flags & (REGULAR_GLASS | AMBIENT_GLASS | MINIMUM_GLASS)) != 0;
}

/*--------------------------------------
	scn::Entity::SetAnimation

After trans seconds, a begins playing at relative frame f.
--------------------------------------*/
void scn::Entity::SetAnimation(const rnd::Animation& a, float f, float trans)
{
	if(a.mesh != msh)
	{
		CON_ERROR("Tried to set animation not belonging to the entity's mesh");
		return;
	}

	// Start transition
	if(!transTime)
	{
		rnd::AbsFrame(anim, frame, midA, midB, midF);

		if(animSpeed < 0.0f)
		{
			com::Swap(midA, midB);
			midF = 1.0f - midF;
		}
	}

	transTime = trans;

	// Set target
	anim.Set(&a);
	frame = f;
	float len = float(a.end - a.start);

	if(frame < 0.0f)
		frame = 0.0f;
	else if(flags & LOOP_ANIM)
	{
		if(frame > len + 1.0f)
			frame = len + 1.0f;
	}
	else if(frame > len)
		frame = len;
}

/*--------------------------------------
	scn::Entity::UpdateAnimation

Advances animation by one tick.

FIXME: Test 1 and 2 frame animations
--------------------------------------*/
void scn::Entity::UpdateAnimation()
{
	rnd::AdvanceTrack(wrp::timeStep.Float(), anim, frame, animSpeed, (flags & LOOP_ANIM) != 0,
		transTime, midA, midB, midF);
}

/*--------------------------------------
	scn::Entity::ClearAnimation
--------------------------------------*/
void scn::Entity::ClearAnimation()
{
	anim.Set(0);
	frame = 0.0f;
	midA = midB = 0;
	midF = transTime = 0.0f;
}

/*--------------------------------------
	scn::Entity::AnimationPlaying
--------------------------------------*/
bool scn::Entity::AnimationPlaying() const
{
	return anim.Value() && (
		(flags & LOOP_ANIM) ||
		transTime ||
		(animSpeed >= 0.0f && frame < anim->end - anim->start) ||
		(animSpeed < 0.0f && frame > 0.0f)
	);
}

/*--------------------------------------
	scn::Entity::UpcomingFrame

Returns true if target frame will be reached this tick. False if already on target.
--------------------------------------*/
bool scn::Entity::UpcomingFrame(float target) const
{
	float time = wrp::timeStep.Float() - transTime;

	// Can't reach target if not advancing an animation
	if(!anim || time <= 0.0f)
		return false;

	float advance = anim->mesh->FrameRate() * animSpeed * time;
	float future = frame + advance;
	float bound = anim->end - anim->start + 1;

	if(advance >= 0.0f)
	{
		if(frame >= target)
		{
			if(transTime && frame == target)
				return true; // About to finish transition to target

			if((flags & LOOP_ANIM) == 0)
				return false; // Already at or past target and not looping

			return future - bound >= target;
		}

		return future >= target;
	}
	else
	{
		if(frame <= target)
		{
			if(transTime && frame == target)
				return true;

			if((flags & LOOP_ANIM) == 0)
				return false;

			return future + bound <= target;
		}

		return future <= target;
	}
}

/*--------------------------------------
	scn::Entity::Overlay

Returns overlay index or -1 for none.
--------------------------------------*/
int scn::Entity::Overlay() const
{
	return -1 + int(OverlayFlags());
}

/*--------------------------------------
	scn::Entity::SetOverlay
--------------------------------------*/
void scn::Entity::SetOverlay(int overlay)
{
	int cur = Overlay();

	if(cur == overlay)
		return;

	if(cur != -1)
	{
		pFlags &= ~(P_OVERLAY_0 | P_OVERLAY_1);
		Overlays()[cur].RemoveEntity(*this);
	}

	if(overlay < 0)
		return;

	if((size_t)overlay >= NUM_OVERLAYS)
		CON_ERRORF("Out of bounds overlay index %d", overlay);

	Overlays()[overlay].AddEntity(*this);
	pFlags |= P_OVERLAY_0 << overlay;
}

/*--------------------------------------
	scn::Entity::SetChild
--------------------------------------*/
void scn::Entity::SetChild(Entity* other)
{
	if(other == child)
		return;

	if(other == this)
		other = 0;

	if(other && (other->pFlags & P_CHILD))
	{
		con::AlertF("Tried to set '%s' entity's child to another entity's '%s' child",
			TypeName(), other->TypeName());
		return;
	}

	if(child && child->Alive())
	{
		child->pFlags &= ~P_CHILD;
		child->LinkWorld();
	}

	child.Set(other);

	if(other)
	{
		other->pFlags |= P_CHILD;
		other->UnlinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::SetPointLink
--------------------------------------*/
void scn::Entity::SetPointLink(bool point)
{
	if(((pFlags & P_POINT_LINK) != 0) == point)
		return;

	pFlags = com::FixedBits(pFlags, P_POINT_LINK, point);

	if(!linkHull && !hull && !msh)
	{
		UnlinkWorld();
		LinkWorld();
	}
}

/*--------------------------------------
	scn::Entity::SetSky
--------------------------------------*/
void scn::Entity::SetSky(bool sky)
{
	if(((pFlags & P_SKY_OVERLAY) != 0) == sky)
		return;

	if(sky)
	{
		pFlags |= P_SKY_OVERLAY;
		SkyOverlay().AddEntity(*this);
	}
	else
	{
		pFlags &= ~P_SKY_OVERLAY;
		SkyOverlay().RemoveEntity(*this);
	}
}

/*--------------------------------------
	scn::Entity::MimicMesh
--------------------------------------*/
void scn::Entity::MimicMesh(const Entity& orig)
{
	SetMesh(0);
	MimicScaledPlace(orig);
	SetMesh(orig.Mesh());
	flags ^= (flags ^ orig.flags) & LOOP_ANIM;
	animSpeed = orig.animSpeed;
	anim = orig.anim;
	frame = orig.frame;
	midA = orig.midA;
	midB = orig.midB;
	midF = orig.midF;
	transTime = orig.transTime;
}

/*--------------------------------------
	scn::Entity::MimicScaledPlace
--------------------------------------*/
void scn::Entity::MimicScaledPlace(const Entity& orig)
{
	SetPlace(orig.Pos(), orig.Ori());
	oldPos = orig.oldPos;
	oldOri = orig.oldOri;
	SetScale(orig.Scale());
	oldScale = orig.oldScale;
}

/*--------------------------------------
	scn::Entity::UnsafeCall

Calls entity's type's function with the entity and numArgs Lua stack values as the arguments.
Assumes the entity is alive, the function exists, and the Lua stack has enough space.
--------------------------------------*/
void scn::Entity::UnsafeCall(int func, int numArgs)
{
	lua_rawgeti(scr::state, LUA_REGISTRYINDEX, type->Refs()[func]);
	LuaPush();

	if(numArgs)
	{
		for(int i = 0; i < 2; i++)
			lua_insert(scr::state, -2 - numArgs);
	}

	scr::Call(scr::state, 1 + numArgs, 0);
}

/*--------------------------------------
	scn::Entity::Call
--------------------------------------*/
void scn::Entity::Call(int func, int numArgs)
{
	if(!Alive() || !type || !type->HasRef(func))
	{
		lua_pop(scr::state, numArgs);
		return;
	}

	scr::EnsureStack(scr::state, 2);
	UnsafeCall(func, numArgs);
}

/*--------------------------------------
	scn::Entity::LinkWorld

Links entity to leaves its hull or, if the entity has no hull, its mesh's hull is in.
--------------------------------------*/
void scn::Entity::LinkWorld()
{
	if((pFlags & P_CHILD) != 0)
		return;

	static hit::Hull box(0.0f, 0.0f);
	const hit::Hull* actHull = 0;
	com::Qua hullOri = com::QUA_IDENTITY;

	if(linkHull)
		actHull = linkHull;
	else if(hull)
	{
		actHull = hull;
		hullOri = HullOri();
	}
	else if(msh)
	{
		float rad = msh->Radius() * scale;
		box.SetBox(-rad, rad);
		actHull = &box;
	}

	if(actHull)
	{
		unsigned zoneCode = IncZoneDrawCode();
		hit::Descent& d = hit::GlobalDescent();
		d.Begin(WorldRoot(), pos, pos);

		while(d.numStacked)
		{
			WorldNode& topNode = (WorldNode&)*d.s[d.numStacked - 1].node;
		
			if(!topNode.left && !topNode.solid)
			{
				LinkObject<Entity, WorldNode>(*this, &Entity::leafLinks, &Entity::numLeafLinks,
					topNode, &WorldNode::entLinks, &WorldNode::numEntLinks);

				Zone& zone = *topNode.zone;

				if(zone.drawCode != zoneCode)
				{
					LinkObject<Entity, Zone>(*this, &Entity::zoneLinks, &Entity::numZoneLinks,
						zone, &Zone::entLinks, &Zone::numEntLinks);

					zone.drawCode = zoneCode;
				}
			}

			d.Descend(*actHull, hullOri);
		}
	}
	else if(pFlags & P_POINT_LINK)
	{
		WorldNode& leaf = (WorldNode&)*PosToLeaf(WorldRoot(), pos);
			
		if(!leaf.solid)
		{
			LinkObject<Entity, WorldNode>(*this, &Entity::leafLinks, &Entity::numLeafLinks,
				leaf, &WorldNode::entLinks, &WorldNode::numEntLinks);

			LinkObject<Entity, Zone>(*this, &Entity::zoneLinks, &Entity::numZoneLinks,
				*leaf.zone, &Zone::entLinks, &Zone::numEntLinks);
		}
	}
}

/*--------------------------------------
	scn::Entity::UnlinkWorld

FIXME: corrupts heap if world is cleared without deleting entities
--------------------------------------*/
void scn::Entity::UnlinkWorld()
{
	UnlinkObjects<Entity, WorldNode>(*this, &Entity::leafLinks, &Entity::numLeafLinks,
		&WorldNode::entLinks, &WorldNode::numEntLinks);

	UnlinkObjects<Entity, Zone>(*this, &Entity::zoneLinks, &Entity::numZoneLinks,
		&Zone::entLinks, &Zone::numEntLinks);
}

/*--------------------------------------
	scn::Entity::Transcribe

FIXME: Be aware once writing new save and load code, there's a lot of new stuff that's not being
transcibed here
	e.g. animation stuff, flag uints, etc.
--------------------------------------*/
void scn::Entity::Transcribe()
{
	if(!Transcript())
		return;

	com::PairMap<com::JSVar>& pairs = *Transcript()->SetObject();

	if(type)
		pairs.Ensure("type")->Value().SetString(type->Name());

	com::JSVarSetVec(pairs.Ensure("pos")->Value(), Pos());

	if(oldPos != Pos())
		com::JSVarSetVec(pairs.Ensure("oldPos")->Value(), oldPos);

	if(!Ori().Identity())
		com::JSVarSetQua(pairs.Ensure("ori")->Value(), Ori());

	if(oldOri != Ori())
		com::JSVarSetQua(pairs.Ensure("oldOri")->Value(), oldOri);

	if(subPalette)
		pairs.Ensure("subPalette")->Value().SetNumber(subPalette);

	if(!(flags & LERP_POS))
		pairs.Ensure("lerpPos")->Value().SetBool((flags & LERP_POS) != 0);

	if(!(flags & LERP_ORI))
		pairs.Ensure("lerpOri")->Value().SetBool((flags & LERP_ORI) != 0);

	if(pFlags & P_ORI_HULL)
		pairs.Ensure("orientHull")->Value().SetBool(OrientHull());

	if(char* flagStr = gFlags.ToStr())
	{
		pairs.Ensure("flags")->Value().SetString(flagStr);
		delete[] flagStr;
	}

	if(Hull() && Hull()->Name())
		pairs.Ensure("hull")->Value().SetString(Hull()->Name());

	if(msh)
		pairs.Ensure("mesh")->Value().SetString(msh->FileName());

	if(tex)
		pairs.Ensure("texture")->Value().SetString(tex->FileName());
}

/*--------------------------------------
	scn::Entity::InterpretTranscript

FIXME: Interpret animation and playback keys
	Make sure to check that playback data is consistent (frame in range, etc.)
--------------------------------------*/
void scn::Entity::InterpretTranscript()
{
	com::PairMap<com::JSVar>* pairs;

	if(!Transcript() || !(pairs = Transcript()->Object()))
		return;

	com::Pair<com::JSVar>* pair = 0;

	if(pair = pairs->Find("orientHull"))
		SetOrientHull(pair->Value().Bool());

	com::Vec3 pos = 0.0f;
	com::Qua ori = com::QUA_IDENTITY;
	float scale = 1.0f;

	if(pair = pairs->Find("pos"))
		pos = com::JSVarToVec3(pair->Value());

	if(pair = pairs->Find("ori"))
		ori = com::JSVarToOri(pair->Value());

	if(pair = pairs->Find("scale"))
		scale = pair->Value().Float();

	SetScaledPlace(pos, ori, scale);

	if(pair = pairs->Find("oldPos"))
		oldPos = com::JSVarToVec3(pair->Value());
	else
		oldPos = Pos();

	if(pair = pairs->Find("oldOri"))
		oldOri = com::JSVarToOri(pair->Value());
	else
		oldOri = Ori();

	if(pair = pairs->Find("oldScale"))
		oldScale = pair->Value().Float();
	else
		oldScale = Scale();

	if(pair = pairs->Find("subPalette"))
		subPalette = pair->Value().Int();

	if(pair = pairs->Find("cloud"))
		flags = com::FixedBits(flags, CLOUD, pair->Value().Bool());

	if(pair = pairs->Find("lerpPos"))
		flags = com::FixedBits(flags, LERP_POS, pair->Value().Bool());

	if(pair = pairs->Find("lerpOri"))
		flags = com::FixedBits(flags, LERP_ORI, pair->Value().Bool());

	if(pair = pairs->Find("flags"))
		gFlags.StrToFlagSet(pair->Value().String());

	if(pair = pairs->Find("hull"))
		SetHull(hit::EnsureHull(pair->Value().String()));

	if(pair = pairs->Find("mesh"))
		SetMesh(rnd::EnsureMesh(pair->Value().String()));

	if(pair = pairs->Find("texture"))
		tex.Set(rnd::EnsureTexture(pair->Value().String()));
}

/*--------------------------------------
	scn::Entity::SaveOld
--------------------------------------*/
void scn::Entity::SaveOld()
{
	oldPos = pos;
	oldOri = ori;
	oldOpacity = opacity;
	UpdateAnimation();
}

/*--------------------------------------
	scn::CreateEntity
--------------------------------------*/
scn::Entity* scn::CreateEntity(EntityType* et, const com::Vec3& pos, const com::Qua& ori)
{
	return new Entity(et, pos, ori);
}

/*--------------------------------------
	scn::KillEntity
--------------------------------------*/
void scn::KillEntity(Entity& ent)
{
	if(ent.pFlags & Entity::P_DYING)
		return; // Already killed or being killed, prevent infinite recursion

	ent.pFlags |= Entity::P_DYING;

	if(ent.Alive())
	{
		ent.Call(ENT_FUNC_TERM);

		// Set uservalue to nil so referenced usertable can be collected, breaking ref loop
			/* i.e. Entity userdata references usertable which references Navigator which
			references Entity userdata which... */
		ent.LuaPush();
		lua_pushnil(scr::state);
		lua_setuservalue(scr::state, -2);
		lua_pop(scr::state, 1);

		// Disable and disappear
		ent.Kill();
		ent.SetChild(0);
		ent.SetSky(false);
		ent.SetOverlay(-1);
		ent.flags &= ~(ent.VISIBLE | ent.WORLD_VISIBLE | ent.SHADOW); // Necessary because ent might be a child
		ent.UnlinkWorld();

		ent.RemoveLock(); // Remove lifetime lock, might delete
	}
}

/*--------------------------------------
	scn::ClearEntities
--------------------------------------*/
void scn::ClearEntities()
{
	for(com::linker<Entity>* it = Entity::List().f, *next; it; it = next)
	{
		Entity& ent = *it->o;
		ent.AddLock(); // So next pointer can be retrieved after killing this entity
		KillEntity(ent);
		next = it->next;
		ent.RemoveLock(); // ~Entity might be called here
	}
}

/*--------------------------------------
	scn::InterpretEntities
--------------------------------------*/
void scn::InterpretEntities(com::PairMap<com::JSVar>& entities)
{
	for(com::Pair<com::JSVar>* it = entities.First(); it; it = it->Next())
	{
		com::PairMap<com::JSVar>* pairs = it->Value().Object();
		com::Pair<com::JSVar>* pair = 0;
		scn::EntityType* type = 0;

		if(pairs && (pair = pairs->Find("type")))
		{
			if(pair->Value().Type() != com::JSVar::STRING)
				CON_ERROR("Non-string entity type");
			else if(!(type = scn::FindEntityType(pair->Value().String())))
				CON_ERRORF("No entity type '%s'", pair->Value().String());
		}

		scn::Entity& ent = *scn::CreateEntity(type, 0.0f, com::QUA_IDENTITY);
		ent.SetRecordID(it->Key(), true);
		ent.SetTranscriptPtr(&it->Value());
		ent.InterpretTranscript();
		ent.Call(ENT_FUNC_INIT);
	}
}