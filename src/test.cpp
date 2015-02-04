#include "memory_pool.h"

#include "catch.hpp"

inline bool is_aligned(const void * ptr, size_t align)
{
	return (reinterpret_cast<uintptr_t>(ptr) & (align - 1)) == 0;
}

TEST_CASE("Single new and delete", "[allocation]")
{
	MemoryPool<uint32_t> mp;
	uint32_t * p = mp.new_object(0xaabbccdd);
	REQUIRE(p != nullptr);
	CHECK(is_aligned(p, 4));
	CHECK(*p == 0xaabbccdd);
	mp.delete_object(p);
}

TEST_CASE("Double new and delete", "[allocation]")
{
	MemoryPool<uint32_t> mp;
	uint32_t * p1 = mp.new_object(0x11223344);
	REQUIRE(p1 != nullptr);
	CHECK(is_aligned(p1, 4));
	uint32_t * p2 = mp.new_object(0x55667788);
	REQUIRE(p2 != nullptr);
	CHECK(is_aligned(p2, 4));
	CHECK(*p1 == 0x11223344);
	mp.delete_object(p1);
	CHECK(*p2 == 0x55667788);
	mp.delete_object(p2);
}

TEST_CASE("Block fill and free", "[allocation]")
{
	MemoryPool<uint32_t> mp;
	std::vector<uint32_t *> v;
	for (size_t i = 0; i < 64; ++i)
	{
		uint32_t * p = mp.new_object(1 << i);
		REQUIRE(p != nullptr);
		CHECK(*p == 1 << i);
		v.push_back(p);
	}
	for (auto p : v)
	{
		mp.delete_object(p);
	}
}

TEST_CASE("Iterate full blocks", "[iteration]")
{
	MemoryPool<uint32_t> mp;
	std::vector<uint32_t *> v;
	for (size_t i = 0; i < 64; ++i)
	{
		uint32_t * p = mp.new_object(1 << i);
		REQUIRE(p != nullptr);
		CHECK(*p == 1 << i);
		v.push_back(p);
	}
	size_t index = 0;
	for (auto itr = mp.begin(), end = mp.end(); itr != end; ++itr)
	{
		CHECK(*itr == 1 << index);
		++index;
	}
	for (auto p : v)
	{
		mp.delete_object(p);
	}
}
