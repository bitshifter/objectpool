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
	typedef uint32_t index_t;

protected:
	/// Indices storage are used both as a free list and a used list.
	/// A free index contains the index of the next free index.
	/// A used indice contains the index of itself.
	index_t * indices_;
	/// The first free index or max_entries_ if no free entries.
	index_t free_head_index_;
	/// The start of pool memory.
	uint8_t * pool_mem_;
	/// The maximum number of pool entries
	const index_t max_entries_;

public:
	MemoryPoolStats get_stats() const;

protected:
	MemoryPoolBase(size_t entry_size, index_t max_entries);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;
};


template <typename T>
class MemoryPool : public MemoryPoolBase
{
public:
	MemoryPool(index_t max_entries);

	template<class... P>
		T * new_object(P&&... params);

	void delete_object(const T * ptr);

	template <typename F>
		void for_each(const F func) const;

protected:

	MemoryPool(const MemoryPool &) = delete;
	MemoryPool & operator=(const MemoryPoolBase &) = delete;

	/// Returns element at a given index, no range checking is performed
	T * entry_at(index_t index);
	const T * entry_at(index_t index) const;

	/// Returns the index of the given pointer
	index_t index_of(const T * ptr) const;

	/// Entry size or alignment if greater
	static const size_t entry_size = alignof(T) > sizeof(T) ? alignof(T) : sizeof(T);
};

template <typename T>
MemoryPool<T>::MemoryPool(index_t max_entries) :
	MemoryPoolBase(entry_size, max_entries) {}

template <typename T>
template <class... P>
T * MemoryPool<T>::new_object(P&&... params)
{
	if (free_head_index_ != max_entries_)
	{
		index_t index = free_head_index_;
		assert(indices_[index] != index);
		free_head_index_ = indices_[index];
		indices_[index] = index;
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
	index_t index = index_of(ptr);
	assert(indices_[index] == index);
	indices_[index] = free_head_index_;
	// store index of next free entry in this pointer
	free_head_index_ = index;
}

template <typename T>
template <typename F>
void MemoryPool<T>::for_each(const F func) const
{
	for (index_t i = 0, count = max_entries_; i != count; ++i)
	{
		T * first = reinterpret_cast<T*>(pool_mem_);
		if (indices_[i] == i)
		{
			func(first + i);
		}
	}
}

template <typename T>
inline T * MemoryPool<T>::entry_at(index_t index)
{
	return reinterpret_cast<T*>(pool_mem_ + (index * entry_size));
}

template <typename T>
inline const T * MemoryPool<T>::entry_at(index_t index) const
{
	return const_cast<MemoryPool<T> *>(this)->entry_at(index);
}

template <typename T>
MemoryPoolBase::index_t MemoryPool<T>::index_of(const T * ptr) const
{
	return ptr - reinterpret_cast<const T*>(pool_mem_);
}

#endif // _BITS_MEMORY_POOL_H_

