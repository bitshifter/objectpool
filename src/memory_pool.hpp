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

namespace detail
{
typedef uint32_t index_t;

template <typename T>
class MemoryPoolBlock
{
    index_t free_head_index_;
    const index_t entries_per_block_;

    MemoryPoolBlock(index_t entries_per_block);
    ~MemoryPoolBlock();

    MemoryPoolBlock(const MemoryPoolBlock &) = delete;
    MemoryPoolBlock & operator=(const MemoryPoolBlock &) = delete;

    /// returns start of indices
    index_t * indices_begin();

    /// returns start of pool memory
    T * memory_begin();

public:
    static MemoryPoolBlock<T> * create(uint32_t entries_per_block);
    static void destroy(MemoryPoolBlock<T> * ptr);

    template <class... P>
    T * new_object(P&&... params);

    void delete_object(const T * ptr);

    template <typename F>
    void for_each(const F func);
};

} // namespace detail


template <typename T>
class FixedMemoryPool
{
    detail::MemoryPoolBlock<T> * block_;

    FixedMemoryPool(const FixedMemoryPool &) = delete;
    FixedMemoryPool & operator=(const FixedMemoryPool &) = delete;

public:
    typedef detail::index_t index_t;

    FixedMemoryPool(index_t max_entries);
    ~FixedMemoryPool();

    template<class... P>
    T * new_object(P&&... params);

    void delete_object(const T * ptr);

    template <typename F>
    void for_each(const F func) const;

    MemoryPoolStats get_stats() const;
};


namespace detail
{

const uint32_t MIN_BLOCK_ALIGN = 64;

void * aligned_malloc(size_t size, size_t align);
void aligned_free(void * ptr);

size_t align_to(size_t n, size_t align);

template <typename T>
MemoryPoolBlock<T> * MemoryPoolBlock<T>::create(index_t entries_per_block)
{
    // the header size
    const size_t header_size = sizeof(MemoryPoolBlock<T>);
    // extend indices size by alignment of T
    const size_t indices_size =
            align_to(sizeof(index_t) * entries_per_block, alignof(T));
    // align block to cache line size, or entry alignment if larger
    const size_t entries_size = sizeof(T) * entries_per_block;
    // block size includes indices + entry alignment + entries
    const size_t block_size = header_size + indices_size + entries_size;
    MemoryPoolBlock<T> * ptr = reinterpret_cast<MemoryPoolBlock<T>*>(
                aligned_malloc(block_size, MIN_BLOCK_ALIGN));
    if (ptr)
    {
        new (ptr) MemoryPoolBlock(entries_per_block);
    }
    assert(reinterpret_cast<uint8_t*>(ptr->indices_begin())
           == reinterpret_cast<uint8_t*>(ptr) + header_size);
    assert(reinterpret_cast<uint8_t*>(ptr->memory_begin())
           == reinterpret_cast<uint8_t*>(ptr) + header_size + indices_size);
    return ptr;
}

template <typename T>
void MemoryPoolBlock<T>::destroy(MemoryPoolBlock<T> * ptr)
{
    ptr->~MemoryPoolBlock();
    aligned_free(ptr);
}

template <typename T>
MemoryPoolBlock<T>::MemoryPoolBlock(index_t entries_per_block) :
    free_head_index_(0),
    entries_per_block_(entries_per_block)
{
    index_t * indices = indices_begin();
    for (index_t i = 0; i < entries_per_block; ++i)
    {
        indices[i] = i + 1;
    }
}

template <typename T>
MemoryPoolBlock<T>::~MemoryPoolBlock()
{
    // TODO: assert empty
}

template <typename T>
index_t * MemoryPoolBlock<T>::indices_begin()
{
    return reinterpret_cast<index_t*>(this + 1);
}

template <typename T>
T * MemoryPoolBlock<T>::memory_begin()
{
    return reinterpret_cast<T*>(indices_begin() + entries_per_block_);
}

template <typename T>
template <class... P>
T * MemoryPoolBlock<T>::new_object(P&&... params)
{
    // get the head of the free list
    const index_t index = free_head_index_;
    if (index != entries_per_block_)
    {
        index_t * indices = indices_begin();
        // assert that this index is not in use
        assert(indices[index] != index);
        // update head of the free list
        free_head_index_ = indices[index];
        // flag index as used
        indices[index] = index;
        T * ptr = memory_begin() + index;
        new (ptr) T(std::forward<P>(params)...);
        return ptr;
    }
    return nullptr;
}

template <typename T>
void MemoryPoolBlock<T>::delete_object(const T * ptr)
{
    if (ptr)
    {
        // assert that pointer is in range
        assert(ptr >= memory_begin() && ptr < (memory_begin() + entries_per_block_));

        // destruct this object
        ptr->~T();

        // get the index of this pointer
        const index_t index = ptr - memory_begin();

        index_t * indices = indices_begin();

        // assert this index is allocated
        assert(indices[index] == index);

        // remove index from used list
        indices[index] = free_head_index_;

        // store index of next free entry in this pointer
        free_head_index_ = index;
    }
}

template <typename T>
template <typename F>
void MemoryPoolBlock<T>::for_each(const F func)
{
    const index_t * indices = indices_begin();
    T * first = memory_begin();
    for (index_t i = 0, count = entries_per_block_; i != count; ++i)
    {
        if (indices[i] == i)
        {
            func(first + i);
        }
    }
}

} // namespace detail

template <typename T>
FixedMemoryPool<T>::FixedMemoryPool(index_t max_entries) :
    block_(detail::MemoryPoolBlock<T>::create(max_entries)) {}

template <typename T>
FixedMemoryPool<T>::~FixedMemoryPool()
{
    detail::MemoryPoolBlock<T>::destroy(block_);
}

template <typename T>
template <class... P>
T * FixedMemoryPool<T>::new_object(P&&... params)
{
    return block_->new_object(std::forward<P>(params)...);
}

template <typename T>
void FixedMemoryPool<T>::delete_object(const T * ptr)
{
    block_->delete_object(ptr);
}

template <typename T>
template <typename F>
void FixedMemoryPool<T>::for_each(const F func) const
{
    block_->for_each(func);
}

template <typename T>
MemoryPoolStats FixedMemoryPool<T>::get_stats() const
{
    MemoryPoolStats stats;
    stats.block_count = 1;
    stats.allocation_count = 0;
    block_->for_each([&stats](T *) { ++stats.allocation_count; });
    return stats;
}

#endif // _BITS_MEMORY_POOL_H_

