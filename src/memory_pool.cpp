#include "memory_pool.h"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>

namespace {
	const size_t POOL_ENTRIES = sizeof(uint32_t) * 8;
	const size_t CACHE_ALIGN = 64 / sizeof(void*);
	
	inline uint8_t * malloc_block(size_t block_size)
	{
#if defined( _WIN32 )
		return reinterpret_cast<uint8_t*>(
				_aligned_malloc(block_size, CACHE_ALIGN * sizeof(void*)));
#else
		void * ptr;
		posix_memalign(&ptr, CACHE_ALIGN, block_size);
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
}

MemoryPoolBase::MemoryPoolBase(uint32_t entry_size) :
	stride_(entry_size)
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
	// find a block with space
	size_t free_block = 0;
	for (const auto block_mask : block_masks_)
	{
		if (block_mask != ~0u) break;
		++free_block;
	}

	// if no free space, allocate a new block and return first entry
	if (free_block == block_masks_.size())
	{
		uint8_t * new_block = malloc_block(POOL_ENTRIES * stride_);
		blocks_.push_back(new_block);
		block_masks_.push_back(0x1);
		return new_block;
	}

	// find free entry in the block
	uint32_t block_mask = block_masks_[free_block];
	// TODO: could probably do some bit magic here
	for (size_t i = 0; i < POOL_ENTRIES; ++i)
	{
		uint32_t bit = 1 << i;
		if ((block_mask & bit) == 0)
		{
			uint8_t * ptr = blocks_[free_block] + stride_ * i;
			block_masks_[free_block] = block_mask | bit;
			return ptr;
		}
	}
	
	assert(false && "failed to allocate entry");

	// if we got here then something bad happened
	return nullptr;
}

void MemoryPoolBase::deallocate(void * ptr)
{
	const size_t block_size = POOL_ENTRIES * stride_;
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
