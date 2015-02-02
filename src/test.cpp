#include "memory_pool.h"

#include <cassert>

namespace
{

void test1()
{
	MemoryPoolBase mp(sizeof(uint32_t));
	uint32_t * p = reinterpret_cast<uint32_t *>(mp.allocate());
	assert(p);
	*p = 0xaabbccdd;
	mp.deallocate(p);
}


void test2()
{
	MemoryPoolBase mp(sizeof(uint32_t));
	uint32_t * p1 = reinterpret_cast<uint32_t *>(mp.allocate());
	assert(p1);
	*p1 = 0x11223344;
	uint32_t * p2 = reinterpret_cast<uint32_t *>(mp.allocate());
	assert(p2);
	*p2 = 0x55667788;
	assert(*p1 == 0x11223344);
	mp.deallocate(p1);
	assert(*p2 == 0x55667788);
	mp.deallocate(p2);
}


void test3()
{
	MemoryPoolBase mp(sizeof(uint32_t));
	std::vector<uint32_t *> v;
	for (size_t i = 0; i < 64; ++i)
	{
		uint32_t * p = reinterpret_cast<uint32_t *>(mp.allocate());
		*p = 1 << i;
		v.push_back(p);
	}

	for (auto p : v)
	{
		mp.deallocate(p);
	}
}

} // anonymous namespace

int main()
{
	test1();
	test2();
	test3();
	return 0;
}

