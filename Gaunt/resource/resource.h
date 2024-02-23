// resource.h
// Martynas Ceicys

#ifndef RESOURCE_H
#define RESOURCE_H

#include <ctype.h>

#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../../GauntCommon/link.h"
#include "../record/pairs.h"
#include "../script/script.h"
#include "../wrap/wrap.h"

namespace res
{

/*
################################################################################################
	RESOURCE
################################################################################################
*/

extern int weakRegistry;

/*======================================
	res::Resource

Base class template that helps with managing objects shared by the engine and the game.

Userdata is created (with given metatable and storing a pointer to the object) once LuaPush is
called, after which the resource must be locked to prevent garbage collection. Objects of any
derived class should be deleted when their userdata is garbage collected or should be kept
permanently locked to prevent premature loss of the userdata and undefined behavior. If
deleteOnUnlock is true, the object must be immediately locked upon construction. The address
inside the userdata is set to 0 when the resource is Kill'd or deleted.

Resources with a transcript are locked until the transcript is unset.

There are four general Resource paradigms:
1. Garbage-collected
	* No permanent locks
	* Deleted once pushed into Lua environment and all references and locks are gone
	* Unused (no locks, never LuaPush'd) deleted explicitly at some point; DeleteUnused function
2. Cleaned-up
	* "Permanent" locks are held and temporarily removed during clean-up process
	* Deleted during clean-up if all references and locks are gone
3. Explicitly-killed
	* Starts with a lifetime lock
	* Function call disables userdata and removes lifetime lock; Resource is "dead"
	* Deleted once all locks are removed
4. Explicitly-deleted
	* Permanently locked
	* Deleted by function call
	* Locks cannot prevent undefined behavior when holding long-term engine-side pointers

FIXME: Write default __gc function that unreferences userdata without requiring the object to be
	deleted, optionally reporting unintended userdata loss

FIXME: Remove recordIDs and transcripts, do Lua auto-serialization
	But we might need these for level loading
======================================*/
template <class T, bool deleteOnUnlock = false> class Resource
{
public:
	static const char* const METATABLE; // Must set this for each template specialization

	bool Used() const {return numLocks || weakRef != LUA_NOREF;}
	bool Locked() const {return numLocks;}
	bool Alive() const {return alive;}

	void AddLock() const
	{
		if(!deleteOnUnlock && !numLocks && weakRef != LUA_NOREF)
		{
			// Lock resource; prevent garbage collection of userdata
			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, weakRegistry);
			lua_rawgeti(scr::state, -1, weakRef);
			ref = luaL_ref(scr::state, LUA_REGISTRYINDEX);
			lua_pop(scr::state, 1);
		}

		numLocks++;
	}

	/* If deleteOnUnlock is true, Resource may be deleted after calling; caller is deliberately
	not told whether it was, and must assume it was unless it can guarantee a life-saving lock
	is in place */
	void RemoveLock() const
	{
		if(!deleteOnUnlock && !numLocks)
			WRP_FATALF("%s resource already unlocked", METATABLE);

		numLocks--;

		if(!numLocks)
		{
			if(deleteOnUnlock)
			{
				LicenseToDelete<T, deleteOnUnlock>::Delete((const T*)this);
				return;
			}
			else if(weakRef != LUA_NOREF)
			{
				// Unlock resource; allow garbage collection of userdata
				luaL_unref(scr::state, LUA_REGISTRYINDEX, ref);
				ref = LUA_NOREF;
			}
		}
	}

	/* Lua does not have const, so const things that are pushed should have no modifying
	interface */
	void LuaPush() const
	{
		if(ref != LUA_NOREF)
		{
			scr::EnsureStack(scr::state, 1);
			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, ref);
		}
		else if(weakRef != LUA_NOREF)
		{
			scr::EnsureStack(scr::state, 2);
			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, weakRegistry);
			lua_rawgeti(scr::state, -1, weakRef);
			lua_remove(scr::state, -2);
		}
		else
		{
			scr::EnsureStack(scr::state, 3);
			EnsureLink();

			// Create userdata
			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, weakRegistry);

			userdata = (T**)lua_newuserdata(scr::state, sizeof(T*));
			*userdata = alive ? (T*)this : 0;

			lua_rawgeti(scr::state, LUA_REGISTRYINDEX, metaRef);
			lua_setmetatable(scr::state, -2);

			if(numLocks)
			{
				lua_pushvalue(scr::state, -1);
				ref = luaL_ref(scr::state, LUA_REGISTRYINDEX);
			}

			lua_pushvalue(scr::state, -1);
			weakRef = luaL_ref(scr::state, -3);
			lua_remove(scr::state, -2);
		}
	}

	void EnsureLink() const
	{
		if(!link)
		{
			com::Link<T>(list, (T*)this);
			link = list.l;
		}
	}

	const com::linker<T>* Linker() const {return link;}

	// Puts the Resource at the front of the list
	void Prioritize() const
	{
		if(link)
			com::Unlink<T>(list, link);

		link = com::LinkBefore<T>(list, list.f, (T*)this);
	}

	const char* RecordID() const {return recordID;}

	void ClearRecordID()
	{
		if(recordID)
			delete[] recordID;

		recordID = 0;
	}

	// Cancels and returns false if id is not unique
	// If id starts with a digit, it will be overwritten by SetDefaultRecordIDs
	bool SetRecordID(const char* id, bool alert = false)
	{
		if(FromRecordID(id))
		{
			if(alert)
				con::AlertF("Record ID '%s' already in use", id);

			return false;
		}

		ClearRecordID();

		if(!id)
			return true;

		recordID = new char[strlen(id) + 1];
		strcpy(recordID, id);
		EnsureLink(); // So the Resource can be found by ID
		return true;
	}

	void SetTranscriptPtr(com::JSVar* var)
	{
		if(transcript)
		{
			if(!var)
			{
				transcript = 0;
				RemoveLock(); // Might be deleted here
				return;
			}
		}
		else
		{
			if(var)
				AddLock();
		}

		transcript = var;
		EnsureLink(); // So UnsetTranscripts unsets this transcript
	}

	com::JSVar* Transcript() {return transcript;}
	const com::JSVar* Transcript() const {return transcript;}

	static void RegisterMetatable(const luaL_Reg* regs, const scr::constant* constants,
		bool indexSelf = true)
	{
		if(metaRef != LUA_NOREF)
			WRP_FATALF("%s registered twice", METATABLE);

		metaRef = scr::RegisterMetatable(scr::state, METATABLE, regs, constants, indexSelf);
	}

	static T* LuaTo(int index)
	{
		if(T** ud = (T**)lua_touserdata(scr::state, index)) // Get userdata pointer
		{
			if(lua_getmetatable(scr::state, index)) // Push userdata's metatable
			{
				lua_rawgeti(scr::state, LUA_REGISTRYINDEX, metaRef); // Push Resource metatable
				int eq = lua_rawequal(scr::state, -1, -2); // Compare metatables and save result
				lua_pop(scr::state, 2); // Pop metatables
				return eq ? *ud : 0;
			}
		}

		return 0; // Not a userdata with a metatable
	}

	// Like LuaTo but raises a Lua error on failure
	static T* CheckLuaTo(int index)
	{
		T* t = UserdataLuaTo(index);

		if(!t)
		{
			luaL_argerror(scr::state, index, lua_pushfstring(scr::state, "got deleted %s",
				METATABLE));
		}

		return t;
	}

	// Like CheckLuaTo but missing arg and explicit nil are accepted
	static T* OptionalLuaTo(int index)
	{
		return lua_type(scr::state, index) <= LUA_TNIL ? 0 : CheckLuaTo(index);
	}

	// Checks the metatable, but the pointer in the userdata can be 0
	static T* UserdataLuaTo(int index)
	{
		if(T** ud = (T**)lua_touserdata(scr::state, index))
		{
			if(lua_getmetatable(scr::state, index))
			{
				lua_rawgeti(scr::state, LUA_REGISTRYINDEX, metaRef);
				int eq = lua_rawequal(scr::state, -1, -2);
				lua_pop(scr::state, 2);

				if(!eq)
					scr::TypeError(scr::state, index, METATABLE);

				return *ud;
			}
		}

		scr::TypeError(scr::state, index, METATABLE);
		return 0;
	}

	static const com::list<T>& List()
	{
		return list;
	}

	static T* FromRecordID(const char* id)
	{
		if(!id)
			return 0;

		for(com::linker<T>* it = list.f; it; it = it->next)
		{
			if(!it->o->RecordID())
				continue;

			if(strcmp(it->o->RecordID(), id) == 0)
				return it->o;
		}

		return 0;
	}

	static void ClearRecordIDs()
	{
		for(com::linker<T>* it = list.f; it; it = it->next)
			it->o->ClearRecordID();
	}

	static unsigned SetDefaultRecordIDs(unsigned start = 1)
	{
		unsigned count = start;
		for(com::linker<T>* it = list.f; it; it = it->next)
		{
			if(!it->o->RecordID() || IsDefaultRecordID(it->o->RecordID()))
				it->o->SetDefaultRecordID(count++);
		}
		return count;
	}

	static bool IsDefaultRecordID(const char* id)
	{
		return id && isdigit(id[0]);
	}

	static void UnsetTranscripts()
	{
		Ptr<T> p;

		for(com::linker<T>* it = list.f; it; it = it->next)
		{
			p.Set(it->o);
			p->SetTranscriptPtr(0);
		}
	}

	// IN	value
	// OUT	bExists
	// Returns true if value is a userdata pointing to a living Resource of type T
	static int CLuaExists(lua_State* l)
	{
		lua_pushboolean(l, LuaTo(1) != 0);
		return 1;
	}

	// IN	resR
	// OUT	strID
	static int CLuaRecordID(lua_State* l)
	{
		T* r = CheckLuaTo(1);
		if(r->RecordID())
		{
			lua_pushstring(l, r->RecordID());
			return 1;
		}

		return 0;
	}

	// IN	resR, strID
	// OUT	bSet
	static int CLuaSetRecordID(lua_State* l)
	{
		T* r = CheckLuaTo(1);
		const char* id = lua_tostring(l, 2);
		lua_pushboolean(l, r->SetRecordID(id));
		return 1;
	}

	// IN	strID
	// OUT	resR
	static int CLuaFromRecordID(lua_State* l)
	{
		const char* id = lua_tostring(l, 1);

		if(T* r = FromRecordID(id))
		{
			r->LuaPush();
			return 1;
		}

		return 0;
	}

	// IN	resR
	// OUT	transcript
	static int CLuaTranscript(lua_State* l)
	{
		T* r = CheckLuaTo(1);
		if(r->transcript)
		{
			rec::LuaPushJSVar(l, *r->transcript);
			return 1;
		}

		return 0;
	}

	// IN	resR, transcript
	static int CLuaSetTranscript(lua_State* l)
	{
		T* r = CheckLuaTo(1);
		if(!r->transcript)
			luaL_argerror(l, 1, "resource has no transcript");

		rec::LuaToJSVar(l, 2, *r->transcript);
		return 0;
	}

	// IN	resR
	static int CLuaDelete(lua_State* l)
	{
		T* r = UserdataLuaTo(1);

		if(!r)
			return 0; // Engine must have already deleted this resource

		if(r->Locked())
			luaL_error(l, "Tried to delete locked resource %s", METATABLE);

		delete r;
		return 0;
	}

	// OUT	resFirst
	static int CLuaFirst(lua_State* l)
	{
		for(com::linker<T>* it = T::List().f; it; it = it->next)
		{
			if(it->o->Alive())
			{
				it->o->LuaPush();
				return 1;
			}
		}

		return 0;
	}

	// OUT	resLast
	static int CLuaLast(lua_State* l)
	{
		for(com::linker<T>* it = T::List().l; it; it = it->prev)
		{
			if(it->o->Alive())
			{
				it->o->LuaPush();
				return 1;
			}
		}

		return 0;
	}

	// IN	resR
	// OUT	resPrev
	static int CLuaPrev(lua_State* l)
	{
		T* r = CheckLuaTo(1);

		for(com::linker<T>* it = r->Linker()->prev; it; it = it->prev)
		{
			if(it->o->Alive())
			{
				it->o->LuaPush();
				return 1;
			}
		}

		return 0;
	}

	// IN	resR
	// OUT	resNext
	static int CLuaNext(lua_State* l)
	{
		T* r = CheckLuaTo(1);

		for(com::linker<T>* it = r->Linker()->next; it; it = it->next)
		{
			if(it->o->Alive())
			{
				it->o->LuaPush();
				return 1;
			}
		}

		return 0;
	}

protected:
	Resource(unsigned numLocks = deleteOnUnlock ? 1 : 0) : transcript(0), numLocks(numLocks),
		recordID(0), link(0), weakRef(LUA_NOREF), ref(LUA_NOREF), userdata(0), alive(true)
	{
#if ENGINE_DEBUG
		if(deleteOnUnlock && !numLocks)
			WRP_FATALF("%s is deleteOnUnlock but did not initialize with any locks", METATABLE);
#endif
	}

	~Resource()
	{
		if(weakRef != LUA_NOREF)
		{
			*userdata = 0;

			if(ref != LUA_NOREF)
				luaL_unref(scr::state, LUA_REGISTRYINDEX, ref);
		}

		if(link)
			com::Unlink<T>(list.f, list.l, link);

		ClearRecordID();
	}

	// Set alive to false and nullifies userdata (cuts off Lua from engine-side object)
	void Kill()
	{
		if(weakRef != LUA_NOREF)
			*userdata = 0;

		ClearRecordID();
		alive = false;
	}

	void InterpretFailAlert(const char* typeName)
	{
		if(RecordID())
			con::AlertF("Failed to interpret %s '%s'", typeName, RecordID());
		else
			con::AlertF("Failed to interpret %s", typeName);
	}

private:
	static com::list<T> list; // Includes resources which have been LuaPush'd or EnsureLink'd
	// FIXME: com::Pool, or at least don't use linker to reduce mallocs
	// FIXME: make optional
	static int metaRef; // Lua registry metatable reference

	mutable unsigned numLocks; // mutable since const pointers should not be weak
	char* recordID; // Used to identify Resources during saving and loading
	mutable com::linker<T>* link;
	com::JSVar* transcript; // Record system allocates and deallocates
	mutable int weakRef, ref; // Lua registry userdata references
	mutable T** userdata; // Ptr to Lua-owned T*
	bool alive; // If false, disabled game-side but still exists engine-side
		// FIXME: waste of bytes for most Resources

	Resource(const Resource&) {};

	void SetDefaultRecordID(unsigned id)
	{
		ClearRecordID();
		size_t size = com::IntLength(id) + 1;
		recordID = new char[size];
		com::SNPrintF(recordID, size, 0, "%u", id);
	}
};

template<class T, bool d> com::list<T> Resource<T, d>::list = {0, 0};
template<class T, bool d> int Resource<T, d>::metaRef = LUA_NOREF;

/*======================================
	res::Ptr

Maintains a lock.
======================================*/
template <class T> class Ptr
{
public:
	Ptr() : val(0) {}

	Ptr(const Ptr<T>& p) : val(p.val)
	{
		if(val)
			val->AddLock();
	}

	Ptr(T* t) : val(t)
	{
		if(val)
			val->AddLock();
	}

	~Ptr()
	{
		if(val)
			val->RemoveLock();
	}

	Ptr<T>& Set(T* t)
	{
		if(val == t) // Prevent unlocking then relocking, since that may delete object
			return *this;

		if(val)
		{
			val->RemoveLock();
			val = 0;
		}

		if(t)
		{
			val = t;
			val->AddLock();
		}
		
		return *this;
	}

	T* Value() {return val;}
	const T* Value() const {return val;}
	Ptr<T>& Set(const Ptr<T>& p) {return Set(p.val);}
	Ptr<T>& operator=(const Ptr<T>& p) {return Set(p.val);}
	operator Ptr<const T>() const {return Ptr<const T>(val);}
	operator T*() const {return val;}
	operator bool() const {return val;}
	T& operator*() const {return *val;}
	T* operator->() const {return val;}
	bool operator==(const Ptr<T>& p) const {return val == p.val;}
	bool operator!=(const Ptr<T>& p) const {return val != p.val;}

private:
	T* val;

	Ptr<T>& operator=(T* t);
};

/*======================================
	res::LicenseToDelete

Resources with deleteOnUnlock == true and a protected destructor must friend this class to allow
Resource access to the destructor.
======================================*/
template <class T, bool doIt> class LicenseToDelete {public: static bool Delete(const T*);};

template <class T> class LicenseToDelete<T, true> {
	public: static bool Delete(const T* t) {delete t; return false;}
};

template <class T> class LicenseToDelete<T, false> {
	public: static bool Delete(const T* t) {return true;}
};

void Init();

}

#endif