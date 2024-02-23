// render_line_pass.cpp
// Martynas Ceicys

#include "render.h"
#include "render_lua.h"
#include "render_private.h"
#include "../../GauntCommon/array.h"
#include "../quaternion/qua_lua.h"

#define LINE_PASS_ATTRIB_POS	0
#define LINE_PASS_ATTRIB_COLOR	1

namespace rnd
{
	const size_t NUM_INIT_LINES = 8;

	struct vertex_line
	{
		GLfloat	pos[3];
		GLfloat	color;
	};

	struct lines
	{
		com::Arr<vertex_line> verts;
		com::Arr<unsigned> times;
		size_t num;
	};

	void	AddLine(lines& lIO, const vertex_line& a, const vertex_line& b, float time);
	void	AdvanceLinesStruct(lines& lIO, unsigned delta);
	void	LineState();
	void	Line3DState();
	void	Line2DState();
	void	LineCleanup();
	bool	Init3DLinePass();
	bool	Init2DLinePass();
}

static struct
{
	GLuint	vertBuffer;
	bool	init;
} linePass = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniWorldToClip;
	GLint	samPalette;
} line3DPass = {0};

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniInvVid;
	GLint	samPalette;
} line2DPass = {0};

static rnd::lines lines3D, lines2D;

/*--------------------------------------
	rnd::DrawLine

For visual debugging. time == 0 means for one frame.
--------------------------------------*/
void rnd::DrawLine(const com::Vec3& a, const com::Vec3& b, int color, float time)
{
	vertex_line la, lb;

	for(size_t i = 0; i < 3; i++)
	{
		la.pos[i] = a[i];
		lb.pos[i] = b[i];
	}

	la.color = lb.color = (GLfloat)color / 255.0f;
	AddLine(lines3D, la, lb, time);
}

/*--------------------------------------
	rnd::Draw2DLine
--------------------------------------*/
void rnd::Draw2DLine(const com::Vec2& a, const com::Vec2& b, int color, float time)
{
	vertex_line la, lb;

	for(size_t i = 0; i < 2; i++)
	{
		la.pos[i] = a[i];
		lb.pos[i] = b[i];
	}

	la.pos[2] = lb.pos[2] = 0.5f;
	la.color = lb.color = (GLfloat)color / 255.0f;
	AddLine(lines2D, la, lb, time);
}

/*--------------------------------------
	rnd::DrawSphereLines
--------------------------------------*/
void rnd::DrawSphereLines(const com::Vec3& pos, const com::Qua ori, float radius, int color,
	float time)
{
	if(color < 0)
		return;

	const int NUM_SEGMENTS = 32;
	const int NUM_RINGS = 30;
	const int NUM_PTS = 2 + NUM_SEGMENTS * NUM_RINGS;
	com::Vec3 pts[NUM_PTS];
	pts[0] = 0.0f;
	pts[0].z -= radius;
	pts[NUM_PTS - 1] = 0.0f;
	pts[NUM_PTS - 1].z += radius;

	for(int ring = 0; ring < NUM_RINGS; ring++)
	{
		float ringAngle = COM_PI * (ring + 1) / float(NUM_RINGS + 1);
		float z = -cos(ringAngle) * radius;
		float ringRadius = sin(ringAngle) * radius;

		for(int seg = 0; seg < NUM_SEGMENTS; seg++)
		{
			com::Vec3& pt = pts[seg + 1 + ring * NUM_SEGMENTS];
			float segAngle = COM_PI_2 * seg / (float)NUM_SEGMENTS;
			pt.x = cos(segAngle) * ringRadius;
			pt.y = sin(segAngle) * ringRadius;
			pt.z = z;
		}
	}

	for(int i = 0; i < NUM_PTS; i++)
		pts[i] = pos + com::VecRot(pts[i], ori);

	for(int ring = 0; ring < NUM_RINGS; ring++)
	{
		int start = 1 + ring * NUM_SEGMENTS;

		for(int seg = 0; seg < NUM_SEGMENTS; seg++)
		{
			int a = start + seg;
			int b = start + (seg + 1) % NUM_SEGMENTS;
			DrawLine(pts[a], pts[b], color, time);
		}
	}

	for(int curve = 0; curve < NUM_SEGMENTS; curve++)
	{
		int prev = 0;

		for(int ring = 0; ring < NUM_RINGS; ring++)
		{
			int cur = 1 + curve + ring * NUM_SEGMENTS;
			DrawLine(pts[prev], pts[cur], color, time);
			prev = cur;
		}

		DrawLine(pts[prev], pts[NUM_PTS - 1], color, time);
	}
}

/*--------------------------------------
	rnd::AdvanceLines
--------------------------------------*/
void rnd::AdvanceLines()
{
	static unsigned long long oldTime = 0;
	unsigned long long time = wrp::SimTime();
	unsigned delta = time - oldTime;
	oldTime = time;
	AdvanceLinesStruct(lines3D, delta);
	AdvanceLinesStruct(lines2D, delta);
}

/*--------------------------------------
	rnd::AddLine
--------------------------------------*/
void rnd::AddLine(lines& l, const vertex_line& a, const vertex_line& b, float time)
{
	if(time >= 0.0f)
	{
		size_t numVerts = l.num * 2;
		l.verts.Ensure(numVerts + 2);
		l.verts[numVerts] = a;
		l.verts[numVerts + 1] = b;
		l.times.Ensure(l.num + 1);
		l.times[l.num] = ceil(time * 1000.0f);
		l.num++;
	}
}

/*--------------------------------------
	rnd::AdvanceLinesStruct
--------------------------------------*/
void rnd::AdvanceLinesStruct(lines& l, unsigned delta)
{
	size_t readLine = 0, writeLine = 0;
	size_t origNum = l.num;

	for(; readLine < origNum; readLine++)
	{
		if(l.times[readLine] > delta)
		{
			l.times[writeLine] = l.times[readLine] - delta;
			l.verts[writeLine * 2] = l.verts[readLine * 2];
			l.verts[writeLine * 2 + 1] = l.verts[readLine * 2 + 1];
			writeLine++;
		}
		else
			l.num--;
	}
}

/*--------------------------------------
	rnd::LineState
--------------------------------------*/
void rnd::LineState()
{
	glDisable(GL_DEPTH_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, linePass.vertBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glEnableVertexAttribArray(LINE_PASS_ATTRIB_POS);
	glEnableVertexAttribArray(LINE_PASS_ATTRIB_COLOR);
}

/*--------------------------------------
	rnd::Line3DState
--------------------------------------*/
void rnd::Line3DState()
{
	glUseProgram(line3DPass.shaderProgram);

	glVertexAttribPointer(LINE_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_line),
		(void*)0);
	glVertexAttribPointer(LINE_PASS_ATTRIB_COLOR, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_line),
		(void*)(sizeof(GLfloat) * 3));

	size_t bufferSize = sizeof(vertex_line) * lines3D.num * 2;
	glBufferData(GL_ARRAY_BUFFER, bufferSize, lines3D.verts.o, GL_DYNAMIC_DRAW);
	glUniformMatrix4fv(line3DPass.uniWorldToClip, 1, GL_FALSE, gWorldToClip);
}

/*--------------------------------------
	rnd::Line2DState
--------------------------------------*/
void rnd::Line2DState()
{
	glUseProgram(line2DPass.shaderProgram);

	glVertexAttribPointer(LINE_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_line),
		(void*)0);
	glVertexAttribPointer(LINE_PASS_ATTRIB_COLOR, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_line),
		(void*)(sizeof(GLfloat) * 3));

	size_t bufferSize = sizeof(vertex_line) * lines2D.num * 2;
	glBufferData(GL_ARRAY_BUFFER, bufferSize, lines2D.verts.o, GL_DYNAMIC_DRAW);
	glUniform2f(line2DPass.uniInvVid, 1.0f / wrp::VideoWidth(), 1.0f / wrp::VideoHeight());
}

/*--------------------------------------
	rnd::LineCleanup
--------------------------------------*/
void rnd::LineCleanup()
{
	glDisableVertexAttribArray(LINE_PASS_ATTRIB_POS);
	glDisableVertexAttribArray(LINE_PASS_ATTRIB_COLOR);
}

/*--------------------------------------
	rnd::LinePass
--------------------------------------*/
void rnd::LinePass()
{
	if(!linePass.init || (!lines3D.num && !lines2D.num))
		return;

	LineState();

	if(lines3D.num)
	{
		Line3DState();
		glDrawArrays(GL_LINES, 0, lines3D.num * 2);
	}

	if(lines2D.num)
	{
		Line2DState();
		glDrawArrays(GL_LINES, 0, lines2D.num * 2);
	}

	LineCleanup();
}

/*--------------------------------------
	rnd::Init3DLinePass
--------------------------------------*/
bool rnd::Init3DLinePass()
{
	prog_attribute attributes[] =
	{
		{LINE_PASS_ATTRIB_POS, "pos"},
		{LINE_PASS_ATTRIB_COLOR, "color"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&line3DPass.uniWorldToClip, "worldToClip"},
		{&line3DPass.samPalette, "palette"},
		{0, 0}
	};

	if(!InitProgram("line 3d", line3DPass.shaderProgram, line3DPass.vertexShader,
	&vertLineSource, 1, line3DPass.fragmentShader, &fragLineSource, 1, attributes, uniforms))
		return false;

	glUseProgram(line3DPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(line3DPass.samPalette, RND_PALETTE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);
	return true;
}

/*--------------------------------------
	rnd::Init2DLinePass
--------------------------------------*/
bool rnd::Init2DLinePass()
{
	prog_attribute attributes[] =
	{
		{LINE_PASS_ATTRIB_POS, "pos"},
		{LINE_PASS_ATTRIB_COLOR, "color"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&line2DPass.uniInvVid, "invVid"},
		{&line2DPass.samPalette, "palette"},
		{0, 0}
	};

	if(!InitProgram("line 2d", line2DPass.shaderProgram, line2DPass.vertexShader,
	&vertLine2DSource, 1, line2DPass.fragmentShader, &fragLineSource, 1, attributes, uniforms))
		return false;

	glUseProgram(line2DPass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(line2DPass.samPalette, RND_PALETTE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);
	return true;
}

/*--------------------------------------
	rnd::InitLinePass
--------------------------------------*/
bool rnd::InitLinePass()
{
	lines3D.verts.Init(NUM_INIT_LINES * 2);
	lines3D.times.Init(NUM_INIT_LINES);
	lines3D.num = 0;
	lines2D.verts.Init(NUM_INIT_LINES * 2);
	lines2D.times.Init(NUM_INIT_LINES);
	lines2D.num = 0;

	glGenBuffers(1, &linePass.vertBuffer);
	return linePass.init = Init3DLinePass() && Init2DLinePass();
}

/*
################################################################################################


	LINE LUA


################################################################################################
*/

/*--------------------------------------
LUA	rnd::DrawLine

IN	v3A, v3B, iColor, [nTime = 0]
--------------------------------------*/
int rnd::DrawLine(lua_State* l)
{
	DrawLine(vec::CheckLuaToVec(l, 1, 2, 3), vec::CheckLuaToVec(l, 4, 5, 6),
		luaL_checkinteger(l, 7), luaL_optnumber(l, 8, 0.0));
	return 0;
}

/*--------------------------------------
LUA	rnd::Draw2DLine

IN	v2A, v2B, iColor, [nTime = 0]
--------------------------------------*/
int rnd::Draw2DLine(lua_State* l)
{
	Draw2DLine(vec::CheckLuaToVec(l, 1, 2), vec::CheckLuaToVec(l, 3, 4),
		luaL_checkinteger(l, 5), luaL_optnumber(l, 6, 0.0));
	return 0;
}

/*--------------------------------------
LUA	rnd::DrawSphereLines

IN	v3Pos, q4Ori, nRadius, iColor, [nTime = 0]
--------------------------------------*/
int rnd::DrawSphereLines(lua_State* l)
{
	DrawSphereLines(vec::CheckLuaToVec(l, 1, 2, 3), qua::CheckLuaToQua(l, 4, 5, 6, 7),
		luaL_checknumber(l, 8), luaL_checkinteger(l, 9), luaL_optnumber(l, 10, 0.0));
	return 0;
}