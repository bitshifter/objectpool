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
    /// The first free index or max_entries_ if no free entries.
    index_t free_head_index_;
    /// Indices storage are used both as a free list and a used list.
    /// A free index contains the index of the next free index.
    /// A used indice contains the index of itself.
    index_t * indices_;
    /// The start of pool memory.
    uint8_t * mem_;
    /// The maximum number of pool entries
    const index_t entries_per_block_;
    /// The size of each entry
    const uint32_t entry_stride_;

public:
    MemoryPoolStats get_stats() const;

protected:
    MemoryPoolBase(uint32_t entry_size, uint32_t entry_align, index_t max_entries);
    ~MemoryPoolBase();

    void * allocate();
    void deallocate(index_t index);

    void * entry_at(index_t index);

    MemoryPoolBase(const MemoryPoolBase &) = delete;
    MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;
};


template <typename T>
class StaticMemoryPool : public MemoryPoolBase
{
public:
    StaticMemoryPool(index_t max_entries);

    template<class... P>
    T * new_object(P&&... params);

    void delete_object(const T * ptr);

    template <typename F>
    void for_each(const F func) const;
};

template <typename T>
StaticMemoryPool<T>::StaticMemoryPool(index_t max_entries) :
    MemoryPoolBase(sizeof(T), alignof(T), max_entries) {}

template <typename T>
template <class... P>
T * StaticMemoryPool<T>::new_object(P&&... params)
{
    // allocate the memory
    void * ptr = allocate();
    if (ptr)
    {
        // construct the object
        new(ptr) T(std::forward<P>(params)...);
    }
    return reinterpret_cast<T*>(ptr);
}

template <typename T>
void StaticMemoryPool<T>::delete_object(const T * ptr)
{
    if (ptr)
    {
        // assert that pointer is in range
        assert(ptr >= entry_at(0) && ptr < entry_at(entries_per_block_));

        // destruct this object
        ptr->~T();

        // free the memory
        const index_t index = ptr - reinterpret_cast<const T*>(mem_);
        deallocate(index);
    }
}

template <typename T>
template <typename F>
void StaticMemoryPool<T>::for_each(const F func) const
{
    for (index_t i = 0, count = entries_per_block_; i != count; ++i)
    {
        T * first = reinterpret_cast<T*>(mem_);
        if (indices_[i] == i)
        {
            func(first + i);
        }
    }
}

#endif // _BITS_MEMORY_POOL_H_

