// render_model_pass_template.h
// Martynas Ceicys

/*--------------------------------------
	rnd::ModelDrawArray
--------------------------------------*/
template <bool (*Filter)(const scn::Entity&), bool bindTex, class Pass, class EntPtr>
void ModelDrawArray(Pass& p, const GLfloat (&wtc)[16], const com::Qua* normOri,
	const EntPtr* ents, size_t numEnts, const Mesh*& curMsh, const Texture*& curTex)
{
	for(size_t i = 0; i < numEnts; i++)
	{
		const scn::Entity& ent = *ents[i];
		const MeshGL* msh = (MeshGL*)ent.Mesh();
		const TextureGL* tex = (TextureGL*)ent.tex.Value();

		if(!(ent.flags & ent.VISIBLE) || (Filter && !Filter(ent)))
			continue;

		if(curMsh != msh)
		{
			glBindBuffer(GL_ARRAY_BUFFER, msh->vBufName);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, msh->iBufName);
			curMsh = msh;
		}

		if(bindTex && curTex != tex)
		{
			glActiveTexture(RND_VARIABLE_TEXTURE_UNIT);
				// Called every time since UpdateStateForEntity might bind other texture units
			glBindTexture(GL_TEXTURE_2D, tex->texName);
			curTex = tex;
		}

		size_t offsets[2];
		float lerp;
		ModelVertexAttribOffset(ent, *msh, offsets, lerp);
		p.UpdateStateForEntity(ent, offsets, lerp, wtc, normOri);
		ModelDrawElements(*msh);
		p.UndoStateForEntity(ent);
	}
}