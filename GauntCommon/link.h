// link.h
// Martynas Ceicys

// FIXME: Remove all code and make a single pooled list class

#ifndef COM_LINK_H
#define COM_LINK_H

namespace com
{

/*
################################################################################################


	LIST/STACK MACROS

To use, object must have two pointers to objects of the same type, prev and next. There should
also be a global pointer that will point to the last object (most recent) of the list.
Initialize the last object pointer to 0, or initialize the last object's prev and next to 0.

Use the two macros to link and unlink objects. The last object pointer is l. It is changed to
point to the newest object. The new object pointer is n.

To iterate through the list, begin from the last object and continue going to the previous
object until 0 is reached.

LINK_F and UNLINK_F can be used to also maintain a first object pointer. Initialize it to 0.

PUSH and POP can be used without next pointers.
################################################################################################
*/

#define COM_LINK(l, n) { \
	(n)->next = 0; \
	(n)->prev = (l); \
	(l) = (n); \
	if((n)->prev) \
		(n)->prev->next = (n); \
}

#define COM_UNLINK(l, n) { \
	if((n)->next) \
		(n)->next->prev = (n)->prev; \
	\
	if((n)->prev) \
		(n)->prev->next = (n)->next; \
	\
	if((l) == (n)) \
		(l) = (n)->prev; \
}

#define COM_LINK_F(f, l, n) { \
	if(!(f)) \
		(f) = (n); \
	\
	COM_LINK(l, n); \
}

#define COM_UNLINK_F(f, l, n) { \
	COM_UNLINK(l, n); \
	\
	if((f) == (n)) \
		(f) = (f)->next; \
}

#define COM_PUSH(l, n) {(n)->prev = (l); (l) = (n);}
#define COM_POP(l) (l) = (l)->prev

/*
################################################################################################
	LINKED LIST STRUCT
################################################################################################
*/

// FIXME: rename to lnk
template <typename obj> struct linker
{
	linker* prev;
	linker* next;

	obj*	o;
};

template <typename obj> struct list
{
	linker<obj> *f, *l;
};

/*--------------------------------------
	com::Link
--------------------------------------*/
template <typename obj> void Link(linker<obj>*& last, obj* object)
{
	linker<obj>* l = new linker<obj>; // FIXME: check if mallocs are hurting speed
	l->o = object;
	COM_LINK(last, l);
}

template <typename obj> void Link(linker<obj>*& first, linker<obj>*& last, obj* object)
{
	linker<obj>* l = new linker<obj>;
	l->o = object;
	COM_LINK_F(first, last, l);
}

template <typename obj> inline void Link(linker<obj>*& last, obj& object)
{
	Link(last, &object);
}

template <typename obj> inline void Link(linker<obj>*& first, linker<obj>*& last, obj& object)
{
	Link(first, last, &object);
}

template <typename obj> void Link(list<obj>& lst, obj* object)
{
	Link(lst.f, lst.l, object);
}

template <typename obj> void Link(list<obj>& lst, obj& object)
{
	Link(lst.f, lst.l, object);
}

/*--------------------------------------
	com::LinkBefore
--------------------------------------*/
template <typename obj> linker<obj>* LinkBefore(list<obj>& lst, linker<obj>* item, obj* object)
{
	if(!item)
		item = lst.f;

	if(!item)
	{
		Link(lst.f, lst.l, object);
		return lst.f;
	}
	
	linker<obj>* l = new linker<obj>;
	l->o = object;

	l->prev = item->prev;
	l->next = item;

	item->prev = l;

	if(l->prev)
		l->prev->next = l;

	if(lst.f == item)
		lst.f = l;
	
	return l;
}

/*--------------------------------------
	com::LinkAfter
--------------------------------------*/
template <typename obj> linker<obj>* LinkAfter(list<obj>& lst, linker<obj>* item, obj* object)
{
	if(!item)
		item = lst.l;

	if(!item)
	{
		Link(lst.f, lst.l, object);
		return lst.l;
	}

	linker<obj>* l = new linker<obj>;
	l->o = object;

	l->prev = item;
	l->next = item->next;

	item->next = l;

	if(l->next)
		l->next->prev = l;

	if(lst.l == item)
		lst.l = l;

	return l;
}

/*--------------------------------------
	com::Unlink
--------------------------------------*/
template <typename obj> void Unlink(linker<obj>*& last, linker<obj>* linked)
{
	COM_UNLINK(last, linked);
	delete linked;
}

template <typename obj> void Unlink(linker<obj>*& first, linker<obj>*& last,
	linker<obj>* linked)
{
	COM_UNLINK_F(first, last, linked);
	delete linked;
}

template <typename obj> void Unlink(list<obj>& lst, linker<obj>* linked)
{
	Unlink(lst.f, lst.l, linked);
}

/*--------------------------------------
	com::Find
--------------------------------------*/
template <typename obj> linker<obj>* Find(linker<obj>* last, obj* object)
{
	linker<obj>* it = last;

	for(; it != 0; it = it->prev)
	{
		if(it->o == object)
			break;
	}

	return it;
}

template <typename obj> linker<obj>* Find(list<obj>& lst, obj* object)
{
	return Find(lst.l, object);
}

/*--------------------------------------
	com::Count
--------------------------------------*/
template <typename obj> size_t Count(const linker<obj>* last)
{
	size_t count = 0;
	for(const linker<obj>* it = last; it != 0; it = it->prev, count++);
	return count;
}

/*--------------------------------------
	com::SwapLinks
--------------------------------------*/
template <typename obj> void SwapLinks(linker<obj>* a, linker<obj>* b)
{
	obj* temp = a->o;
	a->o = b->o;
	b->o = temp;
}

/*--------------------------------------
	com::DeleteListObjects
--------------------------------------*/
template <typename obj> void DeleteListObjects(list<obj>& lst)
{
	while(lst.f)
	{
		delete lst.f->o;
		com::Unlink(lst, lst.f);
	}
}

/*--------------------------------------
	com::Index
--------------------------------------*/
template <typename obj> size_t Index(const list<obj>& lst, const obj& object)
{
	size_t index = 0;
	for(const linker<obj>* it = lst.f; it; it = it->next, index++)
	{
		if(it->o == &object)
			return index;
	}
	return -1;
}

}

#endif