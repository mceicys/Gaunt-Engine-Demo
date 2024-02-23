// mark.h
// Martynas Ceicys

namespace com
{

#define COM_INC_MARK_CODE_OPEN \
	static unsigned master = 0; \
	master++; \
	if(!master) \
	{

#define COM_INC_MARK_CODE_CLOSE \
		master = 1; \
	} \
	return master

#define COM_INC_MARK_CODE_LIST(Owner, code, first) \
	COM_INC_MARK_CODE_OPEN \
	for(com::linker<Owner>* it = (first); it; it = it->next) \
		it->o->code = 0; \
	COM_INC_MARK_CODE_CLOSE

#define COM_INC_MARK_CODE_ARRAY(code, arr, num) \
	COM_INC_MARK_CODE_OPEN \
	for(size_t i = 0; i < (num); i++) \
		(arr)[i].code = 0; \
	COM_INC_MARK_CODE_CLOSE

#define COM_INC_MARK_CODE_POOL(code, pool) \
	COM_INC_MARK_CODE_OPEN \
	for(size_t i = (pool).First(); i != -1; i = (pool)[i].Next()) \
		(pool)[i].o.code = 0; \
	COM_INC_MARK_CODE_CLOSE

#define COM_INC_MARK_CODE_FUNC(Owner, code, Reset) \
	COM_INC_MARK_CODE_OPEN \
	Reset(&Owner::code); \
	COM_INC_MARK_CODE_CLOSE

#if 0
template <class Owner, unsigned (Owner::*mark), void (*Reset)(unsigned Owner::*m)>
class MasterCode
{
public:
	unsigned code;

	unsigned Inc()
	{
		code++;

		if(!code)
		{
			Reset(mark);
			code = 1;
		}

		return code;
	}

	operator unsigned() const {return code;}
	MasterCode<Owner, mark, Reset>& operator=(unsigned u) {code = u; return *this;}
};
#endif

}