// render_restore_pass.cpp
// Martynas Ceicys

#include "render_private.h"

#define RESTORE_PASS_ATTRIB_POS 0

namespace rnd
{
	struct vertex_r
	{
		GLfloat pos[3];
	};
}

static struct
{
	GLuint	vertexShader, fragmentShader, shaderProgram;
	GLint	uniVideoDimensions;
	GLint	samTex;
	GLuint	vertBuffer;
} restorePass = {0};

/*--------------------------------------
	rnd::RestoreDepthBuffer

Draws texture on RND_VARIABLE_2_TEXTURE_UNIT to the depth buffer. Viewport must match video and
texture dimensions. Only vertex attrib 0 can be enabled. It is disabled after the call.
glDepthFunc needs to be reset after calling.
--------------------------------------*/
void rnd::RestoreDepthBuffer()
{
	glUseProgram(restorePass.shaderProgram);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);
	glCullFace(GL_BACK);
	glUniform2f(restorePass.uniVideoDimensions, wrp::VideoWidth(), wrp::VideoHeight());
	glBindBuffer(GL_ARRAY_BUFFER, restorePass.vertBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glVertexAttribPointer(RESTORE_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_r),
		(void*)0);
	glEnableVertexAttribArray(RESTORE_PASS_ATTRIB_POS);

	glDrawArrays(GL_QUADS, 0, 4);

	glDisableVertexAttribArray(RESTORE_PASS_ATTRIB_POS);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
	//glDepthFunc(GL_LEQUAL); // Not reset since state is unknown
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

/*--------------------------------------
	rnd::InitRestorePass
--------------------------------------*/
bool rnd::InitRestorePass()
{
	prog_attribute attributes[] =
	{
		{RESTORE_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&restorePass.uniVideoDimensions, "videoDimensions"},
		{&restorePass.samTex, "tex"},
		{0, 0}
	};

	if(!InitProgram("restore", restorePass.shaderProgram, restorePass.vertexShader,
	&vertRestoreSource, 1, restorePass.fragmentShader, &fragRestoreSource, 1, attributes,
	uniforms))
		return false;

	glUseProgram(restorePass.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(restorePass.samTex, RND_VARIABLE_2_TEXTURE_NUM);

	// Buffers
	glGenBuffers(1, &restorePass.vertBuffer);

	// Create screen quad
	// FIXME: use nearScreenVertexBuffer triangle
	glBindBuffer(GL_ARRAY_BUFFER, restorePass.vertBuffer); // RESET

	vertex_r quadVerts[4] = {
		{-1.0f, -1.0f, 0.0f},
		{1.0f, -1.0f, 0.0f},
		{1.0f, 1.0f, 0.0f},
		{-1.0f, 1.0f, 0.0f}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_r) * 4, quadVerts, GL_STATIC_DRAW);

	// Reset
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

	return true;
}