#include "memory_pool.hpp"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>

namespace {

	const size_t MIN_BLOCK_ALIGN = 64;

	inline uint8_t * malloc_block(size_t block_size, size_t alignment)
	{
#if defined( _WIN32 )
		return reinterpret_cast<uint8_t*>(
				_aligned_malloc(block_size, alignment));
#else
		void * ptr;
		posix_memalign(&ptr, alignment, block_size);
		return reinterpret_cast<uint8_t *>(ptr);
#endif
	}

	inline void free_block(uint8_t * ptr)
	{
#if defined( _WIN32 )
		_aligned_free(ptr);
#else
		std::free(ptr);
#endif
	}

	/// Returns the next unused bit index
	template <typename T>
	inline T find_slot(T n)
	{
		// create a word with a single 1-bit at the position of the rightmost
		// 0-bit in x, producing 0 if none, then count the trailing zeros.
		return __builtin_ctz(~n & (n + 1));
	}


	/// Returns the number of allocations in the given mask
	template <typename T>
	inline T allocation_count(T n)
	{
		return __builtin_popcount(n);
	}

	/// Returns true if the pointer is of the given alignment
	inline bool is_aligned(const void * ptr, size_t align)
	{
		return (reinterpret_cast<uintptr_t>(ptr) & (align - 1)) == 0;
	}
}

MemoryPoolBase::MemoryPoolBase(uint32_t entry_size) :
	stride_(entry_size),
	free_block_(0),
	generation_(0)
{
}

MemoryPoolBase::~MemoryPoolBase()
{
	mask_t composite_mask = 0;
	for (const auto block_mask : block_masks_)
	{
		composite_mask = composite_mask | block_mask;
	}
	assert(composite_mask == 0 && "pool memory still allocated");
	for (auto block : blocks_)
	{
		free_block(block);
	}
}

MemoryPoolStats MemoryPoolBase::get_stats() const
{
	MemoryPoolStats stats;
	stats.block_count = block_masks_.size();
	stats.allocation_count = 0;
	for (const auto block_mask : block_masks_)
	{
		stats.allocation_count += allocation_count(block_mask);
	}
	return stats;
}

void * MemoryPoolBase::allocate()
{
	increment_generation();

	// find a block with space
	const size_t last_block = block_masks_.size();
	size_t free_block;
	for (free_block = free_block_; free_block < last_block; ++free_block)
	{
		// found a block that isn't full
		if (block_masks_[free_block] != ~0u) break;
	}

	// if no free space, allocate a new block and return first entry
	if (free_block == block_masks_.size())
	{
		uint8_t * new_block = malloc_block(NUM_BLOCK_ENTRIES * stride_,
				MIN_BLOCK_ALIGN);
		blocks_.push_back(new_block);
		block_masks_.push_back(0x1);
		// this is the new free block
		free_block_ = free_block;
		return new_block;
	}

	// find free entry in the block
	uint32_t block_mask = block_masks_[free_block];
	uint32_t slot = find_slot(block_mask);

	assert(slot < NUM_BLOCK_ENTRIES);
	assert((block_mask & (1 << slot)) == 0);

	// get the offset for this allocation and flag the bit as used
	uint8_t * ptr = blocks_[free_block] + stride_ * slot;
	block_masks_[free_block] = block_mask | (1 << slot);

	// increment free block if it's full
	free_block_ = free_block + (block_masks_[free_block] == ~0u);

	return ptr;
}

void MemoryPoolBase::deallocate(void * ptr)
{
	increment_generation();

	const size_t block_size = NUM_BLOCK_ENTRIES * stride_;
	uint8_t ** blocks = blocks_.data();
	uint32_t * block_masks = block_masks_.data();
	assert(blocks_.size() == block_masks_.size());
	const size_t block_count = blocks_.size();

	// search for the block containing ptr
	for (size_t block_index = 0; block_index < block_count; ++block_index)
	{
		const auto block_start = *blocks;
		const auto block_end = block_start + block_size;
		// check if pointer is in range
		if (ptr >= block_start && ptr < block_end)
		{
			uint8_t * entry = static_cast<uint8_t *>(ptr);
			// get the index of the entry
			size_t index = (entry - block_start) / stride_;
			// assert that the stride is correct
			assert((block_start + (index * stride_)) == entry);
			// flag the block mask bit as unused
			uint32_t bit = 1 << index;
			*block_masks = *block_masks & ~bit;

			// if this block is now empty then reclaim memory
			if (*block_masks == 0)
			{
				free_block(block_start);
				block_masks_.erase(block_masks_.begin() + block_index);
				blocks_.erase(blocks_.begin() + block_index);
			}

			// check if this block is closer to the front of the list
			if (block_index < free_block_)
			{
				// this block now has some space
				free_block_ = block_index;
			}

			// ensure free block is in range
			if (free_block_ >= block_masks_.size())
			{
				free_block_ = std::max<size_t>(1, block_masks_.size()) - 1;
			}

			return;
		}
		++blocks;
		++block_masks;
	}

	assert(false && "failed to deallocate entry");
}


uint8_t * MemoryPoolBase::element_at(size_t index)
{
	const size_t block_index = index / NUM_BLOCK_ENTRIES;
	const size_t bit_index = index - (block_index * NUM_BLOCK_ENTRIES);
	assert(block_index < block_masks_.size());
	assert(bit_index < NUM_BLOCK_ENTRIES);
	assert(block_masks_[block_index] & (1 << bit_index));
	return blocks_[block_index] + bit_index * stride_;
}

const uint8_t * MemoryPoolBase::element_at(size_t index) const
{
	return const_cast<MemoryPoolBase *>(this)->element_at(index);
}

size_t MemoryPoolBase::next_index(size_t index) const
{
	const size_t block_count = block_masks_.size();
	size_t block_index = index / NUM_BLOCK_ENTRIES;
	size_t bit_index = index - (block_index * NUM_BLOCK_ENTRIES);
	assert(bit_index < NUM_BLOCK_ENTRIES);
	while (block_index < block_count)
	{
		// found a valid entry
		if (block_masks_[block_index] & (1 << bit_index))
		{
			return (block_index * NUM_BLOCK_ENTRIES) + bit_index;
		}
		++bit_index;
		if (bit_index == NUM_BLOCK_ENTRIES)
		{
			++block_index;
		}
	}
	// past the last block
	return block_count * NUM_BLOCK_ENTRIES;
}

size_t MemoryPoolBase::end_index() const
{
	return block_masks_.size() * NUM_BLOCK_ENTRIES;
}

uint32_t MemoryPoolBase::generation() const
{
	return generation_;
}

void MemoryPoolBase::increment_generation()
{
	uint32_t generation = generation_ + 1;
	generation_ = generation == INVALID_GENERATION ? 0 : generation;
}

bool MemoryPoolBase::check_generation(uint32_t generation) const
{
	return generation_ == generation;
}

//
// Tests
//

#include "catch.hpp"

TEST_CASE("Single new and delete", "[allocation]")
{
	MemoryPool<uint32_t> mp;
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
	MemoryPool<uint32_t> mp;
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
	size_t i;
	for (i = 0; i < 64; ++i)
	{
		uint32_t * p = mp.new_object(1 << i);
		REQUIRE(p != nullptr);
		CHECK(*p == 1 << i);
		v.push_back(p);
	}

	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 64);
		CHECK(stats.block_count == 2);
	}

	// check values
	i = 0;
	for (auto itr = mp.begin(), end = mp.end(); itr != end; ++itr)
	{
		CHECK(*itr == 1 << i);
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
		CHECK(stats.allocation_count == 32);
		CHECK(stats.block_count == 2);
	}

	// check remaining objects
	i = 0;
	for (auto itr = mp.begin(), end = mp.end(); itr != end; ++itr)
	{
		CHECK(*itr == 1 << i);
		i += 2;
	}

	// allocate 16 new entries (fill first block)
    for (i = 1; i < 32; i += 2)
	{
		CHECK(v[i] == nullptr);
        v[i] = mp.new_object(1 << i);
	}

	// check allocation and block count
	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 48);
		CHECK(stats.block_count == 2);
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
		CHECK(stats.allocation_count == 32);
		CHECK(stats.block_count == 1);
	}

	for (auto p : v)
	{
		mp.delete_object(p);
	}

	{
		auto stats = mp.get_stats();
		CHECK(stats.allocation_count == 0);
		CHECK(stats.block_count == 0);
	}
}
