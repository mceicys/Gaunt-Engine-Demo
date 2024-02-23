// render_world_template.h
// Martynas Ceicys

/*--------------------------------------
	rnd::WorldDrawZoneTextured
--------------------------------------*/
template <typename pass>
void rnd::WorldDrawZoneTextured(pass& p, const scn::Zone* z)
{
	if(!z->rndReg)
		return;

	for(size_t i = 0; i < z->rndReg->numDrawBatches; i++)
	{
		zone_reg::draw_batch& batch = z->rndReg->drawBatches[i];
		TextureGL* tex = (TextureGL*)batch.tex;
		glBindTexture(GL_TEXTURE_2D, tex->texName);

		if(p.uniTexDims != -1)
		{
			const uint32_t* dims = tex->Dims();
			glUniform2f(p.uniTexDims, dims[0], dims[1]);
		}

		glDrawElements(GL_TRIANGLES, batch.numElements, GL_UNSIGNED_INT,
			(GLvoid*)batch.byteOffset);
	}
}