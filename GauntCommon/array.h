// array.h
// Martynas Ceicys

#ifndef COM_ARRAY_H
#define COM_ARRAY_H

namespace com
{

#define COM_NEXT(i, n) (((i) + 1) % (n))
#define COM_PREV(i, n) ((i) ? (i) - 1 : (n) - 1)

#define COM_ENSURE_FACTOR 2

// Functions which may reallocate only work with arrays made with new[]

/*--------------------------------------
	com::Originals

redirectsInOut must be an array of num size_ts. Each redirect value is set to the index of the
first element in src that is equal to the current position's element in src. If all src elements
are unique, redirectsInOut will be a sequence from 0 to num - 1. Returns the number of original
elements.
--------------------------------------*/
template <typename obj> size_t Originals(const obj* src, size_t num, size_t* redirectsInOut)
{
	for(size_t i = 0; i < num; i++)
		redirectsInOut[i] = i;

	size_t numOriginal = 0;

	for(size_t i = 0; i < num; i++)
	{
		if(redirectsInOut[i] < i)
			continue;

		numOriginal++;

		for(size_t j = i + 1; j < num; j++)
		{
			if(src[i] == src[j])
				redirectsInOut[j] = i;
		}
	}

	return numOriginal;
}

/*--------------------------------------
	com::RemoveDuplicates

Rewrites objs and sets numInOut so each element is unique.

If redirectsInOut is given, it and numOriginal must be the output of Originals called with objs.
redirectsInOut is updated to reflect the new array.

If the equal function pointer is given, the function should return true if the two objects are
considered equal.
--------------------------------------*/
template <typename obj> void RemoveDuplicates(obj* objs, size_t& numInOut)
{
	// For each first instance, rewrite the remaining array so its value appears once
	for(size_t i = 0; i < numInOut - 1; i++)
	{
		size_t j = i + 1;
		for(size_t k = j; k < numInOut; k++)
		{
			if(objs[i] != objs[k])
				objs[j++] = objs[k];
		}

		numInOut = j;
	}
}

template <typename obj> void RemoveDuplicates(obj* objs, size_t num, size_t* redirectsInOut,
	size_t numOriginal)
{
	// For each first instance, grow new array (within the old array) and fix the redirects
	size_t wi = 0;

	for(size_t si = 0; si < num && wi < numOriginal; si++)
	{
		if(si == redirectsInOut[si])
		{
			objs[wi] = objs[si];

			for(size_t fi = si; fi < num; fi++)
			{
				if(si == redirectsInOut[fi])
					redirectsInOut[fi] = wi;
			}

			wi++;
		}
	}
}

template <typename obj> void RemoveDuplicates(obj* objs, size_t& numInOut,
	bool (*equal)(const obj&, const obj&))
{
	for(size_t i = 0; i < numInOut - 1; i++)
	{
		size_t j = i + 1;
		for(size_t k = j; k < numInOut; k++)
		{
			if(!equal(objs[i], objs[k]))
				objs[j++] = objs[k];
		}

		numInOut = j;
	}
}

/*--------------------------------------
	com::Swap
--------------------------------------*/
template <typename obj> void Swap(obj& a, obj& b)
{
	obj temp = a;
	a = b;
	b = temp;
}

/*--------------------------------------
	com::Reverse
--------------------------------------*/
template <typename obj> void Reverse(obj* objs, size_t num)
{
	for(size_t i = 0; i < num / 2; i++)
		Swap(objs[i], objs[num - i - 1]);
}

/*--------------------------------------
	com::Copy
--------------------------------------*/
template <typename obj> void Copy(obj* dest, const obj* src, size_t num)
{
	for(size_t i = 0; i < num; i++)
		dest[i] = src[i];
}

/*--------------------------------------
	com::Resize
--------------------------------------*/
template <typename obj> void Resize(obj*& o, size_t num, size_t newNum)
{
	obj* temp = o;
	o = new obj[newNum];
	Copy(o, temp, num <= newNum ? num : newNum);
	delete[] temp;
}

/*--------------------------------------
	com::Ensure
--------------------------------------*/
template <typename obj> size_t Ensure(obj*& arr, size_t& numAlloc, size_t numUsed,
	size_t numNeeded)
{
	if(numNeeded <= numAlloc)
		return 0;

	size_t oldNumAlloc = numAlloc;

	do
		numAlloc *= COM_ENSURE_FACTOR;
	while(numNeeded > numAlloc);

	com::Resize(arr, numUsed, numAlloc);

	return numAlloc - oldNumAlloc;
}

/*--------------------------------------
	com::InRange

If start is above end, assumes the range wraps.
--------------------------------------*/
inline bool InRange(size_t start, size_t end, size_t index)
{
	if(start <= end)
		return index >= start && index <= end;
	else
		return index >= start || index <= end;
}

/*--------------------------------------
	com::Find
--------------------------------------*/
template <typename obj> size_t Find(const obj* arr, size_t num, const obj& item)
{
	for(size_t i = 0; i < num; i++)
	{
		if(arr[i] == item)
			return i;
	}

	return (size_t)-1;
}

/*--------------------------------------
	com::ShiftCopy

Copies [beg, stop) to [beg + shift, stop + shift).
--------------------------------------*/
template <typename obj> void ShiftCopy(obj* arr, size_t beg, size_t stop, int shift)
{
	if(shift > 0)
	{
		for(size_t i = 0; i < stop - beg; i++)
		{
			size_t j = stop - 1 - i;
			arr[j + shift] = arr[j];
		}
	}
	else if(shift < 0)
	{
		for(size_t i = beg; i < stop; i++)
			arr[i + shift] = arr[i];
	}
}

/*======================================
	com::Arr

Array with some explicit convenience functions.

The Arr may be abandoned and the array deallocated later by passing it to ArrFree.

This works like any dynamic array; there are no safety checks. Assignment copies array address.
There is no destructor.

The current implementation calls new[] and the assignment operator of objects, which means the
array can be deallocated using delete[]. It's better not to, so future changes are easier.

FIXME: o and n should be private
FIXME: Allocator template parameter
	Default one does new, delete, and assignment
	One does realloc and free
	One does placement-new, delete, and copy
	One does placement-new, delete, and move
FIXME: Multiplication can wrap to 0; make it a single operation with a few more checks
======================================*/
template <class obj> class Arr
{
public:
	obj*	o;
	size_t	n;

	Arr() : o(0), n(0) {}
	Arr(size_t num) : o(0), n(0) {Init(num);}
	Arr(size_t num, const obj& set) : o(0), n(0) {Init(num); Set(set, n, 0);}

	obj& operator[](size_t i) {return o[i];}
	const obj& operator[](size_t i) const {return o[i];}

	void Init(size_t num)
	{
		if(o)
			ArrFree(o);

		n = num;
		o = new obj[n];
	}

	void Free()
	{
		if(o)
			ArrFree(o);

		n = 0;
	}

	void Copy(const obj* src, size_t num)
	{
		// FIXME: make an ensure function that doesn't preserve values, use that here
		if(!num)
		{
			Free();
			return;
		}

		if(!n)
		{
			n = num;
			o = new obj[n];
		}
		else if(n < num)
		{
			do
				n *= COM_ENSURE_FACTOR;
			while(n < num);

			delete[] o;
			o = new obj[n];
		}

		com::Copy(o, src, num);
	}

	void Resize(size_t num)
	{
		com::Resize(o, n, num);
		n = num;
	}

	// Makes sure at least num is allocated
	// Returns number of elements added if an allocation occurred, 0 otherwise
	size_t Ensure(size_t num)
	{
		if(num <= n)
			return 0;

		if(!o)
		{
			Init(num);
			return n;
		}

		size_t oldNum = n;

		do
			n *= COM_ENSURE_FACTOR;
		while(num > n);

		com::Resize(o, oldNum, n);
		return n - oldNum;
	}

	// Same as regular Ensure, but set is copied to the new elements
	size_t Ensure(size_t num, const obj& set)
	{
		size_t oldNum = n;
		size_t numAdded = Ensure(num);

		if(numAdded)
		{
			for(obj* dest = o + oldNum; dest < o + n; dest++)
				*dest = set;
		}

		return numAdded;
	}

	// Like Ensure but allocates exactly num if it's less than size
	size_t EnsureExact(size_t num)
	{
		if(num <= n)
			return 0;

		if(!o)
		{
			Init(num);
			return n;
		}

		size_t oldNum = n;
		n = num;
		com::Resize(o, oldNum, n);
		return n - oldNum;
	}

	// Ensures there are exactly num allocated elements
	int EnsureResize(size_t num)
	{
		if(num == n)
			return 0;

		int diff = (int)num - (int)n;

		if(!o)
			Init(num);
		else
			Resize(num);

		return diff;
	}

	int EnsureResize(size_t num, const obj& set)
	{
		size_t oldNum = n;
		int diff = EnsureResize(num);

		if(n > oldNum)
		{
			for(obj* dest = o + oldNum; dest < o + n; dest++)
				*dest = set;
		}

		return diff;
	}

	void Set(const obj& src, size_t num, size_t offset)
	{
		for(obj* dest = o + offset; dest < o + offset + num; dest++)
			*dest = src;
	}
};

/*--------------------------------------
	com::ArrFree
--------------------------------------*/
template <typename obj> void ArrFree(obj*& o)
{
	delete[] o;
	o = 0;
}

template <typename obj> void ArrFree(const obj*& o)
{
	delete[] o;
	o = 0;
}

}

#endif