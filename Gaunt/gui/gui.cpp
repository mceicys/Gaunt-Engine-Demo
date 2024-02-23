// gui.cpp
// Martynas Ceicys

#include <string.h>

#include "gui.h"
#include "../console/console.h"
#include "../../GauntCommon/io.h"
#include "../render/render.h"
#include "../script/script.h"

namespace gui
{
	// IMAGE
	class Image;

	// IMAGE LUA
	int CreateImage(lua_State* l);

	//	metaImage
	int ImgDelete(lua_State* l);
	int ImgTexture(lua_State* l);
	int ImgSetTexture(lua_State* l);
	int ImgPos(lua_State* l);
	int ImgSetPos(lua_State* l);
	int ImgScale(lua_State* l);
	int ImgSetScale(lua_State* l);
	int ImgFrame(lua_State* l);
	int ImgSetFrame(lua_State* l);
	int ImgSubPalette(lua_State* l);
	int ImgSetSubPalette(lua_State* l);
	int ImgVisible(lua_State* l);
	int ImgSetVisible(lua_State* l);

	// TEXT
	class Text;

	// TEXT LUA
	int CreateText(lua_State* l);

	//	metaText
	int TxtDelete(lua_State* l);
	int TxtPos(lua_State* l);
	int TxtSetPos(lua_State* l);
	int TxtScale(lua_State* l);
	int TxtSetScale(lua_State* l);
	int TxtSubPalette(lua_State* l);
	int TxtSetSubPalette(lua_State* l);
	int TxtVisible(lua_State* l);
	int TxtSetVisible(lua_State* l);
	int TxtCharWidth(lua_State* l);
	int TxtSetCharWidth(lua_State* l);
	int TxtString(lua_State* l);
	int TxtStringLength(lua_State* l);
	int TxtSetString(lua_State* l);
	int TxtTexture(lua_State* l);
	int TxtSetTexture(lua_State* l);
	int TxtImageCount(lua_State* l);
	int TxtSetImageCount(lua_State* l);

	// QUICK DRAW
	struct quick_text
	{
		iText*		txt;
		unsigned	time;
	};

	com::Arr<quick_text>	quickTexts;
	size_t					numQuickTexts = 0;
	res::Ptr<rnd::Texture>	quickTextFont;
	int						quickTextWidth = 0;

	void AddQuickText(const com::Vec3& user, const char* str, int scale, int subPalette,
		float time);

	// QUICK DRAW LUA
	int Draw2DText(lua_State* l);
	int Draw3DText(lua_State* l);
}

/*
################################################################################################


	IMAGE


################################################################################################
*/

const char* const gui::iImage::METATABLE = "metaImage";

/*======================================
	gui::Image
======================================*/
class gui::Image : public iImage
{
public:
	rnd::img_reg*	rndReg;

	rnd::Texture*	Texture() const {return rndReg ? rnd::RegisteredTexture(*rndReg) : 0;}
	void			SetTexture(rnd::Texture* tex);
};

/*--------------------------------------
	gui::Image::SetTexture
--------------------------------------*/
void gui::Image::SetTexture(rnd::Texture* tex)
{
	if(rndReg)
		rnd::UnregisterImage(rndReg);

	if(tex)
		rndReg = rnd::RegisterImage(*this, *tex);
}

/*--------------------------------------
	gui::CreateImage
--------------------------------------*/
gui::iImage* gui::CreateImage(rnd::Texture* tex)
{
	Image* img = new Image;

	img->pos = 0.0f;
	img->scale = 1;
	img->frame = 0;
	img->subPalette = 0;
	img->visible = true;

	if(tex)
		img->rndReg = rnd::RegisterImage(*img, *tex);
	else
		img->rndReg = 0;

	return img;
}

/*--------------------------------------
	gui::DeleteImage
--------------------------------------*/
void gui::DeleteImage(iImage*& image)
{
	Image* img = (Image*)image;
	rnd::UnregisterImage(img->rndReg);
	delete img;
	image = 0;
}

/*
################################################################################################


	IMAGE LUA


################################################################################################
*/

/*--------------------------------------
LUA	gui::CreateImage (Image)

IN	texT
OUT	img

FIXME: Actual function name should be Image since images are garbage collected
--------------------------------------*/
int gui::CreateImage(lua_State* l)
{
	rnd::Texture* t = rnd::Texture::LuaTo(1);
	iImage* img = CreateImage(t);
	img->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	gui::ImgDelete (__gc)

IN	imgI
--------------------------------------*/
int gui::ImgDelete(lua_State* l)
{
	iImage* i = iImage::UserdataLuaTo(1);

	if(!i)
		return 0;

	DeleteImage(i);
	return 0;
}

/*--------------------------------------
LUA	gui::ImgTexture (Texture)

IN	imgI
OUT	texT
--------------------------------------*/
int gui::ImgTexture(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->Texture()->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	gui::ImgSetTexture (SetTexture)

IN	imgI, texT
--------------------------------------*/
int gui::ImgSetTexture(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->SetTexture(rnd::Texture::LuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	gui::ImgPos (Pos)

IN	imgI
OUT	xPos, yPos, zPos
--------------------------------------*/
int gui::ImgPos(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	vec::LuaPushVec(l, i->pos);
	return 3;
}

/*--------------------------------------
LUA	gui::ImgSetPos (SetPos)

IN	imgI, xPos, yPos, zPos
--------------------------------------*/
int gui::ImgSetPos(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->pos = vec::LuaToVec(l, 2, 3, 4);
	return 0;
}

/*--------------------------------------
LUA	gui::ImgScale (Scale)

IN	imgI
OUT	iScale
--------------------------------------*/
int gui::ImgScale(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	lua_pushinteger(l, i->scale);
	return 1;
}


/*--------------------------------------
LUA	gui::ImgSetScale (SetScale)

IN	imgI, iScale
--------------------------------------*/
int gui::ImgSetScale(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->scale = luaL_checkinteger(l, 2);
	return 0;
}

/*--------------------------------------
LUA	gui::ImgFrame (Frame)

IN	imgI
OUT	iFrame
--------------------------------------*/
int gui::ImgFrame(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	lua_pushinteger(l, i->frame);
	return 1;
}

/*--------------------------------------
LUA	gui::ImgSetFrame (SetFrame)

IN	imgI, iFrame
--------------------------------------*/
int gui::ImgSetFrame(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->frame = lua_tointeger(l, 2);
	return 0;
}

/*--------------------------------------
LUA	gui::ImgSubPalette (SubPalette)

IN	imgI
OUT	iSubPalette
--------------------------------------*/
int gui::ImgSubPalette(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	lua_pushinteger(l, i->subPalette);
	return 1;
}

/*--------------------------------------
LUA	gui::ImgSetSubPalette (SetSubPalette)

IN	imgI, iSubPalette
--------------------------------------*/
int gui::ImgSetSubPalette(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->subPalette = lua_tointeger(l, 2);
	return 0;
}

/*--------------------------------------
LUA	gui::ImgVisible (Visible)

IN	imgI
OUT	bVisible
--------------------------------------*/
int gui::ImgVisible(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	lua_pushboolean(l, i->visible);
	return 1;
}

/*--------------------------------------
LUA	gui::ImgSetVisible (SetVisible)

IN	imgI, bVisible
--------------------------------------*/
int gui::ImgSetVisible(lua_State* l)
{
	iImage* i = iImage::CheckLuaTo(1);
	i->visible = lua_toboolean(l, 2);
	return 0;
}

/*
################################################################################################


	TEXT


################################################################################################
*/

const char* const gui::iText::METATABLE = "metaText";

/*======================================
	gui::Text
======================================*/
class gui::Text : public iText
{
public:
	com::Vec3		pos;
	int				scale;
	int				subPalette;
	bool			visible;

	int				charWidth;
	char*			string;
	size_t			stringLen;

	res::Ptr<rnd::Texture> texture;
	iImage**		images;
	unsigned int	imageCount;

	com::Vec3		Pos(){return pos;}
	int				Scale(){return scale;}
	int				SubPalette(){return subPalette;}
	bool			Visible(){return visible;}
	int				CharWidth(){return charWidth;}
	const char*		String(){return string;}
	unsigned		StringLength(){return stringLen;}
	rnd::Texture*	Texture(){return texture;}
	unsigned int	ImageCount(){return imageCount;}

	void			SetPos(const com::Vec3 u);
	void			SetScale(const int i);
	void			SetSubPalette(const int i);
	void			SetVisible(const bool b);
	void			SetCharWidth(const int i);
	size_t			SetString(const char* str, size_t n = -1, const char* delim = 0, bool expand = true);
	void			SetTexture(rnd::Texture* tex);
	void			SetImageCount(const unsigned int count);
};

/*--------------------------------------
	gui::Text::SetPos
--------------------------------------*/
void gui::Text::SetPos(const com::Vec3 u)
{
	pos = u;

	for(unsigned int i = 0; i < imageCount; i++)
	{
		images[i]->pos = pos;
		images[i]->pos.x += i * charWidth * scale;
	}
}

/*--------------------------------------
	gui::Text::SetScale
--------------------------------------*/
void gui::Text::SetScale(const int i)
{
	scale = i;
	for(unsigned int i = 0; i < imageCount; i++) images[i]->scale = scale;
	SetPos(pos);
}

/*--------------------------------------
	gui::Text::SetSubPalette
--------------------------------------*/
void gui::Text::SetSubPalette(const int i)
{
	subPalette = i;
	for(unsigned int i = 0; i < imageCount; i++) images[i]->subPalette = subPalette;
}

/*--------------------------------------
	gui::Text::SetVisible

FIXME: Should be able to call this repeatedly without looping thru images every time
	Make a separate function that sets all image visiblity without doing visible == b check
--------------------------------------*/
void gui::Text::SetVisible(const bool b)
{
	visible = b;
	for(unsigned int i = 0; i < imageCount; i++) images[i]->visible = visible;
}

/*--------------------------------------
	gui::Text::SetCharWidth
--------------------------------------*/
void gui::Text::SetCharWidth(const int i)
{
	charWidth = i;
	SetPos(pos);
}

/*--------------------------------------
	gui::Text::SetString

Returns length of string displayed.

FIXME: use realloc
FIXME: expand parameter is temporary, make text boxes that can wrap, clip, etc.
--------------------------------------*/
size_t gui::Text::SetString(const char* str, size_t n, const char* delim, bool expand)
{
	if(str)
	{
		size_t tempLen;

		if(delim)
		{
			const char* term = strpbrk(str, delim);

			if(term)
				tempLen = term - str;
			else
				tempLen = strlen(str);
		}
		else
			tempLen = strlen(str);

		if(tempLen > n)
			tempLen = n;

		if(!string || tempLen != stringLen || (expand && stringLen > imageCount) ||
		strncmp(str, string, stringLen))
		{
			if(string)
				delete[] string;

			stringLen = tempLen;
			string = new char[stringLen + 1];
			strncpy(string, str, stringLen);
			string[stringLen] = 0;
		}
	}
	else
	{
		stringLen = 0;

		if(string)
		{
			delete[] string;
			string = 0;
		}
	}

	if(expand)
	{
		if(stringLen > imageCount)
			SetImageCount(stringLen);
	}

	// FIXME: set special frame value for unused images so renderer doesn't draw them
	for(unsigned int i = 0; i < imageCount && i < stringLen; i++) images[i]->frame = int(string[i]);
	for(unsigned int i = stringLen; i < imageCount; i++) images[i]->frame = 0;

	return com::Min(stringLen, (size_t)imageCount);
}

/*--------------------------------------
	gui::Text::SetTexture
--------------------------------------*/
void gui::Text::SetTexture(rnd::Texture* tex)
{
	//Set new texture for all images
	texture.Set(tex);
	for(unsigned int i = 0; i < imageCount; i++)
		images[i]->SetTexture(texture);
}

/*--------------------------------------
	gui::Text::SetImageCount

FIXME: use realloc

FIXME: Script should not have to worry about this stuff
--------------------------------------*/
void gui::Text::SetImageCount(const unsigned int count)
{
	if(imageCount == count) return;

	if(imageCount > count)
	{
		//Delete excessive images
		for(unsigned int i = count; i < imageCount; i++) DeleteImage(images[i]);
		imageCount = count;
	}
	else
	{
		//Allocate new image array
		iImage** newImages = new iImage*[count];
		for(unsigned int i = 0; i < imageCount; i++) newImages[i] = images[i];

		//Append new images
		for(unsigned int i = imageCount; i < count; i++)
		{
			newImages[i] = CreateImage(texture);
			newImages[i]->AddLock();
		}

		//Replace old array with new array
		if(images) delete[] images;
		images = newImages;

		imageCount = count;

		//Set variables for new images
		SetScale(scale); //Also sets pos
		SetSubPalette(subPalette);
		SetVisible(visible);

		//Set string so new images have correct frame
		SetString(string);
	}
}

/*--------------------------------------
	gui::CreateText
--------------------------------------*/
gui::iText* gui::CreateText(rnd::Texture* tex, int charPixelWidth)
{
	Text* txt = new Text;
	txt->pos = 0.0f;
	txt->scale = 1;
	txt->subPalette = 0;
	txt->visible = true;
	txt->charWidth = charPixelWidth;
	txt->string = 0;
	txt->stringLen = 0;
	txt->images = 0;
	txt->imageCount = 0;
	txt->SetTexture(tex);
	return txt;
}

/*--------------------------------------
	gui::DeleteText
--------------------------------------*/
void gui::DeleteText(iText*& text)
{
	Text* txt = (Text*)text;
	if(txt->string) delete[] txt->string;
	for(int i = 0; i < txt->imageCount; i++) DeleteImage(txt->images[i]);
	delete[] txt->images;
	delete txt;
}

/*
################################################################################################


	TEXT LUA


################################################################################################
*/

/*--------------------------------------
LUA	gui::CreateText (Text)

IN	texT, iCharPixelWidth
OUT	txt
--------------------------------------*/
int gui::CreateText(lua_State* l)
{
	rnd::Texture* t = rnd::Texture::LuaTo(1);
	int charPixelWidth = lua_tointeger(l, 2);
	iText* txt = CreateText(t, charPixelWidth);
	txt->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	gui::TxtDelete (__gc)

IN	txtT
--------------------------------------*/
int gui::TxtDelete(lua_State* l)
{
	iText* t = iText::UserdataLuaTo(1);

	if(!t)
		return 0;

	DeleteText(t);
	return 0;
}

/*--------------------------------------
LUA	gui::TxtPos (Pos)

IN	txtT
OUT	xPos, yPos, zPos
--------------------------------------*/
int gui::TxtPos(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	vec::LuaPushVec(l, t->Pos());
	return 3;
}

/*--------------------------------------
LUA	gui::TxtSetPos (SetPos)

IN	txtT, xPos, yPos, zPos
--------------------------------------*/
int gui::TxtSetPos(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetPos(vec::LuaToVec(l, 2, 3, 4));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtScale (Scale)

IN	txtT
OUT	iScale
--------------------------------------*/
int gui::TxtScale(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushinteger(l, t->Scale());
	return 1;
}


/*--------------------------------------
LUA	gui::TxtSetScale (SetScale)

IN	txtT, iScale
--------------------------------------*/
int gui::TxtSetScale(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetScale(lua_tointeger(l, 2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtSubPalette (SubPalette)

IN	txtT
OUT	iSubPalette
--------------------------------------*/
int gui::TxtSubPalette(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushinteger(l, t->SubPalette());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetSubPalette (SetSubPalette)

IN	txtT, iSubPalette
--------------------------------------*/
int gui::TxtSetSubPalette(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetSubPalette(lua_tointeger(l, 2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtVisible (Visible)

IN	txtT
OUT	bVisible
--------------------------------------*/
int gui::TxtVisible(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushboolean(l, t->Visible());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetVisible (SetVisible)

IN	txtT, bVisible
--------------------------------------*/
int gui::TxtSetVisible(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetVisible(lua_toboolean(l, 2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtCharWidth (CharWidth)

IN	txtT
OUT	iCharWidth
--------------------------------------*/
int gui::TxtCharWidth(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushinteger(l, t->CharWidth());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetCharWidth (SetCharWidth)

IN	txtT, iCharWidth
--------------------------------------*/
int gui::TxtSetCharWidth(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetCharWidth(lua_tointeger(l, 2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtString (String)

IN	txtT
OUT	sTxtString
--------------------------------------*/
int gui::TxtString(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushstring(l, t->String());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtStringLength (StringLength)

IN	txtT
OUT	iLength
--------------------------------------*/
int gui::TxtStringLength(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushinteger(l, t->StringLength());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetString (SetString)

IN	txtT, sTxtString
--------------------------------------*/
int gui::TxtSetString(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetString(lua_tostring(l, 2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtTexture (Texture)

IN	txtT
OUT	texSet
--------------------------------------*/
int gui::TxtTexture(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->Texture()->LuaPush();
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetTexture (SetTexture)

IN	txtT, texSet
--------------------------------------*/
int gui::TxtSetTexture(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetTexture(rnd::Texture::LuaTo(2));
	return 0;
}

/*--------------------------------------
LUA	gui::TxtImageCount (ImageCount)

IN	txtT
OUT	iImageCount

FIXME: change internal type to int (instead of uint)?
--------------------------------------*/
int gui::TxtImageCount(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	lua_pushinteger(l, t->ImageCount());
	return 1;
}

/*--------------------------------------
LUA	gui::TxtSetImageCount (SetImageCount)

IN	txtT, iImageCount
--------------------------------------*/
int gui::TxtSetImageCount(lua_State* l)
{
	iText* t = iText::CheckLuaTo(1);
	t->SetImageCount(lua_tointeger(l, 2));
	return 0;
}

/*
################################################################################################


	QUICK DRAW


################################################################################################
*/

/*--------------------------------------
	gui::Draw2DText
--------------------------------------*/
void gui::Draw2DText(const com::Vec2& pos, const char* str, int scale, int subPalette,
	float time)
{
	com::Vec3 user(pos.x, pos.y, 0.5f);
	AddQuickText(user, str, scale, subPalette, time);
}

/*--------------------------------------
	gui::Draw3DText
--------------------------------------*/
void gui::Draw3DText(const com::Vec3& pos, const char* str, int scale, int subPalette,
	float time)
{
	scn::Camera* cam = scn::ActiveCamera();

	if(!cam)
		return;

	com::Vec3 user = rnd::VecWorldToUser(wrp::VideoWidth(), wrp::VideoHeight(), *cam, pos, 0);
	AddQuickText(user, str, scale, subPalette, time);
}

/*--------------------------------------
	gui::AddQuickText
--------------------------------------*/
void gui::AddQuickText(const com::Vec3& user, const char* str, int scale, int subPalette,
	float time)
{
	const quick_text DEFAULT_QUICK_TEXT = {0};

	if(time >= 0.0f)
	{
		quickTexts.Ensure(numQuickTexts + 1, DEFAULT_QUICK_TEXT);
		quick_text& qt = quickTexts[numQuickTexts];

		if(!qt.txt)
			qt.txt = CreateText(quickTextFont, quickTextWidth);

		qt.txt->SetPos(user);
		qt.txt->SetScale(scale);
		qt.txt->SetSubPalette(subPalette);
		qt.txt->SetString(str);
		qt.txt->SetVisible(true);
		qt.time = ceil(time * 1000.0f);
		numQuickTexts++;
	}
}

/*--------------------------------------
	gui::QuickDrawInit
--------------------------------------*/
void gui::QuickDrawInit()
{
	quickTextFont.Set(rnd::EnsureTexture("font_small.tex"));
	quickTextWidth = 8;
}

/*--------------------------------------
	gui::QuickDrawAdvance
--------------------------------------*/
void gui::QuickDrawAdvance()
{
	static unsigned long long oldTime = 0;
	unsigned long long time = wrp::SimTime();
	unsigned delta = time - oldTime;
	oldTime = time;

	size_t readQT = 0, writeQT = 0;

	while(readQT < numQuickTexts)
	{
		if(quickTexts[readQT].time > delta)
		{
			quickTexts[writeQT].time = quickTexts[readQT].time - delta;
			quickTexts[writeQT].txt = quickTexts[readQT].txt;
			writeQT++;
			readQT++;
		}
		else
		{
			// This quick text is done
			// Swap [readQT] and [last] so txt ptr is preserved for reuse
			numQuickTexts--;
			quick_text temp = quickTexts[readQT];
			temp.txt->SetVisible(false);
			quickTexts[readQT] = quickTexts[numQuickTexts];
			quickTexts[numQuickTexts] = temp;
		}
	}
}

/*
################################################################################################


	QUICK DRAW LUA


################################################################################################
*/

/*--------------------------------------
LUA	gui::Draw2DText

IN	v2Pos, sStr, [iScale = 1, iSubPalette = 0, nTime = 0]
--------------------------------------*/
int gui::Draw2DText(lua_State* l)
{
	Draw2DText(vec::CheckLuaToVec(l, 1, 2), luaL_checkstring(l, 3), luaL_optinteger(l, 4, 1),
		luaL_optinteger(l, 5, 0), luaL_optnumber(l, 6, (lua_Number)0.0));

	return 0;
}

/*--------------------------------------
LUA	gui::Draw3DText

IN	v3Pos, sStr, [nScale = 1, iSubPalette = 0, nTime = 0]
--------------------------------------*/
int gui::Draw3DText(lua_State* l)
{
	Draw3DText(vec::CheckLuaToVec(l, 1, 2, 3), luaL_checkstring(l, 4), luaL_optinteger(l, 5, 1),
		luaL_optinteger(l, 6, 0), luaL_optnumber(l, 7, (lua_Number)0.0));

	return 0;
}

/*
################################################################################################


	GENERAL


################################################################################################
*/

/*--------------------------------------
	gui::Init
--------------------------------------*/
void gui::Init()
{
	// Lua
	luaL_Reg regs[] =
	{
		{"Image", CreateImage},
		{"Text", CreateText},
		{"Draw2DText", Draw2DText},
		{"Draw3DText", Draw3DText},
		{0, 0},
	};

	luaL_Reg imgRegs[] =
	{
		{"__gc", ImgDelete},
		{"Texture", ImgTexture},
		{"SetTexture", ImgSetTexture},
		{"Pos", ImgPos},
		{"SetPos", ImgSetPos},
		{"Scale", ImgScale},
		{"SetScale", ImgSetScale},
		{"Frame", ImgFrame},
		{"SetFrame", ImgSetFrame},
		{"SubPalette", ImgSubPalette},
		{"SetSubPalette", ImgSetSubPalette},
		{"Visible", ImgVisible},
		{"SetVisible", ImgSetVisible},
		{0, 0}
	};

	luaL_Reg txtRegs[] =
	{
		{"__gc", TxtDelete},
		{"Pos", TxtPos},
		{"SetPos", TxtSetPos},
		{"Scale", TxtScale},
		{"SetScale", TxtSetScale},
		{"SubPalette", TxtSubPalette},
		{"SetSubPalette", TxtSetSubPalette},
		{"Visible", TxtVisible},
		{"SetVisible", TxtSetVisible},
		{"CharWidth", TxtCharWidth},
		{"SetCharWidth", TxtSetCharWidth},
		{"String", TxtString},
		{"StringLength", TxtStringLength},
		{"SetString", TxtSetString},
		{"Texture", TxtTexture},
		{"SetTexture", TxtSetTexture},
		{"ImageCount", TxtImageCount},
		{"SetImageCount", TxtSetImageCount},
		{0, 0}
	};

	const luaL_Reg* metas[] = {
		imgRegs,
		txtRegs,
		0
	};

	const char* prefixes[] = {
		"Img",
		"Txt",
		0
	};

	scr::RegisterLibrary(scr::state, "ggui", regs, 0, 0, metas, prefixes);
	iImage::RegisterMetatable(imgRegs, 0);
	iText::RegisterMetatable(txtRegs, 0);
}