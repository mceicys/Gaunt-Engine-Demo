// io.cpp
// Martynas Ceicys

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "io.h"
#include "array.h"

namespace com
{
	int (*CustomPrintF)(const char* format, ...) = printf;
	int (*CustomVPrintF)(const char* format, va_list args) = vprintf;
}

/*--------------------------------------
	com::LineF
--------------------------------------*/
void com::LineF(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	CustomVPrintF(format, arg);
	CustomPrintF("\n");
	va_end(arg);
}

/*--------------------------------------
	com::ErrorF
--------------------------------------*/
void com::ErrorF(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	CustomVPrintF(format, arg);
	va_end(arg);
	exit(1);
}

/*--------------------------------------
	com::PauseF
--------------------------------------*/
void com::PauseF(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	CustomVPrintF(format, arg);
	va_end(arg);
	getchar();
}

/*--------------------------------------
	com::PromptF
--------------------------------------*/
void com::PromptF(char* buffer, int bufSize, const char* promptFormat, ...)
{
	va_list arg;
	va_start(arg, promptFormat);
	CustomVPrintF(promptFormat, arg);
	va_end(arg);
	fgets(buffer, bufSize, stdin);

	// Remove newline char
	size_t size = strlen(buffer);
	buffer[size - 1] = 0;
}

/*--------------------------------------
	com::ReadLine

Reads the next line of file into a buffer, and doubles the size of the buffer if necessary.
Trailing newlines are removed.

Returns a ptr to the static buffer. Calling this again will overwrite it. Returns 0 and does not
set lenOut if EOF before reading or an error occurred.
--------------------------------------*/
char* com::ReadLine(FILE* file, bool removeStartSpace, size_t* const lenOut)
{
	static size_t size = 128;
	static char* buf = (char*)malloc(size);

	if(!fgets(buf, size, file))
		return 0;

	size_t len = strlen(buf);
	while(buf[len - 1] != '\n' && !feof(file))
	{
		buf = (char*)realloc(buf, size * 2);
		fgets(buf + len, size + 1, file);
		size *= 2;
		len = strlen(buf);
	}

	if(buf[len - 1] == '\n')
		buf[len - 1] = 0;

	size_t add = 0;
	if(removeStartSpace)
		for(; add < len && (buf[add] == ' ' || buf[add] == '\t'); add++);

	if(lenOut)
		*lenOut = len - add;

	return buf + add;
}

/*--------------------------------------
	com::StrRSpn
--------------------------------------*/
size_t com::StrRSpn(const char* dest, const char* src, size_t len)
{
	if(!len)
		return 0;

	size_t fullLen = strlen(dest);
	size_t start = fullLen <= len ? fullLen : len + 1;
	size_t i = start;

	while(i)
	{
		i--;

		if(!strchr(src, dest[i]))
		{
			i++;
			break;
		}
	}

	return start - i;
}

/*--------------------------------------
	com::StrRPBrk
--------------------------------------*/
const char* com::StrRPBrk(const char* dest, const char* breakSet)
{
	const char* br = breakSet;
	const char* last = 0;

	while(*br != '\0')
	{
		const char* ch = strrchr(dest, *br);

		if(ch && (!last || ch > last))
			last = ch;

		br++;
	}

	return last;
}

char* com::StrRPBrk(char* dest, const char* breakSet)
{
	return (char*)StrRPBrk((const char*)dest, breakSet);
}

/*--------------------------------------
	com::StrChrUnescaped

Like strchr but ignores ch if it immediately follows es. Returns 0 if ch == es since the escape
character cannot be unescaped.
--------------------------------------*/
const char* com::StrChrUnescaped(const char* str, int ch, int es)
{
	if(ch == es)
		return 0;

	bool escape = false;
	const char* end = str;

	while(1)
	{
		if(*end == ch && !escape)
			return end;

		if(*end == es)
			escape = !escape;
		else
			escape = false;

		if(*end)
			end++;
		else
			break;
	}

	return 0;
}

char* com::StrChrUnescaped(char* str, int ch, int es)
{
	return (char*)StrChrUnescaped((const char*)str, ch, es);
}

/*--------------------------------------
	com::StrNCmpLower
--------------------------------------*/
int com::StrNCmpLower(const char* lhs, const char* rhs, size_t count)
{
	for(size_t i = 0; i < count; i++)
	{
		if(int diff = tolower((unsigned char)lhs[i]) - tolower((unsigned char)rhs[i]))
			return diff;

		if(!lhs[i] && !rhs[i])
			return 0;
	}

	return 0;
}

/*--------------------------------------
	com::StrToI
--------------------------------------*/
int com::StrToI(const char* str, char** endPtr, int radix)
{
#if INT_MAX == LONG_MAX && INT_MIN == LONG_MIN
	return strtol(str, endPtr, radix);
#else
	long l = strtol(str, endPtr, radix);

	if(l > INT_MAX)
	{
		errno = ERANGE;
		return INT_MAX;
	}
	else if(l < INT_MIN)
	{
		errno = ERANGE;
		return INT_MIN;
	}
	
	return l;
#endif
}

/*--------------------------------------
	com::StrToU
--------------------------------------*/
unsigned com::StrToU(const char* str, char** endPtr, int radix)
{
#if UINT_MAX == ULONG_MAX
	return strtoul(str, endPtr, radix);
#else
	unsigned long l = strtoul(str, endPtr, radix);

	if(l > UINT_MAX)
	{
		errno = ERANGE;
		return UINT_MAX;
	}
	
	return l;
#endif
}

/*--------------------------------------
	com::StrToF
--------------------------------------*/
float com::StrToF(const char* str, const char** endPtr)
{
	double d = strtod(str, const_cast<char**>(endPtr));

	if(d > FLT_MAX || d < -FLT_MAX)
	{
		errno = ERANGE;
		return (float)HUGE_VAL;
	}
	
	return (float)d;
}

/*--------------------------------------
	com::StrToDI

Calls strtod, then StrToInf, then StrToNAN until something succeeds or everything fails.
--------------------------------------*/
double com::StrToDI(const char* str, char** endPtr)
{
	char* end;

	double d = strtod(str, &end);
	if(str != end)
	{
		if(endPtr)
			*endPtr = end;

		return d;
	}

	d = StrToInf(str, &end);
	if(str != end)
	{
		if(endPtr)
			*endPtr = end;

		return d;
	}

	return StrToNAN(str, endPtr);
}

/*--------------------------------------
	com::StrToInf

[+|-]inf[inity]
Case-insensitive.

Returns 0.0 if not infinity.
--------------------------------------*/
double com::StrToInf(const char* str, char** endPtr)
{
	if(endPtr)
		*endPtr = (char*)str;

	const char* c = str;
	while(isspace(*c))
		c++;

	if(!*c)
		return 0.0;

	bool neg = false;
	if(*c == '-')
	{
		neg = true;
		c++;
	}
	else if(*c == '+')
		c++;

	if(StrNCmpLower(c, "inf", 3))
		return 0.0;

	c += 3;

	if(!StrNCmpLower(c, "inity", 5))
		c += 5;

	if(endPtr)
		*endPtr = (char*)c;

	return neg ? -HUGE_VAL : HUGE_VAL;
}

/*--------------------------------------
	com::StrToNAN

[+|-]nan[(char-sequence)]
Case-insensitive.

NaN is generated by a zero over zero division. The char-sequence is read but ignored. Returns
0.0 if not NaN.
--------------------------------------*/
double com::StrToNAN(const char* str, char** endPtr)
{
	if(endPtr)
		*endPtr = (char*)str;

	const char* c = str;
	while(isspace(*c))
		c++;

	if(!*c)
		return 0.0;

	if(*c == '+' || *c == '-')
		c++;

	if(StrNCmpLower(c, "nan", 3))
		return 0.0;

	c += 3;

	if(*c == '(')
	{
		// Digits, Latin letters, and underscores only
		while(1)
		{
			c++;
			if(*c == ')')
				break;
			else if((*c < '0' || *c > '9') && (*c < 'A' || *c > 'Z') && *c != '_' &&
			(*c < 'a' || *c > 'z'))
				return 0.0;
		}

		c++;
	}

	if(endPtr)
		*endPtr = (char*)c;

	double zero = 0.0;
	return zero / zero;
}

/*--------------------------------------
	com::StrStrLower
--------------------------------------*/
const char* com::StrStrLower(const char* str, const char* substr)
{
	size_t len = strlen(substr);

	for(const char* c = str; *c; c++)
	{
		if(!StrNCmpLower(c, substr, len))
			return c;
	}

	return 0;
}

/*--------------------------------------
	com::StrStrTokLower

Returns substr in str surrounded by a character in delim and/or the edges of str.
--------------------------------------*/
const char* com::StrStrTokLower(const char* str, const char* substr, const char* delim)
{
	size_t len = strlen(substr);

	for(const char* cur = StrStrLower(str, substr); cur;
	cur = *cur ? StrStrLower(cur + 1, substr) : 0)
	{
		if(cur != str && !strchr(delim, *(cur - 1)))
			continue;

		char end = *(cur + len);
		if(end != 0 && !strchr(delim, end))
			continue;

		return cur;
	}

	return 0;
}

/*--------------------------------------
	com::StrTrimEnd

Puts an earlier null terminator in strIO and returns number of characters trimmed.
--------------------------------------*/
size_t com::StrTrimEnd(char* strIO, const char* chars)
{
	size_t num = StrRSpn(strIO, chars);
	strIO[strlen(strIO) - num] = 0;
	return num;
}

/*--------------------------------------
	com::StrRepChr
--------------------------------------*/
size_t com::StrRepChr(char* strIO, char orig, char set, size_t len)
{
	size_t replaced = 0;

	for(size_t i = 0; i < len && *strIO; i++, strIO++)
	{
		if(*strIO == orig)
		{
			*strIO = set;
			replaced++;
		}
	}

	return replaced;
}

/*--------------------------------------
	com::StrNPrepend

Modifies dest so it begins with prepend. If strlen(prepend) + strlen(dest) > maxLen, dest's
original contents are truncated to make room for prepend. maxLen should be < dest's buffer size
so there's always room for a null terminator. Returns resulting strlen(dest).
--------------------------------------*/
size_t com::StrNPrepend(const char* prepend, char* dest, size_t maxLen)
{
	size_t preLen = strlen(prepend);
	size_t destLen = strlen(dest);

	if(maxLen > preLen)
	{
		// Shift
		size_t rem = maxLen - destLen;
		size_t finalLen, trunc;

		if(preLen > rem)
		{
			finalLen = maxLen;
			trunc = preLen - rem;
		}
		else
		{
			finalLen = preLen + destLen;
			trunc = 0;
		}

		if(destLen)
		{
			for(char *r = dest + destLen - trunc - 1, *w = dest + finalLen - 1; ; r--, w--)
			{
				*w = *r;

				if(r == dest)
					break;
			}
		}

		dest[finalLen] = 0;

		// Copy prepend
		memcpy(dest, prepend, preLen);
		return finalLen;
	}

	// Replace dest w/ prepend, possibly truncated
	memcpy(dest, prepend, maxLen);
	dest[maxLen] = 0;
	return maxLen;
}

/*--------------------------------------
	com::ConvertPath

Copies a modified version of path to out if out is not 0. Returns the length of the modified
path. out cannot overlap with path.

If pre is not 0, it is prepended to the filename.

If post is not 0, it is appended to the filename before the extension.

If ext is not 0, it replaces all characters starting from the last dot in the filename,
including the dot, or appends if path has no extension. If ext is 0, path's extension
is not modified.

If onlyFileName is true, no parent directories are included in the copy.
--------------------------------------*/
size_t com::ConvertPath(const char* path, const char* pre, const char* post, const char* ext,
	bool onlyFileName, char* out)
{
	const char* lastSlash = StrRPBrk(path, "/\\");
	const char* name = lastSlash ? lastSlash + 1 : path;
	const char* endName = strrchr(path, '.');

	if(!endName || (lastSlash && endName <= lastSlash))
		endName = path + strlen(path);

	size_t ancestorLen = lastSlash && !onlyFileName ? lastSlash - path + 1 : 0;
	size_t preLen = pre ? strlen(pre) : 0;
	size_t nameLen = endName - name;
	size_t postLen = post ? strlen(post) : 0;

	if(!ext)
		ext = endName;

	size_t extLen = strlen(ext);
	size_t len = ancestorLen + preLen + nameLen + postLen + extLen;

	if(out)
	{
		strncpy(out, path, ancestorLen);
		out += ancestorLen;

		if(pre)
		{
			strcpy(out, pre);
			out += preLen;
		}

		strncpy(out, name, nameLen);
		out += nameLen;

		if(post)
		{
			strcpy(out, post);
			out += postLen;
		}

		strcpy(out, ext);
	}

	return len;
}

/*--------------------------------------
	com::NewConvertedPath
--------------------------------------*/
char* com::NewConvertedPath(const char* path, const char* pre, const char* post,
	const char* ext, bool onlyFileName)
{
	size_t len = ConvertPath(path, pre, post, ext, onlyFileName, 0);
	char* str = new char[len + 1];
	ConvertPath(path, pre, post, ext, onlyFileName, str);
	return str;
}

/*--------------------------------------
	com::NewStringCopy

Returns a new[] string that is a copy of str. If str is 0, returns 0.
--------------------------------------*/
char* com::NewStringCopy(const char* str)
{
	if(!str)
		return 0;

	char* s = new char[strlen(str) + 1];
	strcpy(s, str);
	return s;
}

/*--------------------------------------
	com::VSNPrintF

Prints at most size - 1 chars to dest followed by a null terminator. Returns the number of
characters written, not including the null terminator, and if fitOut is not 0, sets it to
indicate whether the output fit into the buffer. If size is 1, makes the single character a null
terminator and returns 0. If size is 0, returns 0. Both of these cases assume that something
would have been written given the space and set fitOut to false.

Visual C++ 10.0 vsnprintf doesn't follow the standard, so this function has to be implemented
for each variation to make behavior predictable.
--------------------------------------*/
int com::VSNPrintF(char* dest, int size, bool* fitOut, const char* format, va_list args)
{
#ifdef _MSC_VER
	if(size <= 1)
	{
		if(size == 1)
			dest[0] = 0;

		if(fitOut)
			*fitOut = false;

		return 0;
	}

	int result = _vsnprintf(dest, size - 1, format, args);

	if(result >= 0)
	{
		dest[result] = 0;

		if(fitOut)
			*fitOut = true;

		return result;
	}
	else
	{
		dest[size - 1] = 0;
		
		if(fitOut)
			*fitOut = false;

		return size - 1;
	}
#else
#error VSNPrintF not implemented
#endif
}

/*--------------------------------------
	com::SNPrintF
--------------------------------------*/
int com::SNPrintF(char* dest, int size, bool* fitOut, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = VSNPrintF(dest, size, fitOut, format, args);
	va_end(args);

	return result;
}

/*--------------------------------------
	com::VArrPrintF
--------------------------------------*/
int com::VArrPrintF(Arr<char>& dest, size_t* addedOut, const char* format, va_list args)
{
	size_t added = 0;
	added += dest.Ensure(16);

	bool fit;
	int printed = 0;
	while(1)
	{
		printed = VSNPrintF(dest.o, dest.n, &fit, format, args);

		if(fit)
			break;
		else
			added += dest.Ensure(dest.n * 2);
	}

	if(addedOut)
		*addedOut = added;

	return printed;
}

/*--------------------------------------
	com::ArrPrintF
--------------------------------------*/
int com::ArrPrintF(Arr<char>& dest, size_t* addedOut, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int printed = com::VArrPrintF(dest, addedOut, format, args);
	va_end(args);
	return printed;
}

/*--------------------------------------
	com::FReadAll
--------------------------------------*/
size_t com::FReadAll(Arr<char>& dest, FILE* file)
{
	size_t readTotal = 0;
	size_t read = 0;

	do
	{
		dest.Ensure(readTotal + 2);
		read = fread(dest.o + readTotal, sizeof(char), dest.n - readTotal - 1, file);
		readTotal += read;
	}
	while(read);

	dest[readTotal] = 0;
	return readTotal;
}

/*--------------------------------------
	com::BoolToStr
--------------------------------------*/
const char* com::BoolToStr(bool b)
{
	return b ? "true" : "false";
}