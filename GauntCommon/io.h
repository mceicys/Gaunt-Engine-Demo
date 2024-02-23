// io.h
// Martynas Ceicys

#ifndef COM_IO_H
#define COM_IO_H

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "array.h"

namespace com
{

#define COM_FLT_PRNT_LEN 11 // 1 sign + 1 point + 9 digits
#define COM_FLT_PRNT "%.9g"
#define COM_DBL_PRNT_LEN 19
#define COM_DBL_PRNT "%.17g"

#ifdef _MSC_VER
	#define COM_IMAX_PRNT "%lld"
	#define COM_UMAX_PRNT "%llu"
#else
	#define COM_IMAX_PRNT "%jd"
	#define COM_UMAX_PRNT "%ju"
#endif

#ifdef _MSC_VER
	#define COM_FUNC_NAME __FUNCTION__
#else
	#define COM_FUNC_NAME "func"
#endif

#define COM_ERRORF(format, ...) \
	com::ErrorF("ERROR " __FILE__ " " COM_FUNC_NAME " %u\n" format, __LINE__, __VA_ARGS__)
#define COM_ERROR(str) com::ErrorF("ERROR " __FILE__ " " COM_FUNC_NAME " %u\n" str, __LINE__)
#define COM_SET_FLAG(flags, bit, on) ((on) ? (flags) |= (bit) : (flags) &= ~(bit))
#define COM_LITTLE_ENDIAN *(unsigned char*)&com::UONE

#if COM_ALLOW_OUTPUT
#define COM_OUTPUT(str) CustomPrintF(str)
#define COM_OUTPUT_F(str, ...) CustomPrintF(str, __VA_ARGS__)
#else
#define COM_OUTPUT(str)
#define COM_OUTPUT_F(str, ...)
#endif

extern int (*CustomPrintF)(const char* format, ...); // printf by default
extern int (*CustomVPrintF)(const char* format, va_list args); // vprintf by default
const unsigned UONE = 1u;

/*--------------------------------------
	com::BreakLE

bytesOut must contain at least sizeof(t) elements.
--------------------------------------*/
template <typename t> void BreakLE(const t& val, unsigned char* bytesOut)
{
	if(COM_LITTLE_ENDIAN)
	{
		for(size_t i = 0; i < sizeof(t); i++)
			bytesOut[i] = ((unsigned char*)&val)[i];
	}
	else
	{
		for(size_t i = 0; i < sizeof(t); i++)
			bytesOut[i] = ((unsigned char*)&val)[sizeof(t) - 1 - i];
	}
}

/*--------------------------------------
	com::BreakBE
--------------------------------------*/
template <typename t> void BreakBE(const t& val, unsigned char* bytesOut)
{
	if(COM_LITTLE_ENDIAN)
	{
		for(size_t i = 0; i < sizeof(t); i++)
			bytesOut[i] = ((unsigned char*)&val)[sizeof(t) - 1 - i];
	}
	else
	{
		for(size_t i = 0; i < sizeof(t); i++)
			bytesOut[i] = ((unsigned char*)&val)[i];
	}
}

/*--------------------------------------
	com::WriteLE
--------------------------------------*/
template <typename t> size_t WriteLE(const t* src, size_t count, FILE* file)
{
	size_t numWrite = 0;

	for(size_t i = 0; i < count; i++)
	{
		unsigned char bytes[sizeof(t)];
		BreakLE(src[i], bytes);

		if(fwrite(bytes, sizeof(unsigned char), sizeof(t), file))
			numWrite++;
		else
			break;
	}

	return numWrite;
}

/*--------------------------------------
	com::WriteBE
--------------------------------------*/
template <typename t> size_t WriteBE(const t* src, size_t count, FILE* file)
{
	size_t numWrite = 0;

	for(size_t i = 0; i < count; i++)
	{
		unsigned char bytes[sizeof(t)];
		BreakBE(src[i], bytes);

		if(fwrite(bytes, sizeof(unsigned char), sizeof(t), file))
			numWrite++;
		else
			break;
	}

	return numWrite;
}

/*--------------------------------------
	com::MergeLE

bytes must contain at least sizeof(t) or size elements. size must be <= sizeof(t). If
size < sizeof(t), the least significant bytes are merged and the rest are zero'd.
--------------------------------------*/
template <typename t> void MergeLE(const unsigned char* bytes, t& valOut)
{
	if(COM_LITTLE_ENDIAN)
	{
		for(size_t i = 0; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = bytes[i];
	}
	else
	{
		for(size_t i = 0; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = bytes[sizeof(t) - 1 - i];
	}
}

template <typename t> void MergeLE(const unsigned char* bytes, t* valOut, size_t count)
{
	for(size_t i = 0; i < count; i++, bytes += sizeof(t), valOut++)
		MergeLE(bytes, *valOut);
}

// FIXME: Overloading since can't get size default value to work
template <size_t size, typename t> void MergeLE(const unsigned char* bytes, t& valOut)
{
	if(COM_LITTLE_ENDIAN)
	{
		for(size_t i = 0; i < size; i++)
			((unsigned char*)&valOut)[i] = bytes[i];

		for(size_t i = size; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = 0;
	}
	else
	{
		for(size_t i = 0; i < sizeof(t) - size; i++)
			((unsigned char*)&valOut)[i] = 0;

		for(size_t i = sizeof(t) - size; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = bytes[sizeof(t) - 1 - i];
	}
}

/*--------------------------------------
	com::MergeBE
--------------------------------------*/
template <typename t> void MergeBE(const unsigned char* bytes, t& valOut)
{
	if(COM_LITTLE_ENDIAN)
	{
		for(size_t i = 0; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = bytes[sizeof(t) - 1 - i];
	}
	else
	{
		for(size_t i = 0; i < sizeof(t); i++)
			((unsigned char*)&valOut)[i] = bytes[i];
	}
}

template <typename t> void MergeBE(const unsigned char* bytes, t* valOut, size_t count)
{
	for(size_t i = 0; i < count; i++, bytes += sizeof(t), valOut++)
		MergeBE(bytes, *valOut);
}
	
/*--------------------------------------
	com::ReadLE

Returns number of bytes read. valOut is not set if returned value != sizeof(t).
--------------------------------------*/
template <typename t> size_t ReadLE(t& valOut, FILE* file)
{
	unsigned char bytes[sizeof(t)];
	size_t read = fread(bytes, 1, sizeof(t), file);

	if(read == sizeof(t))
		MergeLE(bytes, valOut);

	return read;
}

template <typename t> size_t ReadLE(t* valOut, size_t count, FILE* file)
{
	size_t read = 0;
	for(size_t i = 0; i < count; i++, valOut++)
		read += ReadLE(*valOut, file);
	return read;
}

/*--------------------------------------
	com::ReadBE
--------------------------------------*/
template <typename t> size_t ReadBE(t& valOut, FILE* file)
{
	unsigned char bytes[sizeof(t)];
	size_t read = fread(bytes, 1, sizeof(t), file);

	if(read == sizeof(t))
		MergeBE(bytes, valOut);

	return read;
}

template <typename t> size_t ReadBE(t* valOut, size_t count, FILE* file)
{
	size_t read = 0;
	for(size_t i = 0; i < count; i++, valOut++)
		read += ReadBE(*valOut, file);
	return read;
}

/*--------------------------------------
	com::IntLength

FIXME: unsigned type causes warning C4146: unary minus operator applied to unsigned type
--------------------------------------*/
template <typename T> size_t IntLength(T i)
{
	size_t n;
	
	if(i < 0)
	{
		n = 2;
		i = -i;
	}
	else
		n = 1;

	while(i > 9)
	{
		n++;
		i /= 10;
	}

	return n;
}

/*--------------------------------------
	com::CastNum

from must have an equal or larger range than to.
--------------------------------------*/
template <typename to, to TO_MIN, to TO_MAX, typename from> to CastNum(from num, int* err = 0)
{
	if(num < (from)TO_MIN)
	{
		if(err)
			*err = ERANGE;
		else
			errno = ERANGE;

		return TO_MIN;
	}

	if(num > (from)TO_MAX)
	{
		if(err)
			*err = ERANGE;
		else
			errno = ERANGE;

		return TO_MAX;
	}

	return (to)num;
}

void			LineF(const char* format, ...);
void			ErrorF(const char* format, ...);
void			PauseF(const char* format, ...);
void			PromptF(char* buffer, int bufSize, const char* promptFormat, ...);
char*			ReadLine(FILE* file, bool removeStartSpace = false, size_t* const lenOut = 0);
size_t			StrRSpn(const char* dest, const char* src, size_t len = -1);
const char*		StrRPBrk(const char* dest, const char* breakSet);
char*			StrRPBrk(char* dest, const char* breakSet);
const char*		StrChrUnescaped(const char* str, int ch, int es = '\\');
char*			StrChrUnescaped(char* str, int ch, int es = '\\');
int				StrNCmpLower(const char* lhs, const char* rhs, size_t count);
int				StrToI(const char* str, char** endPtr, int radix);
unsigned		StrToU(const char* str, char** endPtr, int radix);
float			StrToF(const char* str, const char** endPtr);
double			StrToDI(const char* str, char** endPtr); // FIXME: make all these endPtrs const, use const_cast when calling strtod
double			StrToInf(const char* str, char** endPtr);
double			StrToNAN(const char* str, char** endPtr);
const char*		StrStrLower(const char* str, const char* substr);
const char*		StrStrTokLower(const char* str, const char* substr, const char* delim);
size_t			StrTrimEnd(char* strIO, const char* chars);
size_t			StrRepChr(char* strIO, char orig, char set, size_t len = -1);
size_t			StrNPrepend(const char* prepend, char* dest, size_t maxLen);
size_t			ConvertPath(const char* path, const char* pre, const char* post,
				const char* ext, bool onlyFileName, char* out);
char*			NewConvertedPath(const char* path, const char* pre, const char* post,
				const char* ext, bool onlyFileName);
char*			NewStringCopy(const char* str);
int				VSNPrintF(char* dest, int size, bool* fitOut, const char* format, va_list args);
int				SNPrintF(char* dest, int size, bool* fitOut, const char* format, ...);
int				VArrPrintF(Arr<char>& dest, size_t* addedOut, const char* format, va_list args);
int				ArrPrintF(Arr<char>& dest, size_t* addedOut, const char* format, ...);
size_t			FReadAll(Arr<char>& dest, FILE* file);
const char*		BoolToStr(bool b);

}

#endif