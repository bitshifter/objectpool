#include "memory_pool.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>

namespace {

	const size_t MIN_BLOCK_ALIGN = 64;

	inline void * malloc_block(size_t block_size, size_t alignment)
	{
#if defined( _WIN32 )
		return _aligned_malloc(block_size, alignment);
#else
		void * ptr;
		int result = posix_memalign(&ptr, alignment, block_size);
		return result == 0 ? ptr : nullptr;
#endif
	}

	inline void free_block(void * ptr)
	{
#if defined( _WIN32 )
		_aligned_free(ptr);
#else
		std::free(ptr);
#endif
	}

	/// Returns true if the pointer is of the given alignment
	inline bool is_aligned(const void * ptr, size_t align)
	{
		return (reinterpret_cast<uintptr_t>(ptr) & (align - 1)) == 0;
	}

	/// Returns true if the given value is a power of two
	inline bool is_power_of_2(uint32_t x)
	{
		return ((x != 0) && ((x & (~x + 1)) == x));
	}

	/// Returns the exponent of a power of 2
	inline uint32_t log2(uint32_t n)
	{
		#if _MSC_VER
			unsigned long i;
			_BitScanForward(&i, n);
			return i;
		#else
			return __builtin_ctz(n);
		#endif
	}
}

MemoryPoolBase::MemoryPoolBase(uint_t entry_size, uint_t max_entries) :
	used_indices_(max_entries, false),
	pool_mem_(reinterpret_cast<uint8_t*>(
				malloc_block(max_entries * entry_size, MIN_BLOCK_ALIGN))),
	entry_size_(entry_size),
	max_entries_(max_entries)
{
	// TODO: remove this constraint
	assert(is_power_of_2(entry_size_));
	// intialise the free list
	next_free_.reserve(max_entries);
	for (uint_t i = 0; i < max_entries; ++i)
	{
		next_free_.push_back(max_entries - 1 - i);
	}
}

MemoryPoolBase::~MemoryPoolBase()
{
	assert(next_free_.size() == max_entries_);
	free_block(pool_mem_);
}

MemoryPoolStats MemoryPoolBase::get_stats() const
{
	MemoryPoolStats stats;
	stats.block_count = 1;
	stats.allocation_count = max_entries_ - next_free_.size();
	return stats;
}

void * MemoryPoolBase::allocate()
{
	if (!next_free_.empty())
	{
		uint_t index = next_free_.back();
		next_free_.pop_back();
		assert(!used_indices_[index]);
		used_indices_[index] = true;
		return element_at(index);
	}
	else
	{
		return nullptr;
	}
}

void MemoryPoolBase::deallocate(const void * ptr)
{
	assert(ptr >= pool_mem_ && ptr < (pool_mem_ + (entry_size_ * max_entries_)));
	// remove index from used list
	uint_t used_index = index_of(reinterpret_cast<const uint8_t*>(ptr));
	assert(used_indices_[used_index]);
	used_indices_[used_index] = false;
	// store index of next free entry in this pointer
	next_free_.push_back(used_index);
}

inline uint8_t * MemoryPoolBase::element_at(uint_t index)
{
	return pool_mem_ + (index * entry_size_);
}

inline const uint8_t * MemoryPoolBase::element_at(uint_t index) const
{
	return const_cast<MemoryPoolBase *>(this)->element_at(index);
}

MemoryPoolBase::uint_t MemoryPoolBase::index_of(const uint8_t * ptr) const
{
	return (ptr - pool_mem_) >> (log2(entry_size_));
}

//
// Tests
//

#if UNIT_TESTS

#include "catch.hpp"

TEST_CASE("Single new and delete", "[allocation]")
{
	MemoryPool<uint32_t> mp(64);
	uint32_t * p = mp.new_object(0xaabbccdd);
	REQUIRE(p != nullptr);
	CHECK(is_aligned(p, 4));
	// should be aligned to the cache line size
	CHECK(is_aligned(p, MIN_BLOCK_ALIGN));
	CHECK(*p == 0xaabbccdd);
	mp.delete_object(p);
}

TEST_CASE("Double new and delete", "[allocation]")
{
	MemoryPool<uint32_t> mp(64);
	uint32_t * p1 = mp.new_object(0x11223344);
	REQUIRE(p1 != nullptr);
	CHECK(is_aligned(p1, 4));
	uint32_t * p2 = mp.new_object(0x55667788);
	REQUIRE(p2 != nullptr);
	CHECK(is_aligned(p2, 4));
	CHECK(p2 == p1 + 1);
	CHECK(*p1 == 0x11223344);
	mp.delete_object(p1);
	CHECK(*p2 == 0x55667788);
	mp.delete_object(p2);
}

TEST_CASE("Block fill and free", "[allocation]")
{
	MemoryPool<uint32_t> mp(64);
	std::vector<uint32_t *> v;
	for (size_t i = 0; i < 64; ++i)
	{
		uint32_t * p = mp.new_object(1 << i);
		REQUIRE(p != nullptr);
		CHECK(*p == 1u << i);
		v.push_back(p);
	}
	for (auto p : v)
	{
		mp.delete_object(p);
	}
}

TEST_CASE("Iterate full blocks", "[iteration]")
{
	MemoryPool<uint32_t> mp(64);
	std::vector<uint32_t *> v;
	size_t i;
	for (i = 0; i < 64; ++i)
	{
		uint32_t * p = mp.new_object(1 << i);
		REQUIRE(p != nullptr);
		CHECK(*p == 1u << i);
		v.push_back(p);
	}

	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 64u);
		CHECK(stats.block_count == 1u);
	}

	// check values
	i = 0;
	for (auto itr = v.begin(), end = v.end(); itr != end; ++itr)
	{
		CHECK(**itr == 1u << i);
		++i;
	}

	// delete every second entry
	for (i = 1; i < 64; i += 2)
	{
		uint32_t * p = v[i];
		v[i] = nullptr;
		mp.delete_object(p);
	}

	// check allocation count is reduced but block count is the same
	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 32u);
		CHECK(stats.block_count == 1u);
	}

	// check remaining objects
	i = 0;
	mp.for_each([&i](const uint32_t * itr)
			{
				CHECK(*itr == 1u << i);
				i += 2;
			});
	/*
	for (i = 0; i < 64; i += 2)
	{
		CHECK(*v[i] == 1u << i);
	}
	*/

	// allocate 16 new entries (fill first block)
    for (i = 1; i < 32; i += 2)
	{
		CHECK(v[i] == nullptr);
        v[i] = mp.new_object(1 << i);
	}

	// check allocation and block count
	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 48u);
		CHECK(stats.block_count == 1u);
	}

	// delete objects in second block
	for (i = 32; i < 64; ++i)
	{
		uint32_t * p = v[i];
		v[i] = nullptr;
		mp.delete_object(p);
	}

	// check that the empty block was freed
	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 32u);
		CHECK(stats.block_count == 1u);
	}

	for (auto p : v)
	{
		mp.delete_object(p);
	}

	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 0u);
		CHECK(stats.block_count == 1u);
	}
}

#endif // UNIT_TESTS

