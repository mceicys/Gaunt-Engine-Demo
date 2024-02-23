// gui.h
// Martynas Ceicys

// FIXME: remove interface classes, add constructors and destructors

#ifndef GUI_H
#define GUI_H

#include "gui.h"
#include "../render/regs.h"
#include "../render/texture.h"
#include "../resource/resource.h"
#include "../vector/vec_lua.h"

namespace gui
{

/*
################################################################################################


	IMAGE


################################################################################################
*/

/*======================================
	gui::iImage

2D texture occupying any part of the window. All UI elements are drawn with images.
======================================*/
class iImage : public res::Resource<iImage>
{
public:
	com::Vec3				pos; // bottom left origin, in screen pixels, z is depth [0.0, 1.0]
	int						scale;
	unsigned int			frame;
	int						subPalette;
	bool					visible; // FIXME: bit flags

	virtual rnd::Texture*	Texture() const = 0;
	virtual void			SetTexture(rnd::Texture* tex) = 0;
};

iImage* CreateImage(rnd::Texture* tex);
void	DeleteImage(iImage*& image);

/*
################################################################################################


	TEXT


################################################################################################
*/

/*======================================
	gui::iText

Row of images with frames corresponding to chars in specified buffer.
Texture should be set to a monospaced font.

FIXME: Getting text to display is too cumbersome and complicated. Simplify interface (or what's
	required to get started).
FIXME: Rename so abbreviation isn't confused with texture
======================================*/
class iText : public res::Resource<iText>
{
public:
	virtual com::Vec3		Pos()												= 0;
	virtual int				Scale()												= 0;
	virtual int				SubPalette()										= 0;
	virtual bool			Visible()											= 0;
	virtual int				CharWidth()											= 0;
	virtual const char*		String()											= 0;
	virtual unsigned		StringLength()										= 0;
	virtual rnd::Texture*	Texture()											= 0;
	virtual unsigned int	ImageCount()										= 0;

	virtual void			SetPos(const com::Vec3 u)							= 0;
	virtual void			SetScale(const int i)								= 0;
	virtual void			SetSubPalette(const int i)							= 0;
	virtual void			SetVisible(const bool b)							= 0;
	virtual void			SetCharWidth(const int i)							= 0;
	virtual size_t			SetString(const char* str, size_t n = -1, const char* delim = 0, bool expand = true) = 0;
	virtual void			SetTexture(rnd::Texture* tex)						= 0;
	virtual void			SetImageCount(const unsigned int count)				= 0;
};

iText*	CreateText(rnd::Texture* texture, int charPixelWidth);
void	DeleteText(iText*& text);

/*
################################################################################################
	QUICK DRAW
################################################################################################
*/

void Draw2DText(const com::Vec2& pos, const char* str, int scale = 1, int subPalette = 0,
	float time = 0.0f);

void Draw3DText(const com::Vec3& pos, const char* str, int scale = 1, int subPalette = 0,
	float time = 0.0f);

void QuickDrawInit();
void QuickDrawAdvance();

/*
################################################################################################
	GENERAL
################################################################################################
*/

void Init();

}

#endif