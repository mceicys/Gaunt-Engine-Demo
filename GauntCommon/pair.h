// pair.h
// Martynas Ceicys

#ifndef COM_PAIR_H
#define COM_PAIR_H

#include <string.h>

#include "link.h"

namespace com
{

template <class T> class PairMap;

/*======================================
	com::Pair
======================================*/
template <class T> class Pair
{
public:
	Pair() : key(0), prev(0), next(0) {}

	Pair(const char* k) : key(0), prev(0), next(0)
	{
		SetKey(k);
	}

	~Pair()
	{
		SetKey(0);
	}

	const char*		Key() const {return key;}
	T&				Value() {return value;}
	const T&		Value() const {return value;}
	Pair<T>*		Prev() {return prev;}
	const Pair<T>*	Prev() const {return prev;}
	Pair<T>*		Next() {return next;}
	const Pair<T>*	Next() const {return next;}

private:
	char*	key;
	T		value;
	Pair<T>	*prev, *next;

	void SetKey(const char* k)
	{
		if(key)
			delete[] key;

		if(k)
		{
			key = new char[strlen(k) + 1];
			strcpy(key, k);
		}
	}

	friend class PairMap<T>;
};

/*======================================
	com::PairMap
======================================*/
template <class T> class PairMap
{
public:
	PairMap() : first(0), last(0), n(0) {}

	PairMap(PairMap<T>&& pm) : first(pm.first), last(pm.last), n(pm.n)
	{
		pm.first = pm.last = 0;
		pm.n = 0;
	}

	~PairMap()
	{
		while(first)
		{
			Pair<T>* save = first;
			COM_UNLINK_F(first, last, first);
			delete save;
		}
	}

	const Pair<T>* Find(const char* key) const
	{
		Pair<T>* p = first;

		while(p && strcmp(p->Key(), key))
			p = p->next;

		return p;
	}

	const Pair<T>* Find(size_t index) const
	{
		Pair<T>* p = first;
		for(; index && p; index--, p = p->next);
		return p;
	}

	Pair<T>* Find(const char* key)
	{
		return const_cast<Pair<T>*>(const_cast<const PairMap<T>*>(this)->Find(key));
	}

	Pair<T>* Find(size_t index)
	{
		return const_cast<Pair<T>*>(const_cast<const PairMap<T>*>(this)->Find(index));
	}

	const T* Value(const char* key) const
	{
		const Pair<T>* p = Find(key);
		return p ? &p->Value() : 0;
	}

	const T* Value(size_t index) const
	{
		const Pair<T>* p = Find(index);
		return p ? &p->Value() : 0;
	}

	T* Value(const char* key)
	{
		return const_cast<T*>(const_cast<const PairMap<T>*>(this)->Value(key));
	}

	T* Value(size_t index)
	{
		return const_cast<T*>(const_cast<const PairMap<T>*>(this)->Value(index));
	}

	// Returned pair's value is unknown; do not read until setting it
	Pair<T>* Ensure(const char* key)
	{
		if(Pair<T>* found = Find(key))
			return found;

		Pair<T>* p = new Pair<T>(key);
		COM_LINK_F(first, last, p);
		n++;
		return p;
	}

	// Nullifies the pair's key, making it inaccessible
	// When and whether pair and its value are deleted is undefined; do not access after calling
	void UnsetKey(Pair<T>& pair)
	{
		COM_UNLINK_F(first, last, &pair);
		delete &pair;
		n--;
	}

	bool UnsetKey(const char* key)
	{
		if(Pair<T>* p = Find(key))
		{
			UnsetKey(*p);
			return true;
		}
		else
			return false;
	}

	// List is in insertion order
	Pair<T>* First() {return first;}
	const Pair<T>* First() const {return first;}
	Pair<T>* Last() {return last;}
	const Pair<T>* Last() const {return last;}
	size_t Num() const {return n;}

private:
	Pair<T>	*first, *last;
	size_t n;

	PairMap(const PairMap<T>&);
	PairMap<T>& operator=(const PairMap<T>&);
};

}

#endif