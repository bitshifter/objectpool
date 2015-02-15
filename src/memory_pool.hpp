#ifndef _BITS_MEMORY_POOL_H_
#define _BITS_MEMORY_POOL_H_

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

struct MemoryPoolStats
{
	size_t block_count = 0;
	size_t allocation_count = 0;
};

class MemoryPoolBase
{
public:
	typedef uint32_t uint_t;

protected:
	std::vector<uint_t> next_free_;
	std::vector<uint8_t> used_indices_;
	uint8_t * pool_mem_;
	const uint_t entry_size_;
	const uint_t max_entries_;

public:
	MemoryPoolStats get_stats() const;

protected:
	MemoryPoolBase(uint_t entry_size, uint_t max_entries);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;

	void * allocate();
	void deallocate(const void * ptr);

	/// Returns element at a given index, no range checking is performed
	uint8_t * element_at(uint_t index);
	const uint8_t * element_at(uint_t index) const;

	/// Returns the index of the given pointer
	uint_t index_of(const uint8_t * ptr) const;
};

template <typename T>
class MemoryPool : public MemoryPoolBase
{
public:
	MemoryPool(uint_t max_entries) :
		MemoryPoolBase(sizeof(T), max_entries) {}

	template<class... P>
	T * new_object(P&&... params)
	{
		T * ptr = reinterpret_cast<T *>(allocate());
		if (ptr)
		{
			new(ptr) T(std::forward<P>(params)...);
		}
		return ptr;
	}

	void delete_object(const T * ptr)
	{
		if (ptr)
		{
			ptr->~T();
			deallocate(const_cast<T*>(ptr));
		}
	}

	template <typename F>
	void for_each(const F func) const
	{
		uint_t index = 0;
		for (auto itr = used_indices_.begin(), end = used_indices_.end();
				itr != end; ++itr, ++index)
		{
			T * first = reinterpret_cast<T*>(pool_mem_);
			if (*itr)
			{
				func(first + index);
			}
		}
	}
};

#endif // _BITS_MEMORY_POOL_H_

