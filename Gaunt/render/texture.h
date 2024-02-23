// texture.h
// Martynas Ceicys

#ifndef RND_TEXTURE_H
#define RND_TEXTURE_H

#include <stdint.h>
#include "../resource/resource.h"

namespace rnd
{

/*======================================
	rnd::Texture

2D color map with frame information
======================================*/
class Texture : public res::Resource<Texture>
{
public:
	enum
	{
		ALPHA = 1 << 0, // At least one texel has discard color 0
		MIP = 1 << 1 // Display mipmap
	};

	struct frame
	{
		uint32_t corner[2]; // top left
		uint32_t dims[2];
		int32_t origin[2];
	};

	const char*		FileName() const {return fileName;}
	const char*		ShortName() const {return shortName;}
	const uint32_t*	Dims() const {return dims;}
	uint32_t		NumFrames() const {return numFrames;}
	const frame*	Frames() const {return frames;}
	bool			Alpha() const {return (flags & ALPHA) != 0;}
	bool			Mip() const {return (flags & MIP) != 0;}
	void			SetMip(bool b);

protected:

	char*			fileName;
	const char*		shortName; // File name without directories
	uint32_t		dims[2];
	uint32_t		numFrames;
	frame*			frames;
	uint16_t		flags;

					Texture(const char* fileName, const uint32_t (&dims)[2], uint32_t numFrames,
						frame* frames, uint16_t flags);
					~Texture();

private:
					Texture();
					Texture(const Texture&);
};

}

#endif