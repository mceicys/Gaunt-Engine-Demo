// path_ticket.cpp
// Martynas Ceicys

#include "path.h"
#include "path_ticket.h"

namespace pat
{
	TicketQueue<FlightWorkMemory> Ticket<FlightWorkMemory>::q;

	con::Option
		maxNumVisits("pat_max_num_visits", 50),
		Ticket<FlightWorkMemory>::maxNumSimultaneous("pat_max_num_simultaneous_flight", 3,
			Ticket<FlightWorkMemory>::SetMaxNumSimultaneous);
}