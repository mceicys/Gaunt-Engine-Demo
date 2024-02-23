// render_anti_aliasing_pass.cpp
// Martynas Ceicys

// FIXME: optimize fxaa_grain.h, it's twice as slow as original FXAA

#include "render.h"
#include "render_private.h"

#define ANTI_ALIASING_PASS_ATTRIB_POS 0

namespace rnd
{
	void AntiAliasingPassState();
	void AntiAliasingPassCleanup();
}

static struct
{
	GLuint		vertexShader, fragmentShader, shaderProgram;
	GLint		uniSeed, uniGrainScale, uniWarpStraight, uniWarpDiagonal,
				uniFXAAQualityRcpFrame, uniFXAAQualitySubpix, uniFXAAQualityEdgeThreshold,
				uniFXAAQualityEdgeThresholdMin;
	GLint		samTex;
} antiAliasingProg = {0};

/*--------------------------------------
	rnd::AntiAliasingPass

OUTPUT:
* COLOR_BUFFER: Smoothed RGB
--------------------------------------*/
void rnd::AntiAliasingPass()
{
	if(!antiAliasingProg.shaderProgram)
		return;

	/* FIXME: Do composition pass straight into another framebuffer. Can't be geometry buffer
	since composition pass reads from it. */
	timers[TIMER_MISC].Start();
	CopyFrameBuffer(texGeometryBuffer);
	timers[TIMER_MISC].Stop();

	timers[TIMER_ANTI_ALIASING_PASS].Start();
	AntiAliasingPassState();
	glDrawArrays(GL_TRIANGLES, 0, 3);
	AntiAliasingPassCleanup();
	timers[TIMER_ANTI_ALIASING_PASS].Stop();
}

/*--------------------------------------
	rnd::AntiAliasingPassState
--------------------------------------*/
void rnd::AntiAliasingPassState()
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glUseProgram(antiAliasingProg.shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, nearScreenVertexBuffer);
	glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, texGeometryBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEnableVertexAttribArray(ANTI_ALIASING_PASS_ATTRIB_POS);

	glVertexAttribPointer(ANTI_ALIASING_PASS_ATTRIB_POS, 3, GL_FLOAT, GL_FALSE,
		sizeof(screen_vertex), (void*)0);

	glUniform2f(antiAliasingProg.uniFXAAQualityRcpFrame, 1.0f / (GLfloat)wrp::VideoWidth(),
		1.0f / (GLfloat)wrp::VideoHeight());

	glUniform1f(antiAliasingProg.uniFXAAQualityEdgeThreshold, fxaaQualityEdgeThreshold.Float());
	glUniform1f(antiAliasingProg.uniFXAAQualityEdgeThresholdMin, fxaaQualityEdgeThresholdMin.Float());

	if(antiAliasingProg.uniFXAAQualitySubpix != -1)
		glUniform1f(antiAliasingProg.uniFXAAQualitySubpix, fxaaQualitySubpix.Float());

	if(antiAliasingProg.uniSeed != -1)
		glUniform1f(antiAliasingProg.uniSeed, shaderRandomSeed);

	if(antiAliasingProg.uniGrainScale != -1)
		glUniform1f(antiAliasingProg.uniGrainScale, 1.0f / warpGrainSize.Float());

	if(antiAliasingProg.uniWarpStraight != -1)
		glUniform1f(antiAliasingProg.uniWarpStraight, warpAmount.Float() * warpStraight.Float());

	if(antiAliasingProg.uniWarpDiagonal != -1)
		glUniform1f(antiAliasingProg.uniWarpDiagonal, warpAmount.Float() * warpDiagonal.Float());
}

/*--------------------------------------
	rnd::AntiAliasingPassCleanup
--------------------------------------*/
void rnd::AntiAliasingPassCleanup()
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glDisableVertexAttribArray(ANTI_ALIASING_PASS_ATTRIB_POS);
}

/*--------------------------------------
	rnd::ReinitAntiAliasingProgram
--------------------------------------*/
bool rnd::ReinitAntiAliasingProgram()
{
	DeleteAntiAliasingProgram();

	prog_attribute attributes[] =
	{
		{ANTI_ALIASING_PASS_ATTRIB_POS, "pos"},
		{0, 0}
	};

	int ca = antiAliasing.Integer();

	prog_uniform uniforms[] =
	{
		{&antiAliasingProg.uniSeed, "seed", true},
		{&antiAliasingProg.uniGrainScale, "grainScale", true},
		{&antiAliasingProg.uniWarpStraight, "warpStraight", true},
		{&antiAliasingProg.uniWarpDiagonal, "warpDiagonal", true},
		{&antiAliasingProg.uniFXAAQualityRcpFrame, "fxaaQualityRcpFrame"},
		{&antiAliasingProg.uniFXAAQualitySubpix, "fxaaQualitySubpix", true},
		{&antiAliasingProg.uniFXAAQualityEdgeThreshold, "fxaaQualityEdgeThreshold"},
		{&antiAliasingProg.uniFXAAQualityEdgeThresholdMin, "fxaaQualityEdgeThresholdMin"},
		{&antiAliasingProg.samTex, "tex"},
		{0, 0}
	};

	const char* fxaaPath = ca == ANTI_ALIASING_FXAA ? "shaders/fxaa_3_11.h" : "shaders/fxaa_grain.h";
	FILE* fxaaFile = fopen(fxaaPath, "r");

	if(!fxaaFile)
	{
		con::LogF("Could not open %s", fxaaPath);
		goto fail;
	}

	com::Arr<char> fxaaSource(86000);
	com::FReadAll(fxaaSource, fxaaFile);
	size_t len = strlen(fxaaSource.o);

	const char* fragSrcs[] = {
		fragFXAAPrependSource,
		ca == ANTI_ALIASING_SOFT_FXAA || ca == ANTI_ALIASING_WARPED_FXAA ? fragFXAAPrependBlurSource : "",
		ca == ANTI_ALIASING_WARPED_FXAA ? fragFXAAPrependWarpSource : "", // FIXME: set these in SetShaderDefines
		shaderDefineFXAAGather4,
		"\n",
		fxaaSource.o,
		ca == ANTI_ALIASING_FXAA ? fragFXAAAppendSource : fragGrainSource
	};

	bool progInit = InitProgram("anti-aliasing", antiAliasingProg.shaderProgram,
		antiAliasingProg.vertexShader, &vertFXAASource, 1, antiAliasingProg.fragmentShader,
		fragSrcs, sizeof(fragSrcs) / sizeof(char*), attributes, uniforms);

	fxaaSource.Free();

	if(!progInit)
		goto fail;

	glUseProgram(antiAliasingProg.shaderProgram); // RESET

	// Set constant uniforms
	glUniform1i(antiAliasingProg.samTex, RND_VARIABLE_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;

fail:
	con::LogF("Failed to initialize anti-aliasing program, reverting to no anti-aliasing");
	antiAliasing.SetValue(ANTI_ALIASING_NONE);
	DeleteAntiAliasingProgram();
	return false;
}

/*--------------------------------------
	rnd::DeleteAntiAliasingProgram
--------------------------------------*/
void rnd::DeleteAntiAliasingProgram()
{
	if(antiAliasingProg.shaderProgram) glDeleteProgram(antiAliasingProg.shaderProgram);
	if(antiAliasingProg.vertexShader) glDeleteShader(antiAliasingProg.vertexShader);
	if(antiAliasingProg.fragmentShader) glDeleteShader(antiAliasingProg.fragmentShader);

	memset(&antiAliasingProg, 0, sizeof(antiAliasingProg));
}

/*--------------------------------------
	rnd::EnsureAntiAliasingProgram
--------------------------------------*/
void rnd::EnsureAntiAliasingProgram()
{
	if(!antiAliasingProg.shaderProgram && antiAliasing.Integer() != ANTI_ALIASING_NONE)
		ReinitAntiAliasingProgram();
}