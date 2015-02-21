#include "memory_pool.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>

namespace {

    const size_t MIN_BLOCK_ALIGN = 64;

    /// Aligns n to align. N will be unchanged if it is already aligned
    inline size_t align_to(size_t n, size_t align)
    {
	return (1 + (n - 1) / align) * align;
    }

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
    inline bool is_aligned_to(const void * ptr, size_t align)
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

MemoryPoolBase::MemoryPoolBase(size_t entry_size, index_t max_entries) :
    indices_(nullptr),
    free_head_index_(0),
    pool_mem_(nullptr),
    max_entries_(max_entries)
{
    const size_t index_size = sizeof(index_t);
    // align entries to the index alignment or index_t size at minimum
    const size_t entry_align = std::max(entry_size, index_size);
    // ensure indices include offset for entry alignment
    const size_t indices_size = align_to(index_size * max_entries, entry_align);
    // align block to cache line size, or entry alignment if larger
    const size_t block_align = std::max(entry_align, MIN_BLOCK_ALIGN);
    const size_t entries_size = entry_size * max_entries;
    // block size includes indices + entry alignment + entries
    const size_t block_size = indices_size + entries_size;
    uint8_t * bytes = reinterpret_cast<uint8_t*>(
	    malloc_block(block_size, block_align));
    indices_ = reinterpret_cast<index_t*>(bytes);
    // pool mem is offset from the start of the block
    pool_mem_ = bytes + indices_size;
    assert(is_aligned_to(indices_, block_align));
    assert(is_aligned_to(pool_mem_, entry_align));
    for (index_t i = 0; i < max_entries; ++i)
    {
	indices_[i] = i + 1;
    }
}

MemoryPoolBase::~MemoryPoolBase()
{
    assert(get_stats().allocation_count == 0);
    free_block(indices_);
}

MemoryPoolStats MemoryPoolBase::get_stats() const
{
    MemoryPoolStats stats;
    stats.block_count = 1;
    stats.allocation_count = 0;
    for (index_t i = 0; i < max_entries_; ++i)
    {
	stats.allocation_count += indices_[i] == i;
    }
    return stats;
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
    CHECK(is_aligned_to(p, 4));
    // should be aligned to the cache line size
    CHECK(is_aligned_to(p, MIN_BLOCK_ALIGN));
    CHECK(*p == 0xaabbccdd);
    mp.delete_object(p);
}

TEST_CASE("Double new and delete", "[allocation]")
{
    MemoryPool<uint32_t> mp(64);
    uint32_t * p1 = mp.new_object(0x11223344);
    REQUIRE(p1 != nullptr);
    CHECK(is_aligned_to(p1, 4));
    uint32_t * p2 = mp.new_object(0x55667788);
    REQUIRE(p2 != nullptr);
    CHECK(is_aligned_to(p2, 4));
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

