#ifndef _BITS_MEMORY_POOL_H_
#define _BITS_MEMORY_POOL_H_

#include <cstdint>
#include <memory>
#include <vector>

class MemoryPoolBase
{
	typedef uint32_t mask_t;
	std::vector<uint8_t *> blocks_;
	std::vector<mask_t> block_masks_;
	const uint32_t stride_;
public:
	MemoryPoolBase(uint32_t entry_size);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;

	void * allocate();
	void deallocate(void * ptr);
};

#endif // _BITS_MEMORY_POOL_H_

