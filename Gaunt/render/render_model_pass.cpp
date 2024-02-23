// render_model_pass.cpp
// Martynas Ceicys

#include "render.h"
#include "render_private.h"
#include "../console/console.h"
#include "../hit/hit.h"
#include "../wrap/wrap.h"

namespace rnd
{
	/*======================================
		rnd::CloudPassClass
	======================================*/
	static class CloudPassClass
	{
	public:
		GLuint	vertexShader, fragmentShader, shaderProgram;
		GLint	uniLerp, uniTexShift, uniModelToClip, uniModelToNormal, uniModelCam,
				uniModelToVoxel, uniVoxelInvDims, uniVoxelScale, uniSubPalette, uniOpacity,
				uniDither;
		GLint	samVoxels, samTexture, samSubPalettes;

		void UpdateStateForEntity(const scn::Entity& ent, const size_t (&offsets)[2],
			float lerp, const GLfloat (&wtc)[16], const com::Qua* normOri)
		{
			ModelSetPlaceVertexAttribPointers(*ent.Mesh(), offsets);
			ModelSetTexcoordVertexAttribPointer(*ent.Mesh());
			ModelUploadMTCAndMTN(uniModelToClip, uniModelToNormal, ent, wtc, normOri);
			ModelUploadLerp(uniLerp, lerp);
			ModelUploadTexShift(uniTexShift, ent);
			ModelUploadSubPalette(uniSubPalette, ent);
			ModelUploadOpacity(uniOpacity, ent);

			CloudSetVoxelState(uniModelCam, uniModelToVoxel, uniVoxelInvDims, uniVoxelScale,
				ent);
		}

		void UndoStateForEntity(const scn::Entity&) {}
	} cloudPass = {};

	RegularModelProg modelProg = {0};

	bool InitCloudPass();
}

/*--------------------------------------
	rnd::RegularModelProg::UpdateStateForEntity
--------------------------------------*/
void rnd::RegularModelProg::UpdateStateForEntity(const scn::Entity& ent,
	const size_t (&offsets)[2], float lerp, const GLfloat (&wtc)[16], const com::Qua* normOri)
{
	ModelSetPlaceVertexAttribPointers(*ent.Mesh(), offsets);
	ModelSetTexcoordVertexAttribPointer(*ent.Mesh());
	ModelUploadMTCAndMTN(uniModelToClip, uniModelToNormal, ent, wtc, 0);
	ModelUploadLerp(uniLerp, lerp);
	ModelUploadTexShift(uniTexShift, ent);
	ModelUploadTexDims(uniTexDims, ent);
	ModelUploadSubPalette(uniSubPalette, ent);
	ModelUploadOpacity(uniOpacity, ent);
}

/*--------------------------------------
	rnd::ModelUploadMTC

Model-to-clip
--------------------------------------*/
void rnd::ModelUploadMTC(GLint uniMTC, const scn::Entity& ent, const GLfloat (&wtc)[16])
{
	GLfloat modelToWorld[16];
	ModelToWorld(ent.FinalPos(), ent.FinalOri(), ent.FinalScale(), modelToWorld);
	GLfloat modelToClip[16];
	com::Multiply4x4(modelToWorld, wtc, modelToClip);
	glUniformMatrix4fv(uniMTC, 1, GL_FALSE, modelToClip);
}

/*--------------------------------------
	rnd::ModelUploadMTCAndMTN

Model-to-clip and model-to-normal
--------------------------------------*/
void rnd::ModelUploadMTCAndMTN(GLint uniMTC, GLint uniMTN, const scn::Entity& ent,
	const GLfloat (&wtc)[16], const com::Qua* normOri)
{
	GLfloat modelToWorld[16], modelToNormal[16];

	ModelToWorld(ent.FinalPos(), ent.FinalOri(), ent.FinalScale(), modelToWorld,
		&modelToNormal, normOri);

	glUniformMatrix4fv(uniMTN, 1, GL_FALSE, modelToNormal);
	GLfloat modelToClip[16];
	com::Multiply4x4(modelToWorld, wtc, modelToClip);
	glUniformMatrix4fv(uniMTC, 1, GL_FALSE, modelToClip);
}

/*--------------------------------------
	rnd::ModelUploadLerp
--------------------------------------*/
void rnd::ModelUploadLerp(GLint uniLerp, float lerp)
{
	glUniform1f(uniLerp, lerp);
}

/*--------------------------------------
	rnd::ModelUploadTexShift
--------------------------------------*/
void rnd::ModelUploadTexShift(GLint uniTexShift, const scn::Entity& ent)
{
	com::Vec2 uv = ent.FinalUV();
	glUniform2f(uniTexShift, uv.x, uv.y);
}

/*--------------------------------------
	rnd::ModelUploadTexDims
--------------------------------------*/
void rnd::ModelUploadTexDims(GLint uniTexDims, const scn::Entity& ent)
{
	const uint32_t* dims = ent.tex->Dims();
	glUniform2f(uniTexDims, dims[0], dims[1]);
}

/*--------------------------------------
	rnd::ModelUploadSubPalette
--------------------------------------*/
void rnd::ModelUploadSubPalette(GLint uniSubPalette, const scn::Entity& ent)
{
	glUniform1f(uniSubPalette, NormalizeSubPalette(ent.subPalette));
}

/*--------------------------------------
	rnd::ModelUploadOpacity
--------------------------------------*/
void rnd::ModelUploadOpacity(GLint uniOpacity, const scn::Entity& ent)
{
	glUniform1f(uniOpacity, ent.FinalOpacity());
}

/*--------------------------------------
	rnd::CloudSetVoxelState
--------------------------------------*/
void rnd::CloudSetVoxelState(GLint uniModelCam, GLint uniModelToVoxel, GLint uniVoxelInvDims,
	GLint uniVoxelScale, const scn::Entity& ent)
{
	const MeshGL& msh = *((MeshGL*)ent.Mesh());
	glActiveTexture(RND_VARIABLE_2_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_3D, msh.texVoxels);
	com::Vec3 modelCam = ModelCamPos(ent, *scn::ActiveCamera());
	glUniform3f(uniModelCam, modelCam.x, modelCam.y, modelCam.z);
	GLfloat invScale = 1.0f / msh.voxelScale;

	GLfloat modelToVoxel[16] = {
		invScale,	0.0f,		0.0f,		-msh.voxelOrigin[0],
		0.0f,		invScale,	0.0f,		-msh.voxelOrigin[1],
		0.0f,		0.0f,		invScale,	-msh.voxelOrigin[2],
		0.0f,		0.0f,		0.0f,		1.0f
	};

	glUniformMatrix4fv(uniModelToVoxel, 1, GL_FALSE, modelToVoxel);

	glUniform3f(uniVoxelInvDims, msh.voxelInvDims[0], msh.voxelInvDims[1],
		msh.voxelInvDims[2]);

	glUniform1f(uniVoxelScale, msh.voxelScale);
}

/*--------------------------------------
	rnd::ModelVertexAttribOffset
--------------------------------------*/
void rnd::ModelVertexAttribOffset(const scn::Entity& ent, const MeshGL& msh,
	size_t (&offsetsOut)[2], float& lerpOut)
{
	size_t frames[2];
	float time = wrp::timeStep.Float();

	if(ent.flags & scn::Entity::LERP_FRAME)
		time *= wrp::Fraction();

	float frame = ent.Frame(), trans = ent.TransitionTime();
	frames[0] = ent.TransitionA();
	frames[1] = ent.TransitionB();
	lerpOut = ent.TransitionF();
	AdvanceTrack(time, ent.Animation(), frame, ent.animSpeed, (ent.flags & ent.LOOP_ANIM) != 0,
		trans, frames[0], frames[1], lerpOut);

	if((ent.flags & scn::Entity::LERP_FRAME) == 0)
		lerpOut = 0.0f;

	size_t maxFrame = msh.NumFrames() - 1;
	size_t frameSize = sizeof(vertex_mesh_place) * (size_t)msh.numFrameVerts;

	for(size_t i = 0; i < 2; i++)
		offsetsOut[i] = msh.texDataSize + COM_MIN(frames[i], maxFrame) * frameSize;
}

/*--------------------------------------
	rnd::ModelSetPlaceVertexAttribPointers

Pass's UpdateStateForEntity should call this if it's using the standard model vertex attributes:
pos0, pos1, norm0, norm1.
--------------------------------------*/
void rnd::ModelSetPlaceVertexAttribPointers(const Mesh& msh, const size_t (&offsets)[2])
{
	for(size_t j = 0; j < 2; j++)
	{
		glVertexAttribPointer(RND_MODEL_PASS_ATTRIB_POS0 + j, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_mesh_place), (void*)offsets[j]);

		glVertexAttribPointer(RND_MODEL_PASS_ATTRIB_NORM0 + j, 3, GL_FLOAT, GL_FALSE,
			sizeof(vertex_mesh_place), (void*)(offsets[j] + sizeof(GLfloat) * 3));
	}
}

/*--------------------------------------
	rnd::ModelSetTexcoordVertexAttribPointer
--------------------------------------*/
void rnd::ModelSetTexcoordVertexAttribPointer(const Mesh& msh)
{
	glVertexAttribPointer(RND_MODEL_PASS_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertex_mesh_tex), (void*)0);
}

/*--------------------------------------
	rnd::ModelDrawElements

All GL state must be set for drawing before calling.
--------------------------------------*/
void rnd::ModelDrawElements(const MeshGL& msh)
{
	glDrawElements(GL_TRIANGLES, msh.numFrameIndices, msh.indexType, 0);
}

/*--------------------------------------
	rnd::ModelToWorld

mtnOut can be given to get a model-to-normal matrix as well.
--------------------------------------*/
void rnd::ModelToWorld(const com::Vec3& pos, const com::Qua& ori, float scale,
	GLfloat (&mtwOut)[16], GLfloat (*mtnOut)[16], const com::Qua* normOri)
{
	if(ori.Identity())
		com::Identity4x4(mtwOut);
	else
		com::MatQua(ori, mtwOut);

	if(mtnOut)
	{
		if(!normOri || normOri->Identity())
			for(size_t i = 0; i < 16; (*mtnOut)[i] = mtwOut[i], i++);
		else
			com::MatQua((*normOri * ori).Normalized(), *mtnOut);
	}

	if(scale != 1.0f)
	{
		mtwOut[0] *= scale;
		mtwOut[1] *= scale;
		mtwOut[2] *= scale;
		mtwOut[4] *= scale;
		mtwOut[5] *= scale;
		mtwOut[6] *= scale;
		mtwOut[8] *= scale;
		mtwOut[9] *= scale;
		mtwOut[10] *= scale;
	}

	mtwOut[3] = pos.x;
	mtwOut[7] = pos.y;
	mtwOut[11] = pos.z;
}

void rnd::ModelToWorld(const scn::Entity& ent, GLfloat (&mOut)[16])
{
	ModelToWorld(ent.FinalPos(), ent.FinalOri(), ent.FinalScale(), mOut);
}

/*--------------------------------------
	rnd::ModelCamPos
--------------------------------------*/
com::Vec3 rnd::ModelCamPos(const scn::Entity& ent, const scn::Camera& cam)
{
	com::Vec3 modelCam = cam.FinalPos();
	float scale = ent.FinalScale();

	if(scale)
	{
		modelCam -= ent.FinalPos();
		modelCam /= scale;
		modelCam = com::VecRotInv(modelCam, ent.FinalOri());
	}
	else
		modelCam = 0.0f;

	return modelCam;
}

/*--------------------------------------
	rnd::ModelState

All state set here is constant until the model pass is done.

FIXME: this is called in other passes, resulting in redundant state setting
--------------------------------------*/
void rnd::ModelState()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glEnable(GL_CULL_FACE);
	glEnable(GL_STENCIL_TEST);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS0);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS1);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_TEXCOORD);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM0);
	glEnableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM1);
}

/*--------------------------------------
	rnd::ModelCleanup
--------------------------------------*/
void rnd::ModelCleanup()
{
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS0);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_POS1);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_TEXCOORD);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM0);
	glDisableVertexAttribArray(RND_MODEL_PASS_ATTRIB_NORM1);
	glDisable(GL_STENCIL_TEST);
}

/*--------------------------------------
	rnd::ModelPass

Draws visible entities.

OUTPUT:
* COLOR_BUFFER: Scene color indices (r) and normals (g, b, a)
* DEPTH_BUFFER: Scene depth
--------------------------------------*/
void rnd::ModelPass()
{
	timers[TIMER_MODEL_PASS].Start();

	const Mesh* curMsh = 0;
	const Texture* curTex = 0;
	ModelState();

	// Regular model pass
	if(numVisEnts)
	{
		glUseProgram(modelProg.shaderProgram);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_GEQUAL, 0, -1); // Don't draw over overlay stencils

		// FIXME: need to set these whenever modelProg is used (e.g. overlay pass)
		if(modelProg.uniMipBias != -1)
			glUniform1f(modelProg.uniMipBias, mipBias.Float());

		if(modelProg.uniMipFade != -1)
			glUniform1f(modelProg.uniMipFade, mipFade.Float());

		PaletteGL* curPal = CurrentPaletteGL();

		if(modelProg.uniRampDistScale != -1)
			glUniform1f(modelProg.uniRampDistScale, curPal->rampDistScale);

		if(modelProg.uniInvNumRampTexels != -1)
			glUniform1f(modelProg.uniInvNumRampTexels, curPal->invNumRampTexels);
	
		ModelDrawArray<0, true>(modelProg, gWorldToClip, 0, visEnts.o, numVisEnts, curMsh, curTex);
	}

	// Cloud pass
	if(numVisCloudEnts)
	{
		timers[TIMER_MODEL_PASS].Stop();
		timers[TIMER_CLOUD_PASS].Start();

		//glDisable(GL_CULL_FACE);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilFunc(GL_GEQUAL, cloudMask, -1);
		glStencilMask(cloudMask);
		glUseProgram(cloudPass.shaderProgram);

		ModelDrawArray<0, true>(cloudPass, gWorldToClip, 0, visCloudEnts.o, numVisCloudEnts,
			curMsh, curTex);

		glStencilMask(-1);
		//glEnable(GL_CULL_FACE);
	}

	ModelCleanup();

	timers[numVisCloudEnts ? TIMER_CLOUD_PASS : TIMER_MODEL_PASS].Stop();
}

/*--------------------------------------
	rnd::FilterNoGlass
--------------------------------------*/
bool rnd::FilterNoGlass(const scn::Entity& ent)
{
	return !ent.Glass();
}

/*--------------------------------------
	rnd::OverlayPass

Draws overlay entities.

If the overlay's RELIT flag is set, overlay entities will be lit from the overlay camera's
viewpoint. That is, the active camera's position and orientation do not affect how the overlay
is lit even though it appears to the user that the entities are moving with the active camera.
The drawn normals are the real-world normals of the overlaid entity, not the apparent on-screen
normals, and lights affecting the overlay entity use a matrix relative to the overlay camera
instead of the active camera.

If RELIT is not set, overlay entities will be lit from the active camera's viewpoint, as if the
on-screen placement of each entity is its real-world placement. The drawn normals are rotated by
the active camera's orientation relative to the overlay camera's, and lights use a matrix
relative to the active camera, how the scene is normally lit.
--------------------------------------*/
void rnd::OverlayPass()
{
	timers[TIMER_OVERLAY_PASS].Start();

	overlayRelitMask = 0;
	const Mesh* curMsh = 0;
	const Texture* curTex = 0;
	size_t i = 0;

	for(; i < scn::NUM_OVERLAYS; i++)
	{
		if(scn::Overlays()[i].cam)
			break;
	}

	if(i == scn::NUM_OVERLAYS)
		return;

	ModelState();
	glUseProgram(modelProg.shaderProgram);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	com::Qua actOri = scn::ActiveCamera()->FinalOri();

	for(size_t i = 0; i < scn::NUM_OVERLAYS; i++)
	{
		const scn::Overlay& ovr = scn::Overlays()[i];

		if(ovr.cam)
		{
			GLint ref = 1 << (overlayStartBit - i);
			glStencilFunc(GL_GEQUAL, ref, -1);
			com::Qua normOri = com::QUA_IDENTITY;

			if(ovr.flags & ovr.RELIT)
				overlayRelitMask |= ref; // Stencil out overlay when doing regular lighting
			else
			{
				// Rotate normals by active cam's orientation relative to overlay cam's
				normOri = (actOri * ovr.cam->FinalOri().Conjugate()).Normalized();
			}

			// Filtering out GLASS here because overlay glass is drawn in the glass pass
			ModelDrawArray<FilterNoGlass, true>(modelProg, gOverlayWTC[i], &normOri,
				ovr.Entities(), ovr.NumEntities(), curMsh, curTex);
		}
	}

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	ModelCleanup();

	timers[TIMER_OVERLAY_PASS].Stop();
}

/*--------------------------------------
	rnd::InitModelPass
--------------------------------------*/
bool rnd::InitModelPass()
{
	if(!ReinitModelProgram())
	{
		con::AlertF("Model program initialization failed");
		return false;
	}

	if(!InitCloudPass())
	{
		con::AlertF("Cloud pass initialization failed");
		return false;
	}

	return true;
}

/*--------------------------------------
	rnd::ReinitModelProgram
--------------------------------------*/
bool rnd::ReinitModelProgram()
{
	DeleteModelProgram();

	prog_attribute attributes[] =
	{
		{RND_MODEL_PASS_ATTRIB_POS0, "pos0"},
		{RND_MODEL_PASS_ATTRIB_POS1, "pos1"},
		{RND_MODEL_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{RND_MODEL_PASS_ATTRIB_NORM0, "norm0"},
		{RND_MODEL_PASS_ATTRIB_NORM1, "norm1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&modelProg.uniLerp, "lerp"},
		{&modelProg.uniTexShift, "texShift"},
		{&modelProg.uniModelToClip, "modelToClip"},
		{&modelProg.uniModelToNormal, "modelToNormal"},
		{&modelProg.uniTexDims, "texDims"},
		{&modelProg.uniSubPalette, "subPalette"},
		{&modelProg.uniOpacity, "opacity"},
		{&modelProg.uniDither, "dither"},
		{&modelProg.uniMipBias, "mipBias"},
		{&modelProg.uniMipFade, "mipFade"},
		{&modelProg.uniRampDistScale, "rampDistScale"},
		{&modelProg.uniInvNumRampTexels, "invNumRampTexels"},
		{&modelProg.samTexture, "texture"},
		{&modelProg.samSubPalettes, "subPalettes"},
		{&modelProg.samRampLookup, "rampLookup"},
		{&modelProg.samRamps, "ramps"},
		{0, 0}
	};

	const char* fragSrcs[] = {
		"#version 130", // FIXME: do 120 if all defines are 0 (except MIP_MAPPING)
		shaderDefineMipMapping,
		shaderDefineMipFade,
		shaderDefineFilterBrightness,
		shaderDefineFilterColor,
		shaderDefineAnchorTexelNormals,
		"\n",
		fragModelSource
	};

	if(!InitProgram("model", modelProg.shaderProgram, modelProg.vertexShader, &vertModelSource,
	1, modelProg.fragmentShader, fragSrcs, sizeof(fragSrcs) / sizeof(char*), attributes,
	uniforms))
		goto fail;

	glUseProgram(modelProg.shaderProgram); // RESET

	// Set constant uniforms
	GLfloat dither[16];
	MakeDitherArray(dither);

	glUniform1fv(modelProg.uniDither, 16, dither);
	glUniform1i(modelProg.samTexture, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(modelProg.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);
	glUniform1i(modelProg.samRamps, RND_RAMP_TEXTURE_NUM);
	glUniform1i(modelProg.samRampLookup, RND_RAMP_LOOKUP_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;

fail:
	// FIXME: set low quality options and try again like in ReinitWorldProgram
	DeleteModelProgram();
	return false;
}

/*--------------------------------------
	rnd::DeleteModelProgram
--------------------------------------*/
void rnd::DeleteModelProgram()
{
	if(modelProg.shaderProgram) glDeleteProgram(modelProg.shaderProgram);
	if(modelProg.vertexShader) glDeleteShader(modelProg.vertexShader);
	if(modelProg.fragmentShader) glDeleteShader(modelProg.fragmentShader);

	memset(&modelProg, 0, sizeof(modelProg));
}

/*--------------------------------------
	rnd::EnsureModelProgram
--------------------------------------*/
void rnd::EnsureModelProgram()
{
	if(!modelProg.shaderProgram)
	{
		if(!ReinitModelProgram())
			WRP_FATAL("Could not compile model shader program");
	}
}

/*--------------------------------------
	rnd::InitCloudPass
--------------------------------------*/
bool rnd::InitCloudPass()
{
	prog_attribute attributes[] =
	{
		{RND_MODEL_PASS_ATTRIB_POS0, "pos0"},
		{RND_MODEL_PASS_ATTRIB_POS1, "pos1"},
		{RND_MODEL_PASS_ATTRIB_TEXCOORD, "texCoord"},
		{RND_MODEL_PASS_ATTRIB_NORM0, "norm0"},
		{RND_MODEL_PASS_ATTRIB_NORM1, "norm1"},
		{0, 0}
	};

	prog_uniform uniforms[] =
	{
		{&cloudPass.uniLerp, "lerp"},
		{&cloudPass.uniTexShift, "texShift"},
		{&cloudPass.uniModelToClip, "modelToClip"},
		{&cloudPass.uniModelToNormal, "modelToNormal"},
		{&cloudPass.uniModelCam, "modelCam"},
		{&cloudPass.uniModelToVoxel, "modelToVoxel"},
		{&cloudPass.uniVoxelInvDims, "voxelInvDims"},
		{&cloudPass.uniVoxelScale, "voxelScale"},
		{&cloudPass.uniSubPalette, "subPalette"},
		{&cloudPass.uniOpacity, "opacity"},
		{&cloudPass.uniDither, "dither"},
		{&cloudPass.samVoxels, "voxels"},
		{&cloudPass.samTexture, "texture"},
		{&cloudPass.samSubPalettes, "subPalettes"},
		{0, 0}
	};

	if(!InitProgram("cloud model", cloudPass.shaderProgram, cloudPass.vertexShader,
	&vertModelCloudSource, 1, cloudPass.fragmentShader, &fragModelCloudSource, 1, attributes,
	uniforms))
		return false;

	glUseProgram(cloudPass.shaderProgram); // RESET

	// Set constant uniforms
	GLfloat dither[16];
	MakeDitherArray(dither);

	glUniform1fv(cloudPass.uniDither, 16, dither);
	glUniform1i(cloudPass.samVoxels, RND_VARIABLE_2_TEXTURE_NUM);
	glUniform1i(cloudPass.samTexture, RND_VARIABLE_TEXTURE_NUM);
	glUniform1i(cloudPass.samSubPalettes, RND_SUB_PALETTES_TEXTURE_NUM);

	// Reset
	glUseProgram(0);

	return true;
}