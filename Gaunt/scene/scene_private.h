// scene_private
// Martynas Ceicys

#ifndef SCENE_PRIVATE_H
#define SCENE_PRIVATE_H

#include "scene.h"

namespace scn
{

// FIXME: doing obj_links with macros is probably faster and easier to read

/*--------------------------------------
	scn::LinkObject
--------------------------------------*/
template <class This, class Other>
void LinkObject(This& t, com::Arr<obj_link<Other>> (This::*links), size_t (This::*numLinks),
	Other& o, com::Arr<obj_link<This>> (Other::*otherLinks), size_t (Other::*otherNum))
{
	(t.*links).Ensure(t.*numLinks + 1);
	(o.*otherLinks).Ensure(o.*otherNum + 1);

	(t.*links)[t.*numLinks].obj = &o;
	(t.*links)[t.*numLinks].reverseIndex = o.*otherNum;

	(o.*otherLinks)[o.*otherNum].obj = &t;
	(o.*otherLinks)[o.*otherNum].reverseIndex = t.*numLinks;

	++(t.*numLinks);
	++(o.*otherNum);
}

/*--------------------------------------
	scn::UnlinkObjects
--------------------------------------*/
template <class This, class Other>
void UnlinkObjects(This& t, com::Arr<obj_link<Other>> (This::*links), size_t (This::*numLinks),
	com::Arr<obj_link<This>> (Other::*otherLinks), size_t (Other::*otherNum))
{
	for(obj_link<Other>* it = (t.*links).o; it < (t.*links).o + t.*numLinks; it++)
	{
		Other& other = *it->obj;
		obj_link<This>& src = (other.*otherLinks)[--(other.*otherNum)];
		obj_link<This>& dest = (other.*otherLinks)[it->reverseIndex];
		dest = src;

		/* other's link to src.obj got moved to dest, update reverseIndex kept by src.obj's link
		to other to reflect this change */
		(src.obj->*links)[src.reverseIndex].reverseIndex = it->reverseIndex;
	}

	t.*numLinks = 0;
}

#define SCN_SETUP_TRANSCRIPT_KEY_BUFFER(bufName, bufSize, prefix, varDestName, varSizeName) \
	char bufName[bufSize]; \
	size_t varSizeName; \
	char* varDestName; \
	{ \
		size_t preLen = strlen(prefix); \
		varSizeName = preLen < bufSize ? bufSize - preLen : 0; \
		varDestName = bufName + preLen; \
	} \
	strncpy(bufName, prefix, bufSize)

}

#endif