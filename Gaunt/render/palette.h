// palette.h
// Martynas Ceicys

#ifndef RND_PALETTE_H
#define RND_PALETTE_H

#include <stdint.h>
#include "../resource/resource.h"

namespace rnd
{

/*======================================
	rnd::Palette

256 colors and alternative arrangements (sub-palettes and ramps) of those colors. Textures only
specify indices, the palette is used to get the RGB color.
======================================*/
class Palette : public res::Resource<Palette>
{
public:
	const char*		FileName() const {return fileName;}
	uint32_t		NumSubPalettes() const {return numSubPalettes;}
	unsigned char	NumFirstBrights() const {return numFirstBrights;}
	uint32_t		MaxRampSpan() const {return maxRampSpan;}

protected:
	char*			fileName;
	uint32_t		numSubPalettes, maxRampSpan;
	unsigned char	numFirstBrights; // Number of first colors in palette tied to bright ramps
									// Currently unused; might be useful to shader
										// FIXME: remove

					Palette(const char* fileName, uint32_t numSubPalettes,
						unsigned char numFirstBrights, uint32_t maxRampSpan);
					~Palette();

private:
					Palette();
					Palette(const Palette&);
};

}

#endif