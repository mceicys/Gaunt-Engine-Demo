// glengine.cpp
// Martynas Ceicys

#include "glengine.h"
#include "../../GauntCommon/io.h"

bool HaveGLExtension(const char* ext)
{
	const char* cur = (const char*)glGetString(GL_EXTENSIONS);

	if(!cur)
		return false;

	while(*cur)
	{
		size_t len = strcspn(cur, " ");

		if(!strncmp(cur, ext, len))
			return true;

		cur += len;

		if(*cur == 0)
			break; // In case last extension is not delimited by space

		cur++;
	}

	return false;
}

bool HaveGLCore(unsigned majorReq, unsigned minorReq)
{
	const char* version = (const char*)glGetString(GL_VERSION);

	if(!version)
		return false;

	unsigned major = com::StrToU(version, 0, 10);
	unsigned minor = com::StrToU(version + strcspn(version, ".") + 1, 0, 10);
	return major > majorReq || (major == majorReq && minor >= minorReq);
}