// render_timer.cpp
// Martynas Ceicys

#include "render_private.h"
#include "../console/console.h"

rnd::Timer* rnd::Timer::timing = 0;

/*--------------------------------------
	rnd::Timer::Timer
--------------------------------------*/
rnd::Timer::Timer() : query(0), accum(0), pending(false) {}

/*--------------------------------------
	rnd::Timer::~Timer
--------------------------------------*/
rnd::Timer::~Timer()
{
	Stop();

	if(query)
		glDeleteQueries(1, &query);
}

/*--------------------------------------
	rnd::Timer::Init
--------------------------------------*/
void rnd::Timer::Init()
{
	if(query)
		WRP_FATAL("Render timer initialized twice");

	glGenQueries(1, &query);
}

/*--------------------------------------
	rnd::Timer::Start
--------------------------------------*/
void rnd::Timer::Start()
{
	if(!doTiming || timing == this)
		return;

	if(timing)
	{
		// Can only have one query running at a time
		con::LogF("Render timers overlapped");
		timing->Stop();
	}

	UpdateAccumulated();
	glBeginQuery(GL_TIME_ELAPSED, query);
	timing = this;
}

/*--------------------------------------
	rnd::Timer::Stop
--------------------------------------*/
void rnd::Timer::Stop()
{
	if(timing == this)
	{
		glEndQuery(GL_TIME_ELAPSED);
		pending = true;
		timing = 0;
	}
}

/*--------------------------------------
	rnd::Timer::Reset
--------------------------------------*/
void rnd::Timer::Reset()
{
	Stop();
	accum = 0;
	pending = false;
}

/*--------------------------------------
	rnd::Timer::Accumulated
--------------------------------------*/
GLuint rnd::Timer::Accumulated()
{
	Stop();
	UpdateAccumulated();
	return accum;
}

/*--------------------------------------
	rnd::Timer::UpdateAccumulated
--------------------------------------*/
void rnd::Timer::UpdateAccumulated()
{
	if(pending)
	{
		GLuint result;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &result);
		accum += result;
		pending = false;
	}
}