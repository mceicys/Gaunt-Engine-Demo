// json.h
// Martynas Ceicys

// FIXME: Encode/decode \uXXXX[\uXXXX] escape sequence (from/to UTF-8?)
/* FIXME: General stack traversal functionality, with const and non-const support
	Or figure out a way for JSVars to maintain a parent pointer while PairMaps and Arrs might
	reallocate (assignment function pointer?) */

#ifndef COM_JSON_H
#define COM_JSON_H

#include <stdarg.h>
#include <stdlib.h>

#include "array.h"
#include "fp.h"
#include "io.h"
#include "pair.h"

namespace com
{

class JSVar;

typedef Arr<JSVar> JSArr;
typedef PairMap<JSVar> JSObj;

// PARSE
bool	ParseJSON(const char* json, JSVar& rootOut, unsigned* lineOut = 0);

// WRITE
void	WriteJSON(const JSVar& root, FILE* file, const char* indent = "\t",
		bool postSpace = true);

// FREE
void	FreeJSON(JSVar& rootInOut);

// JSON STRING
size_t	JSONDecodedStringLength(const char* str, size_t strLen = -1, unsigned* lineIO = 0);
void	DecodeJSONString(const char* str, char* out, size_t strLen = -1);
size_t	WriteJSONEncodedString(const char* str, FILE* file);

#define COM_JSVAR_GETTER_VARIANT(name, type, func) \
int name(type& valOut) const \
{ \
	int err = 0; \
	valOut = func(&err); \
	return err; \
}

/*======================================
	com::JSVar

Must be Freed manually before deleting. Assignment operator does not make a copy of the value.
======================================*/
class JSVar
{
public:
	enum type
	{
		NONE,
		NUMBER,
		BOOLEAN,
		STRING,
		ARRAY,
		OBJECT
	};

	JSVar() : str(0), t(NONE), n(0) {}

	bool Free()
	{
		if(t == NONE)
			return false;
		else if(t == ARRAY)
		{
			for(size_t i = 0; i < n; i++)
				arr->o[i].Free();

			arr->Free();
			delete arr;
		}
		else if(t == OBJECT)
		{
			for(Pair<JSVar>* it = obj->First(); it; it = it->Next())
				it->Value().Free();

			delete obj;
		}
		else if(t == STRING)
			free(str);

		str = 0;
		t = NONE;
		n = 0;
		return true;
	}

	void Steal(JSVar& jsv)
	{
		t = jsv.t;
		n = jsv.n;

		switch(t)
		{
		case NUMBER:
			num = jsv.num;
			break;
		case BOOLEAN:
			bln = jsv.bln;
			break;
		case STRING:
			str = jsv.str;
			break;
		case OBJECT:
			obj = jsv.obj;
			break;
		case ARRAY:
			arr = jsv.arr;
			break;
		}

		jsv.str = 0;
		jsv.t = NONE;
		jsv.n = 0;
	}

	void SetNumber(double d)
	{
		Free();
		num = d;
		t = NUMBER;
	}

	void SetBool(bool b)
	{
		Free();
		bln = b;
		t = BOOLEAN;
	}

	void SetString(const char* s)
	{
		Free();
		str = (char*)malloc(strlen(s) + 1);
		strcpy(str, s);
		t = STRING;
	}

	void SetStringDecode(const char* s)
	{
		Free();
		str = (char*)malloc(JSONDecodedStringLength(s) + 1);
		DecodeJSONString(s, str);
		t = STRING;
	}

	void SetStringF(const char* format, ...)
	{
		Free();
		va_list args;
		va_start(args, format);

		size_t size = 32;
		str = (char*)malloc(size);

		size_t len;
		while(1)
		{
			bool fit;
			len = VSNPrintF(str, size, &fit, format, args);

			if(fit)
				break;

			size *= 2;
			str = (char*)realloc(str, size);
		}

		str = (char*)realloc(str, len + 1);
		t = STRING;
		va_end(args);
	}

	Arr<JSVar>* SetArray()
	{
		Free();
		arr = new Arr<JSVar>;
		t = ARRAY;
		return arr;
	}

	PairMap<JSVar>* SetObject()
	{
		Free();
		obj = new PairMap<JSVar>;
		t = OBJECT;
		return obj;
	}

	type Type() const {return t;}

	double Double(int* errOut = 0) const
	{
		if(t != NUMBER)
		{
			if(errOut)
				*errOut = 1;

			return 0.0;
		}

		return num;
	}

	COM_JSVAR_GETTER_VARIANT(ErrDouble, double, Double)

	// FIXME: errOut should indicate out-of-range cast in Float and FP
	float Float(int* errOut = 0) const
	{
		return (float)Double(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrFloat, float, Float)

	fp FP(int* errOut = 0) const
	{
		return (fp)Double(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrFP, fp, FP)

	template <typename n, n N_MIN, n N_MAX> n Number(int* errOut = 0) const
	{
		if(t != NUMBER)
		{
			if(errOut)
				*errOut = 1;

			return (n)0;
		}

		return CastNum<n, N_MIN, N_MAX>(num, errOut);
	}

	template <typename n, n N_MIN, n N_MAX> int ErrNumber(n& valOut) const
	{
		int err = 0;
		valOut = Number<n, N_MIN, N_MAX>(&err);
		return err;
	}

	int Int(int* errOut = 0) const
	{
		return Number<int, INT_MIN, INT_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrInt, int, Int)

	unsigned Unsigned(int* errOut = 0) const
	{
		return Number<unsigned, 0, UINT_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrUnsigned, unsigned, Unsigned)

	long Long(int* errOut = 0) const
	{
		return Number<long, LONG_MIN, LONG_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrLong, long, Long)

	unsigned long ULong(int* errOut = 0) const
	{
		return Number<unsigned long, 0, ULONG_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrULong, unsigned long, ULong)

	size_t SizeT(int* errOut = 0) const
	{
		return Number<size_t, 0, SIZE_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrSizeT, size_t, SizeT)

	int32_t Int32T(int* errOut = 0) const
	{
		return Number<int32_t, INT32_MIN, INT32_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrInt32T, int32_t, Int32T)

	uint32_t UInt32T(int* errOut = 0) const
	{
		return Number<uint32_t, 0, UINT32_MAX>(errOut);
	}

	COM_JSVAR_GETTER_VARIANT(ErrUInt32T, uint32_t, UInt32T)

	bool Bool(int* errOut = 0) const
	{
		if(t == BOOLEAN)
			return bln;
		else if(t == NUMBER)
			return num != 0.0;
		else
		{
			if(errOut)
				*errOut = 1;

			return false;
		}
	}

	COM_JSVAR_GETTER_VARIANT(ErrBool, bool, Bool)

	const char* String() const
	{
		if(t == STRING)
			return str;
		else if(t == BOOLEAN)
			return bln ? "true" : "false";
		else if(t == NONE)
			return "null";
		else
			return 0;
	}

	const Arr<JSVar>* Array() const
	{
		return t == ARRAY ? arr : 0;
	}

	Arr<JSVar>* Array()
	{
		return t == ARRAY ? arr : 0;
	}

	size_t NumElems() const
	{
		return n;
	}

	size_t SetNumElems(size_t num)
	{
		if(t != ARRAY)
			return 0;

		n = num;
		arr->Ensure(n);
		return n;
	}

	const PairMap<JSVar>* Object() const
	{
		return t == OBJECT ? obj : 0;
	}

	PairMap<JSVar>* Object()
	{
		return t == OBJECT ? obj : 0;
	}

	size_t WriteBasic(FILE* file, bool quoteString = false) const
	{
		int write = 0;

		if(t == NONE)
			write = fwrite("null", sizeof(char), 4, file);
		else if(t == NUMBER)
		{
			if(num > DBL_MAX)
				write = fprintf(file, "inf");
			else if(num < -DBL_MAX)
				write = fprintf(file, "-inf");
			else if(num != num)
				write = fprintf(file, "nan");
			else
				write = fprintf(file, COM_DBL_PRNT, num);
		}
		else if(t == BOOLEAN)
			write = fprintf(file, "%s", bln ? "true" : "false");
		else if(t == STRING)
		{
			if(quoteString)
			{
				write += fputc('"', file) != EOF;
				write += WriteJSONEncodedString(str, file);
				write += fputc('"', file) != EOF;
			}
			else
				write = WriteJSONEncodedString(str, file);
		}

		if(write > 0)
			return write;
		else
			return 0;
	}

	bool IsBasicArray() const
	{
		if(t != ARRAY)
			return false;

		for(size_t i = 0; i < NumElems(); i++)
		{
			if(arr->o[i].Type() >= ARRAY)
				return false;
		}

		return true;
	}

private:
	union
	{
		double			num;
		bool			bln;
		char*			str;
		PairMap<JSVar>*	obj;
		Arr<JSVar>*		arr;
	};

	type	t;
	size_t	n; // Number of array elements
};

}

#endif