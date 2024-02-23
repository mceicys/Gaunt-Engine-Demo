// render_world_glass_pass.cpp -- Draw light on world faces
// Martynas Ceicys

#include "render.h"
#include "render_private.h"

#define WORLD_GLASS_PASS_ATTRIB_POS			0
#define WORLD_GLASS_PASS_ATTRIB_INTENSITY	1

namespace rnd
{
	void WorldGlassState();
	void WorldGlassCleanup();
}

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniWorldToClip, uniIntensityFactor, uniIntensityExponent;
} worldAmbientPass = {0};

/*--------------------------------------
	rnd::WorldGlassState
--------------------------------------*/
void rnd::WorldGlassState()
{
	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboLightBuffer);

	glUseProgram(worldAmbientPass.shaderProgram);
	glUniformMatrix4fv(worldAmbientPass.uniWorldToClip, 1, GL_FALSE, gWorldToClip);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_EQUAL);
	glDepthMask(GL_FALSE);

	glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_BLEND);

	glEnableVertexAttribArray(WORLD_GLASS_PASS_ATTRIB_POS);
	glEnableVertexAttribArray(WORLD_GLASS_PASS_ATTRIB_INTENSITY);

	glBindBuffer(GL_ARRAY_BUFFER, worldVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldElementBuffer);

	glVertexAttribPointer(WORLD_GLASS_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE,
		sizeof(vertex_world), (void*)0);
}

/*--------------------------------------
	rnd::WorldGlassCleanup
--------------------------------------*/
void rnd::WorldGlassCleanup()
{
	if(usingFBOs.Bool())
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glDisableVertexAttribArray(WORLD_GLASS_PASS_ATTRIB_POS);
	glDisableVertexAttribArray(WORLD_GLASS_PASS_ATTRIB_INTENSITY);

	glDisable(GL_BLEND);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDepthMask(GL_TRUE);
}

/*--------------------------------------
	rnd::WorldGlassPass

FIXME: Influence is not affected here so surfaces only lit by world-glass are sharply recolored
	by other lights.

FIXME: Convert this into an ambience lightmap feature?
	Engine should be able to bake lightmap at run time
		Designer can then edit ambience bulbs and vertices at run time, then bake them to draw more efficiently when playing
			Designer could even directly draw in the lightmap, too, provided an interface
			Problem: Vertex + brush edits would be lost if the .wld geometry was changed
		Only bulbs marked "ambient" with no sub-palette are baked in, along with additive and multiplicative world-vertex values
		Entities don't need to react to light map
			We don't want most ambient lights to light up entities anyway; looks bad

FIXME: Only draw batches containing glass attributes?
FIXME: Have processor sort batch triangles so non-glass comes first, min-shade and additive ambient next, subtractive ambient last
	Draw partial batch so only glass frags are made
		This might break OpenGL's rasterization consistency guarantee, in which case the whole batch will have to be drawn
			May as well leave the sorting
	UPDATE: If this is going to become a lightmap renderer, these considerations can be dropped; all zone surfaces will be redrawn
--------------------------------------*/
void rnd::WorldGlassPass()
{
	timers[TIMER_WORLD_GLASS_PASS].Start();

	WorldGlassState();

	// Additive ambient
	glVertexAttribPointer(WORLD_GLASS_PASS_ATTRIB_INTENSITY, 1, GL_FLOAT, GL_FALSE,
		sizeof(vertex_world), (void*)(sizeof(GLfloat) * 9));

	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glUniform1f(worldAmbientPass.uniIntensityFactor,
		worldAmbientFactor.Float() / CurrentMaxIntensity());

	glUniform1f(worldAmbientPass.uniIntensityExponent, worldAmbientExponent.Float());

	for(size_t i = 0; i < numVisZones; i++)
		WorldDrawZone(visZones[i]);

	// Multiplicative darkness
	/* This darkens all lights, not just ambient; it's supposed to look like the diffuse texture
	is darker */
	glVertexAttribPointer(WORLD_GLASS_PASS_ATTRIB_INTENSITY, 1, GL_FLOAT, GL_FALSE,
		sizeof(vertex_world), (void*)(sizeof(GLfloat) * 10));

	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	glUniform1f(worldAmbientPass.uniIntensityFactor, 1.0f);
	glUniform1f(worldAmbientPass.uniIntensityExponent, 1.0f);

	for(size_t i = 0; i < numVisZones; i++)
		WorldDrawZone(visZones[i]);

	WorldGlassCleanup();

	timers[TIMER_WORLD_GLASS_PASS].Stop();
}

/*--------------------------------------
	rnd::InitWorldGlassPass
--------------------------------------*/
bool rnd::InitWorldGlassPass()
{
	prog_attribute attributes[] =
	{
		{WORLD_GLASS_PASS_ATTRIB_POS, "pos"},
		{WORLD_GLASS_PASS_ATTRIB_INTENSITY, "intensity"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&worldAmbientPass.uniWorldToClip, "worldToClip"},
		{&worldAmbientPass.uniIntensityFactor, "intensityFactor"},
		{&worldAmbientPass.uniIntensityExponent, "intensityExponent"},
		{0, 0}
	};

	if(!InitProgram("world ambient glass", worldAmbientPass.shaderProgram,
	worldAmbientPass.vertexShader, &vertWorldGlassSource, 1, worldAmbientPass.fragmentShader,
	&fragWorldGlassSource, 1, attributes, uniforms))
		return false;

	//glUseProgram(worldAmbientPass.shaderProgram); // RESET

	// Reset
	//glUseProgram(0);

	return true;
}