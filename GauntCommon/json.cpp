// json.cpp
// Martynas Ceicys

#include <ctype.h>

#include "json.h"

namespace com
{
	// PARSE
	void		SkipWhitespace(const char*& cIO, unsigned& lineIO);
	void		NextChar(const char*& cIO, unsigned& lineIO);
	bool		FinishParseJSON(bool success, Arr<char>& bufIO, Arr<JSVar*>& stackIO,
				unsigned line, unsigned* lineOut);
	const char*	DecodeUntilQuote(const char* src, Arr<char>& bufIO, unsigned* lineIO);
	bool		ParseValue(const char*& cIO, unsigned& lineIO, Arr<char>& bufIO,
				Arr<JSVar*>& stackIO, size_t& numNodesIO, JSVar& valOut);
	bool		GoodComma(const char*& cIO, unsigned& lineIO);

	// WRITE
	struct write_node;

	bool		WriteValue(const JSVar& val, FILE* file, bool newLineContainer,
				Arr<write_node>& stackIO, size_t& numNodesIO);

	// FREE
	bool		FreeIfEmpty(JSVar& varIO, Arr<JSVar*>& stackIO, size_t& numNodesIO);
}

#define DEFAULT_JSVAR_STACK_SIZE 8

/*
################################################################################################


	PARSE


################################################################################################
*/

/*--------------------------------------
	com::ParseJSON

Returns true if all of json was parsed successfully. Does not FreeJSON on failure. "inf" and
"nan" values are valid, though that breaks from the standard.
--------------------------------------*/
#define FINISH_PARSE_JSON(success) FinishParseJSON(success, buf, stack, line, lineOut)

bool com::ParseJSON(const char* json, JSVar& rootOut, unsigned* lineOut)
{
	const char* c = json;
	unsigned line = 1;
	SkipWhitespace(c, line);

	if(*c != '{')
		return false;

	Arr<char> buf(32);
	Arr<JSVar*> stack(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;

	rootOut.SetObject();
	stack[numNodes] = &rootOut;
	numNodes++;

	NextChar(c, line);

	while(numNodes)
	{
		JSVar& var = *stack[numNodes - 1];
		
		if(var.Type() == JSVar::OBJECT)
		{
			if(*c == '}')
			{
				numNodes--;
				NextChar(c, line);
				if(!GoodComma(c, line))
					return FINISH_PARSE_JSON(false);
			}
			else if(*c == '"')
			{
				const char* endKey = DecodeUntilQuote(c + 1, buf, &line);
				Pair<JSVar>& pair = *var.Object()->Ensure(buf.o);

				c = endKey;
				NextChar(c, line);
				if(*c != ':')
					return FINISH_PARSE_JSON(false);
				NextChar(c, line);

				if(!ParseValue(c, line, buf, stack, numNodes, pair.Value()))
					return FINISH_PARSE_JSON(false);
			}
			else
				return FINISH_PARSE_JSON(false);
		}
		else if(var.Type() == JSVar::ARRAY)
		{
			if(*c == ']')
			{
				numNodes--;
				NextChar(c, line);
				if(!GoodComma(c, line))
					return FINISH_PARSE_JSON(false);
			}
			else
			{
				Arr<JSVar>& arr = *var.Array();
				arr.Ensure(var.NumElems() + 1);

				if(ParseValue(c, line, buf, stack, numNodes, arr[var.NumElems()]))
					var.SetNumElems(var.NumElems() + 1);
				else
					return FINISH_PARSE_JSON(false);
			}
		}
		else
			return FINISH_PARSE_JSON(false);
	}

	return FINISH_PARSE_JSON(true);
}

/*--------------------------------------
	com::SkipWhitespace
--------------------------------------*/
void com::SkipWhitespace(const char*& cIO, unsigned& lineIO)
{
	while(1)
	{
		cIO += strspn(cIO, "\t\f\r ");

		if(*cIO == '\n')
		{
			lineIO++;
			cIO++;
		}
		else
			break;
	}
}

/*--------------------------------------
	com::NextChar
--------------------------------------*/
void com::NextChar(const char*& cIO, unsigned& lineIO)
{
	if(*cIO == '\n')
		lineIO++;

	cIO++;
	SkipWhitespace(cIO, lineIO);
}

/*--------------------------------------
	com::FinishParseJSON
--------------------------------------*/
bool com::FinishParseJSON(bool success, Arr<char>& bufIO, Arr<JSVar*>& stackIO, unsigned line,
	unsigned* lineOut)
{
	bufIO.Free();
	stackIO.Free();

	if(lineOut)
		*lineOut = line;

	return success;
}

/*--------------------------------------
	com::DecodeUntilQuote

Returns end quote. If given, increments lineIO every real newline.
--------------------------------------*/
const char* com::DecodeUntilQuote(const char* src, Arr<char>& bufIO, unsigned* lineIO)
{
	const char* end = StrChrUnescaped(src, '"');
	size_t len = end - src;
	bufIO.Ensure(JSONDecodedStringLength(src, len, lineIO) + 1);
	DecodeJSONString(src, bufIO.o, len);
	return end;
}

/*--------------------------------------
	com::ParseValue

Returns true if value was parsed and appropriately has or lacks a comma. If successful, cIO is
set to the character after the value (and comma if it exists). May reallocate bufIO and stackIO.
--------------------------------------*/
bool com::ParseValue(const char*& cIO, unsigned& lineIO, Arr<char>& bufIO, Arr<JSVar*>& stackIO,
	size_t& numNodesIO, JSVar& valOut)
{
	bool maybeComma = false;

	const char* end;
	double num = com::StrToDI(cIO, (char**)&end);

	if(end != cIO)
	{
		valOut.SetNumber(num);
		cIO = end - 1;
		maybeComma = true;
	}
	else if(*cIO == '{')
	{
		valOut.SetObject();
		stackIO.Ensure(numNodesIO + 1);
		stackIO[numNodesIO] = &valOut;
		numNodesIO++;
	}
	else if(*cIO == '[')
	{
		valOut.SetArray();
		stackIO.Ensure(numNodesIO + 1);
		stackIO[numNodesIO] = &valOut;
		numNodesIO++;
	}
	else if(*cIO == '"')
	{
		end = DecodeUntilQuote(cIO + 1, bufIO, &lineIO);
		valOut.SetString(bufIO.o);
		cIO = end;
		maybeComma = true;
	}
	else if(!strncmp(cIO, "true", 4))
	{
		valOut.SetBool(true);
		cIO += 3;
		maybeComma = true;
	}
	else if(!strncmp(cIO, "false", 5))
	{
		valOut.SetBool(false);
		cIO += 4;
		maybeComma = true;
	}
	else if(!strncmp(cIO, "null", 4))
	{
		valOut.Free();
		cIO += 3;
		maybeComma = true;
	}
	else
		return false;

	NextChar(cIO, lineIO);

	if(maybeComma)
		return GoodComma(cIO, lineIO);

	return true;
}

/*--------------------------------------
	com::GoodComma

cIO should point to a possible comma. Returns true if the placement or lack of a comma follows
JSON standard. cIO is set to the next readable character after the comma.
--------------------------------------*/
bool com::GoodComma(const char*& cIO, unsigned& lineIO)
{
	if(*cIO == ',')
	{
		NextChar(cIO, lineIO);
		return *cIO != '}' && *cIO != ']';
	}
	else
		return *cIO == '}' || *cIO == ']' || *cIO == 0;
}

/*
################################################################################################


	WRITE


################################################################################################
*/

struct com::write_node
{
	const JSVar* parent;
	bool basicArray;

	// Next var to write
	union
	{
		const Pair<JSVar>* pair;
		size_t i;
	};
};

/*--------------------------------------
	com::WriteJSON

If indent is 0, the tree is written on one line. If postSpace is true, a space is written after
the colon following a key and after commas not followed by a new line.
--------------------------------------*/
void com::WriteJSON(const JSVar& root, FILE* file, const char* indent, bool postSpace)
{
	Arr<write_node> stack(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;

	WriteValue(root, file, indent, stack, numNodes);

	while(numNodes)
	{
		size_t curNode = numNodes - 1;
		JSVar::type type = stack[curNode].parent->Type();
		const JSVar& parent = *stack[curNode].parent;

		bool first;
		bool end;
		if(type == JSVar::OBJECT)
		{
			first = parent.Object()->First() == stack[curNode].pair;
			end = !stack[curNode].pair;
		}
		else
		{
			first = !stack[curNode].i;
			end = stack[curNode].i == parent.NumElems();
		}

		bool newLine = false;

		if(!first)
		{
			if(end)
			{
				if(indent && !stack[curNode].basicArray)
				{
					fputc('\n', file);
					newLine = true;
				}
			}
			else if(stack[curNode].basicArray)
			{
				if(postSpace)
					fprintf(file, "%s", ", ");
				else
					fputc(',', file);
			}
			else
			{
				if(indent)
				{
					fprintf(file, "%s", ",\n");
					newLine = true;
				}
				else
					fputc(',', file);
			}
		}

		if(indent && (newLine || (first && !stack[curNode].basicArray)))
		{
			for(size_t i = 0; i < curNode + !end; i++)
				fprintf(file, "%s", indent);
		}

		if(end)
		{
			fputc(type == JSVar::OBJECT ? '}' : ']', file);
			numNodes--;
		}
		else if(type == JSVar::OBJECT)
		{
			const Pair<JSVar>& pair = *stack[curNode].pair;
			stack[curNode].pair = stack[curNode].pair->Next();

			// Write key
			fputc('"', file);
			WriteJSONEncodedString(pair.Key(), file);
			fprintf(file, "%s", postSpace ? "\": " : "\":");

			WriteValue(pair.Value(), file, indent, stack, numNodes);
		}
		else if(type == JSVar::ARRAY)
		{
			size_t i = stack[curNode].i;
			stack[curNode].i++;
			WriteValue(parent.Array()->o[i], file, indent, stack, numNodes);
		}
		else
			break; // error
	}

	stack.Free();
	return;
}

/*--------------------------------------
	com::WriteValue

If val is not a container, writes it and returns true. Otherwise, pushes val onto the stack,
writes its starting bracket, and returns false. May reallocate stackIO if false is returned.
--------------------------------------*/
bool com::WriteValue(const JSVar& val, FILE* file, bool newLineContainer,
	Arr<write_node>& stackIO, size_t& numNodesIO)
{
	if(val.Type() == JSVar::OBJECT || val.Type() == JSVar::ARRAY)
	{
		stackIO.Ensure(numNodesIO + 1);
		stackIO[numNodesIO].parent = &val;

		if(val.Type() == JSVar::OBJECT)
		{
			stackIO[numNodesIO].pair = val.Object()->First();
			stackIO[numNodesIO].basicArray = false;
			fputc('{', file);
		}
		else
		{
			stackIO[numNodesIO].i = 0;
			stackIO[numNodesIO].basicArray = val.IsBasicArray();
			fputc('[', file);
		}

		if(newLineContainer && !stackIO[numNodesIO].basicArray)
			fputc('\n', file);

		numNodesIO++;
		return false;
	}

	val.WriteBasic(file, true);
	return true;
}

/*
################################################################################################


	FREE


################################################################################################
*/

/*--------------------------------------
	com::FreeJSON

Frees rootIO without recursion.
--------------------------------------*/
void com::FreeJSON(JSVar& rootIO)
{
	Arr<JSVar*> stack(DEFAULT_JSVAR_STACK_SIZE);
	size_t numNodes = 0;

	FreeIfEmpty(rootIO, stack, numNodes);

	while(numNodes)
	{
		JSVar& parent = *stack[numNodes - 1];

		if(parent.Type() == JSVar::OBJECT)
		{
			Pair<JSVar>* next = 0;
			for(Pair<JSVar>* it = parent.Object()->First(); it; it = next)
			{
				next = it->Next();
				JSVar& child = it->Value();

				if(FreeIfEmpty(child, stack, numNodes))
					parent.Object()->UnsetKey(*it);
				else
					break;
			}

			if(!parent.Object()->First())
				numNodes--;
		}
		else if(parent.Type() == JSVar::ARRAY)
		{
			while(parent.NumElems())
			{
				JSVar& child = parent.Array()->o[parent.NumElems() - 1];

				if(FreeIfEmpty(child, stack, numNodes))
					parent.SetNumElems(parent.NumElems() - 1);
				else
					break;
			}

			if(!parent.NumElems())
				numNodes--;
		}
		else
		{
			// Should never get here
			parent.Free();
			numNodes--;
		}
	}

	rootIO.Free();
	stack.Free();
}

/*--------------------------------------
	com::FreeIfEmpty

If varIO is not a container or is empty, frees it and returns true. Otherwise, puts it on the
stack and returns false.
--------------------------------------*/
bool com::FreeIfEmpty(JSVar& varIO, Arr<JSVar*>& stackIO, size_t& numNodesIO)
{
	if((varIO.Type() == JSVar::OBJECT && varIO.Object()->First()) ||
	(varIO.Type() == JSVar::ARRAY && varIO.NumElems()))
	{
		stackIO.Ensure(numNodesIO + 1);
		stackIO[numNodesIO] = &varIO;
		numNodesIO++;
		return false;
	}
	else
	{
		varIO.Free();
		return true;
	}
}

/*
################################################################################################


	JSON STRING


################################################################################################
*/

/*--------------------------------------
	com::JSONDecodedStringLength

Returns length of string once decoded. If given, increments lineIO every real newline.
--------------------------------------*/
size_t com::JSONDecodedStringLength(const char* str, size_t strLen, unsigned* lineIO)
{
	size_t size = 0;
	for(const char* c = str; *c && c - str < strLen; c++)
	{
		if(*c == '\\' && !++c)
			break;
		else if(lineIO && *c == '\n')
			(*lineIO)++;

		size++;
	}

	return size;
}

/*--------------------------------------
	com::DecodeJSONString

out must be able to contain JSONDecodedStringLength(str) + 1 chars.
--------------------------------------*/
void com::DecodeJSONString(const char* str, char* out, size_t strLen)
{
	static const char ESCAPE[] = "btnfr";
	static const char CONVERT[] = "\b\t\n\f\r";

	size_t i = 0;
	const char* c = str;
	while(*c && c - str < strLen)
	{
		if(*c == '\\')
		{
			if(!++c)
				break;

			if(const char* e = strchr(ESCAPE, *c))
				out[i++] = CONVERT[e - ESCAPE];
			else
				out[i++] = *c; // No translation, ignore backslash
		}
		else
			out[i++] = *c;

		c++;
	}

	out[i] = 0;
}

/*--------------------------------------
	com::WriteJSONEncodedString
--------------------------------------*/
size_t com::WriteJSONEncodedString(const char* str, FILE* file)
{
	static const char ENCODE[] = "\b\t\n\f\r\"/\\";
	static const char CONVERT[] = "btnfr\"/\\";
	size_t write = 0;

	while(1)
	{
		const char* c = strpbrk(str, ENCODE);
		write += fwrite(str, sizeof(char), c ? c - str : strlen(str), file);

		if(c)
		{
			const char* e = strchr(ENCODE, *c);
			char escape[2] = {'\\', CONVERT[e - ENCODE]};
			write += fwrite(escape, sizeof(char), 2, file);

			c++;
			str = c;
		}
		else
			break;
	}

	return write;
}