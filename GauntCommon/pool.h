// pool.h
// Martynas Ceicys

#ifndef COM_POOL_H
#define COM_POOL_H

#include "array.h"

namespace com
{

/*======================================
	com::Pool

Consecutive memory linked list.

FIXME: Do O(n) initialization after each realloc so linking and active-check are faster
FIXME: Do placement new so objects don't have to reset themselves manually when activated
FIXME: Keep links in a separate array instead of wrapping PNode?
	Objects can get their own index so long as they guarantee they're part of the pool
	May be less cache friendly
======================================*/
template <class T> class Pool
{
public:
	template <class T> class PNode
	{
	public:
		T o;

		size_t Prev() const {return prev;}
		size_t Next() const {return next;}

	private:
		size_t prev, next;

		PNode() : prev(-1), next(-1) {}
		PNode(const PNode&);

		PNode& operator=(const PNode& p)
		{
			o = p.o;
			prev = p.prev;
			next = p.next;
			return *this;
		}

		friend class Pool<T>;
		friend class Arr<PNode<T>>;
		friend void Copy<PNode<T>>(PNode<T>*, const PNode<T>*, size_t);
		friend void Resize<PNode<T>>(PNode<T>*&, size_t, size_t);
	};

	Pool() : top(-1), fc(-1), lc(-1), n(0) {}
	Pool(size_t num) : top(0), fc(-1), lc(-1), n(0) {nodes.Init(num);}

	PNode<T>& operator[](size_t i) {return nodes[i];}
	const PNode<T>& operator[](size_t i) const {return nodes[i];}
	size_t Num() const {return n;}
	size_t First() const {return fc;}
	size_t Last() const {return lc;}

	void Init(size_t num)
	{
		nodes.Init(num);
		top = n = 0;
		fc = lc = -1;
	}

	void Free()
	{
		nodes.Free();
		top = -1;
		fc = lc = -1;
		n = 0;
	}

	size_t Ensure(size_t num)
	{
		size_t oldNum = nodes.n;
		size_t numAdded = nodes.Ensure(num);

		if(numAdded && top == -1)
			top = oldNum;

		return numAdded;
	}

	size_t Link()
	{
		size_t ret = top;

		if(ret == -1)
		{
			ret = n;
			Ensure(n + 1);
		}

		top = nodes[ret].next;

		if(fc == -1)
			fc = ret;

		nodes[ret].prev = lc;
		nodes[ret].next = -1;

		if(lc != -1)
			nodes[lc].next = ret;

		lc = ret;
		n++;
		return ret;
	}

	void Unlink(size_t i)
	{
		PNode<T>& del = nodes[i];

		if(!Active(i))
			return;

		if(del.prev == -1)
			fc = del.next;
		else
			nodes[del.prev].next = del.next;

		if(del.next == -1)
			lc = del.prev;
		else
			nodes[del.next].prev = del.prev;

		// Push on open node stack
		del.prev = i; // Indicates unlinked node
		del.next = top;
		top = i;
		n--;
	}

	bool Active(size_t i) const
	{
		return i < nodes.n && // Not invalid index
			nodes[i].prev != i && // Not unlinked node
			(nodes[i].prev != nodes[i].next || fc == i); // Not uninitialized node
	}

	size_t Index(const PNode<T>& node) const {return &node - nodes.o;}

private:
	Arr<PNode<T>> nodes;
	size_t top, fc, lc, n;
};

};

#endif