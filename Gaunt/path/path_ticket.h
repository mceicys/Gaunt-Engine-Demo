// path_ticket.h
// Martynas Ceicys

#ifndef PATH_TICKET_H
#define PATH_TICKET_H

#include "../console/option.h"
#include "../../GauntCommon/pool.h"

namespace pat
{

extern con::Option maxNumVisits;

template <class Mem> class Ticket;

/*======================================
	pat::TicketQueue
======================================*/
template <class Mem> class TicketQueue
{
public:
	com::Pool<Ticket<Mem>*>	pathQueue; // Tickets waiting for a memory
	Ticket<Mem>**			working; // Tickets that have a memory
	Mem*					memories;
	Mem**					openMemories; // Not assigned to a Ticket
	size_t					numOpenMemories;
	unsigned				punchCode;
};

/*======================================
	pat::Ticket

Used to queue up for, receive, and keep storage for path searches.

FIXME: this shouldn't be public but can't define FlightNavigator otherwise
======================================*/
template <class Mem> class Ticket
{

public:
	Ticket() : index(-1), mem(0), code(0) {}
	~Ticket() {Drop();}

	Mem* Memory() const {return mem;}
	bool Active() const {return code == q.punchCode;} // Punch'd after latest IncPunchCode
	bool Queued() const {return index != -1;} // FIXME: rename since also true when past queue

private:
	size_t		index; // Index in wait queue pool if mem == 0, otherwise index in working array
	Mem*		mem;
	unsigned	code;

// public static:
public:
	static con::Option	maxNumSimultaneous;

// private static:
private:
	static TicketQueue<Mem> q;

	friend void Init();
	friend void PostTick();

/*
################################################################################################
	PUBLIC DEFINITIONS
################################################################################################
*/

public:

/*--------------------------------------
	pat::Ticket::Punch

Call this every tick a Mem is needed. If, after calling, the Ticket does not have a Mem, the
Ticket is in the waiting queue. Call every tick to hang onto its place in the queue and to its
assigned Mem if it has one. A newly assigned Mem's state is clear.
--------------------------------------*/
void Punch()
{
	code = q.punchCode;

	if(index == -1)
	{
		index = q.pathQueue.Link();
		q.pathQueue[index].o = this;
	}

	if(!mem && q.numOpenMemories && index == q.pathQueue.First())
	{
		q.pathQueue.Unlink(index);
		index = NumWorking();
		q.working[index] = this;
		mem = q.openMemories[--q.numOpenMemories];
	}
}

/*--------------------------------------
	pat::Ticket::Drop

Drops the Ticket's Mem or its place in the queue.
--------------------------------------*/
void Drop()
{
	code = 0;

	if(mem)
	{
		mem->Clear();
		q.working[index] = q.working[NumWorking() - 1];
		q.working[index]->index = index;
		q.openMemories[q.numOpenMemories++] = mem;
		mem = 0;
	}
	else if(index != -1)
		q.pathQueue.Unlink(index);

	index = -1;
}

/*--------------------------------------
	pat::Ticket::OfferDrop

Drops and returns true if there are more queued Tickets than open Mems.
--------------------------------------*/
bool OfferDrop()
{
	if(q.pathQueue.Num() > q.numOpenMemories)
	{
		Drop();
		return true;
	}

	return false;
}

/*
################################################################################################
	PUBLIC STATIC DEFINITIONS
################################################################################################
*/

public:

/*--------------------------------------
	pat::Ticket::SetMaxNumSimultaneous

(Re)allocates Mems. Drops all current searches.

FIXME: this can be used to cheat, should there be a cheat mode?
--------------------------------------*/
static void SetMaxNumSimultaneous(con::Option& opt, float set)
{
	int si = set;

	if(si < 0)
	{
		con::AlertF("Max number of simultaneous path searches must be non-negative");
		return;
	}

	if(opt.Float() == si)
		return;

	FreeMemories();
	opt.ForceValue(si);
	AllocateMemories();
}

/*
################################################################################################
	PRIVATE STATIC DEFINITIONS
################################################################################################
*/

private:

/*--------------------------------------
	pat::Ticket::IncPunchCode
--------------------------------------*/
static void IncPunchCode()
{
	if(!(++q.punchCode))
		q.punchCode = 1;
}

/*--------------------------------------
	pat::Ticket::DropStale
--------------------------------------*/
static void DropStale()
{
	for(size_t i = q.pathQueue.First(), next; i != -1; i = next)
	{
		next = q.pathQueue[i].Next();

		if(!q.pathQueue[i].o->Active())
			q.pathQueue[i].o->Drop();
	}

	for(size_t i = 0; i < NumWorking();)
	{
		if(q.working[i]->Active())
			i++;
		else
			q.working[i]->Drop();
	}
}

/*--------------------------------------
	pat::Ticket::RestoreAllVisits
--------------------------------------*/
static void RestoreAllVisits()
{
	for(size_t i = 0; i < maxNumSimultaneous.Integer(); i++)
		q.memories[i].RestoreVisits();
}

/*--------------------------------------
	pat::Ticket::NumWorking
--------------------------------------*/
static size_t NumWorking()
{
	return maxNumSimultaneous.Integer() - q.numOpenMemories;
}

/*--------------------------------------
	pat::Ticket::AllocateMemories
--------------------------------------*/
static void AllocateMemories()
{
	q.numOpenMemories = maxNumSimultaneous.Integer();

	if(q.numOpenMemories)
	{
		q.working = new Ticket<Mem>*[q.numOpenMemories];
		memset(q.working, 0, sizeof(Ticket<Mem>*) * q.numOpenMemories);
		q.memories = new Mem[q.numOpenMemories];
		q.openMemories = new Mem*[q.numOpenMemories];
		
		for(size_t i = 0; i < q.numOpenMemories; i++)
			q.openMemories[i] = &q.memories[i];
	}
}

/*--------------------------------------
	pat::Ticket::FreeMemories
--------------------------------------*/
static void FreeMemories()
{
	// Drop working Tickets
	while(NumWorking())
		q.working[0]->Drop();

	// Free
	if(q.working)
	{
		delete[] q.working;
		q.working = 0;
	}

	if(q.memories)
	{
		delete[] q.memories;
		q.memories = 0;
	}

	if(q.openMemories)
	{
		delete[] q.openMemories;
		q.openMemories = 0;
	}

	q.numOpenMemories = 0;
}

/*--------------------------------------
	pat::Ticket::Init

Call in pat::Init
--------------------------------------*/
static void Init()
{
	AllocateMemories();
	IncPunchCode();
}

/*--------------------------------------
	pat::Ticket::PostTick

Call in pat::PostTick
--------------------------------------*/
static void PostTick()
{
	DropStale();
	IncPunchCode();
	RestoreAllVisits();
}

}; // End of Ticket

}

#endif