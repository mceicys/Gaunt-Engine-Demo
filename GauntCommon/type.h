// type.h
// Martynas Ceicys

#ifndef COM_TYPE_H
#define COM_TYPE_H

namespace com
{

#define	COM_EQUAL_TYPES(a, b) com::type_comp<a, b>::equal

template <typename a, typename b>
struct type_comp
{
	static const bool equal = false;
};

template <typename a>
struct type_comp<a, a>
{
	static const bool equal = true;
};

/*--------------------------------------
	com::ConvertArray

If in_type and out_type are equivalent, returns a. Otherwise, returns a new array of type
out_type with num elements copied from a. The new array should be deallocated with delete[].
--------------------------------------*/
template <typename out_type, typename in_type> out_type* ConvertArray(in_type* a, size_t num)
{
	if(COM_EQUAL_TYPES(in_type, out_type))
		return (out_type*)a;
	else
	{
		out_type* b = new out_type[num];

		for(size_t i = 0; i < num; i++)
			b[i] = (out_type)a[i];

		return b;
	}
}

}

#endif