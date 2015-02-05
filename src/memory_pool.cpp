#include "memory_pool.h"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>

namespace {
	
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
}

MemoryPoolBase::MemoryPoolBase(uint32_t entry_size) :
	stride_(entry_size),
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

void * MemoryPoolBase::allocate()
{
	increment_generation();

	// find a block with space
	size_t free_block = 0;
	for (const auto block_mask : block_masks_)
	{
		// found a block that isn't full so break
		if (block_mask != ~0u) break;
		++free_block;
	}

	// if no free space, allocate a new block and return first entry
	if (free_block == block_masks_.size())
	{
		uint8_t * new_block = malloc_block(NUM_BLOCK_ENTRIES * stride_,
				MIN_BLOCK_ALIGN);
		blocks_.push_back(new_block);
		block_masks_.push_back(0x1);
		return new_block;
	}

	// find free entry in the block
	uint32_t block_mask = block_masks_[free_block];
	uint32_t slot = find_slot(block_mask);

	assert(slot < NUM_BLOCK_ENTRIES);
	assert((block_mask & (1 << slot)) == 0);

	uint8_t * ptr = blocks_[free_block] + stride_ * slot;
	block_masks_[free_block] = block_mask | (1 << slot);

	return ptr;
}

void MemoryPoolBase::deallocate(void * ptr)
{
	increment_generation();

	const size_t block_size = NUM_BLOCK_ENTRIES * stride_;
	uint8_t ** blocks = blocks_.data();
	mask_t * block_masks = block_masks_.data();
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
			mask_t bit = 1 << index;
			*block_masks = *block_masks & ~bit;
			// TODO: check if this block is now empty and reclaim memory
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

