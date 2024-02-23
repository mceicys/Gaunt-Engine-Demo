// render_mesh_extra.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "../vector/vec_lua.h"
#include "../quaternion/qua_lua.h"

/*
################################################################################################


	SOCKET


################################################################################################
*/

const char* const rnd::Socket::METATABLE = "metaSocket";

/*--------------------------------------
	rnd::Socket::Place

This is mainly for debugging, it's useless without animation transition parameters.
--------------------------------------*/
com::place rnd::Socket::Place(const Animation* ani, float frame) const
{
	if(!CheckAnimation(ani))
	{
		const com::place ZERO = {0.0f, com::QUA_IDENTITY};
		return ZERO;
	}

	size_t a, b;
	float f;
	AbsFrame(ani, frame, a, b, f);
	return LerpedPlace(values[a], values[b], f);
}

/*--------------------------------------
	rnd::Socket::PlaceEntity

Returns socket's world placement on ent at fraction [0, 1] between the start and end of the
current tick.
--------------------------------------*/
com::place rnd::Socket::PlaceEntity(const scn::Entity& ent, float fraction) const
{
	const Animation* ani = ent.Animation();

	if(!CheckAnimation(ani))
	{
		const com::place ZERO = {0.0f, com::QUA_IDENTITY};
		return ZERO;
	}

	float frame = ent.Frame();
	size_t a = ent.TransitionA(), b = ent.TransitionB();
	float f = ent.TransitionF(), trans = ent.TransitionTime();
	AdvanceTrack(wrp::timeStep.Float() * fraction, ani, frame, ent.animSpeed,
		(ent.flags & ent.LOOP_ANIM) != 0, trans, a, b, f);

	com::Vec3 pos = (ent.flags & ent.LERP_POS) != 0 ? COM_LERP(ent.oldPos, ent.Pos(), fraction) : ent.Pos();
	com::Qua ori = (ent.flags & ent.LERP_ORI) != 0 ? com::Slerp(ent.oldOri, ent.Ori(), fraction).Normalized() : ent.Ori();
	com::place soc = com::LerpedPlace(values[a], values[b], f);
	soc.pos = com::VecRot(soc.pos, ori) + pos;
	soc.ori = ori * soc.ori;
	return soc;
}

/*--------------------------------------
	rnd::Socket::PlaceAlign

Returns ent's world placement so the socket is aligned with p and o at the start of the latest
tick + time.
--------------------------------------*/
com::place rnd::Socket::PlaceAlign(const com::Vec3& p, const com::Qua& o,
	const scn::Entity& ent, float time) const
{
	const Animation* ani = ent.Animation();

	if(!CheckAnimation(ani))
	{
		const com::place ZERO = {0.0f, com::QUA_IDENTITY};
		return ZERO;
	}

	float frame = ent.Frame();
	size_t a = ent.TransitionA(), b = ent.TransitionB();
	float f = ent.TransitionF(), trans = ent.TransitionTime();
	AdvanceTrack(time, ani, frame, ent.animSpeed, (ent.flags & ent.LOOP_ANIM) != 0, trans, a, b,
		f);

	com::place ret = com::LerpedPlace(values[a], values[b], f);
	ret.ori = o * ret.ori.Conjugate();
	ret.pos = p - com::VecRot(ret.pos, ret.ori);
	return ret;
}

/*--------------------------------------
	rnd::Socket::CheckAnimation
--------------------------------------*/
bool rnd::Socket::CheckAnimation(const Animation* ani) const
{
	if(ani && ani->mesh != mesh)
	{
		CON_ERRORF("Socket '%s' belongs to mesh '%s' but animation '%s' belongs to mesh '%s'",
			name, mesh->FileName(), ani->Name(), ani->mesh->FileName());

		return false;
	}

	return true;
}

/*
################################################################################################


	SOCKET LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::SocPlace (Place)

IN	socS, aniA, nFrame
OUT	v3Pos, q4Ori
--------------------------------------*/
int rnd::SocPlace(lua_State* l)
{
	Socket* s = Socket::CheckLuaTo(1);
	const Animation* a = Animation::CheckLuaTo(2);
	float frame = lua_tonumber(l, 3);
	com::place place = s->Place(a, frame);
	vec::LuaPushVec(l, place.pos);
	qua::LuaPushQua(l, place.ori);
	return 7;
}

/*--------------------------------------
LUA	rnd::SocPlaceEntity (PlaceEntity)

IN	socS, entE, nFraction
OUT	v3Pos, q4Ori
--------------------------------------*/
int rnd::SocPlaceEntity(lua_State* l)
{
	Socket* s = Socket::CheckLuaTo(1);
	scn::Entity* e = scn::Entity::CheckLuaTo(2);
	float fraction = luaL_checknumber(l, 3);
	com::place place = s->PlaceEntity(*e, fraction);
	vec::LuaPushVec(l, place.pos);
	qua::LuaPushQua(l, place.ori);
	return 7;
}

/*--------------------------------------
LUA	rnd::SocPlaceAlign (PlaceAlign)

IN	socS, v3SocPos, q4SocOri, entE, nTime
OUT	v3EntPos, q4EntOri
--------------------------------------*/
int rnd::SocPlaceAlign(lua_State* l)
{
	Socket* s = Socket::CheckLuaTo(1);
	com::Vec3 socPos = vec::LuaToVec(l, 2, 3, 4);
	com::Qua socOri = qua::LuaToQua(l, 5, 6, 7, 8);
	scn::Entity* e = scn::Entity::CheckLuaTo(9);
	float time = luaL_checknumber(l, 10);
	com::place place = s->PlaceAlign(socPos, socOri, *e, time);
	vec::LuaPushVec(l, place.pos);
	qua::LuaPushQua(l, place.ori);
	return 7;
}

/*
################################################################################################


	ANIMATION


################################################################################################
*/

const char* const rnd::Animation::METATABLE = "metaAnimation";

/*--------------------------------------
	rnd::AdvanceAnimation

Returns relative frame after advancing by advance frames. A frame up to one integer beyond the
animation's length is between the end and start frames.
--------------------------------------*/
float rnd::AdvanceAnimation(const Animation& ani, float frame, float advance, bool loop)
{
	frame += advance;

	if(loop)
	{
		float bound = ani.end - ani.start + 1;

		if(advance >= 0.0f)
		{
			while(frame >= bound)
				frame -= bound;
		}
		else
		{
			while(frame < 0)
				frame += bound;
		}
	}
	else if(advance >= 0.0f)
	{
		float length = ani.end - ani.start;

		if(frame > length)
			frame = length;
	}
	else if(frame < 0.0f)
		frame = 0.0f;

	return frame;
}

/*--------------------------------------
	rnd::AdvanceTrack
--------------------------------------*/
void rnd::AdvanceTrack(float time, const Animation* ani, float& frame, float speed, bool loop,
	float& trans, size_t& a, size_t& b, float& f)
{
	if(trans <= time)
	{
		time = time - trans;
		trans = 0.0f;

		if(ani)
		{
			frame = rnd::AdvanceAnimation(*ani, frame, ani->mesh->FrameRate() * speed * time,
				loop);
		}

		AbsFrame(ani, frame, a, b, f);
	}
	else
	{
		size_t pre, post; // Discrete frames near target
		float beg, end; // t range (-1, 2)
		AbsFrame(ani, frame, pre, post, end);

		if(a == pre && b == post)
			beg = f + 1.0f; // Lerping from pre to target
		else if(a == post && b == pre)
		{
			beg = f + 1.0f; // Lerping from post to target
			end = 1.0f - end; // Flip end factor
		}
		else
		{
			if(b == pre || b == post || !f)
				beg = f; // Lerping towards discrete frame near target
			else
				beg = f - 1.0f; // Lerping towards some discrete frame

			if(speed < 0.0f && pre != post)
			{
				// Will move from post to target instead of from pre; flip end factor
				end = 1.0f - end;
			}
		}

		end += 1.0f;

		// (-1, 0): from old frame to some discrete frame
		// [0, 1] : between old and discrete frame near target
		// (1, 2) : from nearby discrete frame to target frame
		float t = COM_LERP(beg, end, time / trans);

		if(t < 0.0f)
			f = t + 1.0f;
		else if(t <= 1.0f)
		{
			if(b != pre && b != post)
			{
				a = b;
				b = speed >= 0.0f ? pre : post;
			}

			f = t;
		}
		else
		{
			if(b == pre)
			{
				if(a != post)
				{
					a = pre;
					b = post;
				}
			}
			else if(b == post)
			{
				if(a != pre)
				{
					a = post;
					b = pre;
				}
			}
			else if(speed >= 0.0f)
			{
				a = pre;
				b = post;
			}
			else
			{
				a = post;
				b = pre;
			}

			f = t - 1.0f;
		}

		trans -= time;
	}
}

/*--------------------------------------
	rnd::AbsFrame

frame = [0, ani->end - ani->start + 1)
--------------------------------------*/
void rnd::AbsFrame(const Animation* ani, float frame, size_t& a, size_t& b, float& f)
{
	if(!ani)
	{
		a = b = 0;
		f = 0;
		return;
	}

	float flr = floorf(frame);
	a = ani->start + flr;
	b = ani->start + ceilf(frame);
	f = frame - flr;

	if(a > ani->end)
		a = ani->start; // Out-of-bounds frame

	if(b > ani->end)
		b = ani->start; // Wrap to start
}

/*
################################################################################################


	ANIMATION LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::AniFrames (Frames)

IN	aniA
OUT	iStart, iEnd
--------------------------------------*/
int rnd::AniFrames(lua_State* l)
{
	Animation* a = Animation::CheckLuaTo(1);
	lua_pushinteger(l, a->start);
	lua_pushinteger(l, a->end);
	return 2;
}

/*--------------------------------------
LUA	rnd::AniEnd (End)

IN	aniA
OUT	iEnd
--------------------------------------*/
int rnd::AniEnd(lua_State* l)
{
	Animation* a = Animation::CheckLuaTo(1);
	lua_pushinteger(l, a->end);
	return 1;
}

/*--------------------------------------
LUA	rnd::AniNumFrames (NumFrames)

IN	aniA
OUT	iNum
--------------------------------------*/
int rnd::AniNumFrames(lua_State* l)
{
	Animation* a = Animation::CheckLuaTo(1);
	lua_pushinteger(l, a->end - a->start + 1);
	return 1;
}