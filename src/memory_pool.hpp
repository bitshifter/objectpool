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
	const uint_t max_entries_;

public:
	MemoryPoolStats get_stats() const;

protected:
	MemoryPoolBase(uint_t entry_size, uint_t max_entries);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;
};


template <typename T>
class MemoryPool : public MemoryPoolBase
{
public:
	MemoryPool(uint_t max_entries);

	template<class... P>
	T * new_object(P&&... params);

	void delete_object(const T * ptr);

	template <typename F>
	void for_each(const F func) const;

protected:

	MemoryPool(const MemoryPool &) = delete;
	MemoryPool & operator=(const MemoryPoolBase &) = delete;

	/// Returns element at a given index, no range checking is performed
	T * entry_at(uint_t index);
	const T * entry_at(uint_t index) const;

	/// Returns the index of the given pointer
	uint_t index_of(const T * ptr) const;

	static const uint_t entry_size = sizeof(T);
};

template <typename T>
MemoryPool<T>::MemoryPool(uint_t max_entries) :
	MemoryPoolBase(entry_size, max_entries) {}

template <typename T>
template<class... P>
T * MemoryPool<T>::new_object(P&&... params)
{
	if (!next_free_.empty())
	{
		uint_t index = next_free_.back();
		next_free_.pop_back();
		assert(!used_indices_[index]);
		used_indices_[index] = true;
		T * ptr = entry_at(index);
		new(ptr) T(std::forward<P>(params)...);
		return ptr;
	}
	else
	{
		return nullptr;
	}
}

template <typename T>
void MemoryPool<T>::delete_object(const T * ptr)
{
	if (!ptr) return;

	assert(ptr >= entry_at(0) && ptr < entry_at(max_entries_));

	// destruct this object
	ptr->~T();

	// remove index from used list
	uint_t used_index = index_of(ptr);
	assert(used_indices_[used_index]);
	used_indices_[used_index] = false;

	// store index of next free entry in this pointer
	next_free_.push_back(used_index);
}

template <typename T>
template <typename F>
void MemoryPool<T>::for_each(const F func) const
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

template <typename T>
inline T * MemoryPool<T>::entry_at(uint_t index)
{
	return reinterpret_cast<T*>(pool_mem_ + (index * entry_size));
}

template <typename T>
inline const T * MemoryPool<T>::entry_at(uint_t index) const
{
	return const_cast<MemoryPool<T> *>(this)->entry_at(index);
}

template <typename T>
MemoryPoolBase::uint_t MemoryPool<T>::index_of(const T * ptr) const
{
	return ptr - reinterpret_cast<const T*>(pool_mem_);
}

#endif // _BITS_MEMORY_POOL_H_

