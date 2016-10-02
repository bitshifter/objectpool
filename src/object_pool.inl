// Header guards an include is for code completion in IDEs
// Don't include this file directly!
#ifndef _BITS_OBJECT_POOL_INL_
#define _BITS_OBJECT_POOL_INL_

#ifndef _BITS_OBJECT_POOL_HPP_
#include "object_pool.hpp"
#endif

/// Internal implementation
namespace detail
{

const uint32_t MIN_BLOCK_ALIGN = 64;

void* aligned_malloc(size_t size, size_t align);
void aligned_free(void* ptr);

// Aligns n to align. N will be unchanged if it is already aligned
inline size_t align_to(size_t n, size_t align)
{
    return (1 + (n - 1) / align) * align;
}

template <typename T>
ObjectPoolBlock<T>* ObjectPoolBlock<T>::create(index_t entries_per_block)
{
    // the header size
    const size_t header_size = sizeof(ObjectPoolBlock<T>);
#if _MSC_VER <= 1800
    const size_t entry_align = __alignof(T);
#else
    const size_t entry_align = alignof(T);
#endif
    // extend indices size by alignment of T
    const size_t indices_size = align_to(sizeof(index_t) * entries_per_block, entry_align);
    // align block to cache line size, or entry alignment if larger
    const size_t entries_size = sizeof(T) * entries_per_block;
    // block size includes indices + entry alignment + entries
    const size_t block_size = header_size + indices_size + entries_size;
    ObjectPoolBlock<T>* ptr =
        reinterpret_cast<ObjectPoolBlock<T>*>(aligned_malloc(block_size, MIN_BLOCK_ALIGN));
    if (ptr)
    {
        new (ptr) ObjectPoolBlock(entries_per_block);
        assert(reinterpret_cast<uint8_t*>(ptr->indices_begin())
            == reinterpret_cast<uint8_t*>(ptr) + header_size);
        assert(reinterpret_cast<uint8_t*>(ptr->memory_begin())
            == reinterpret_cast<uint8_t*>(ptr) + header_size + indices_size);
    }
    return ptr;
}

template <typename T>
void ObjectPoolBlock<T>::destroy(ObjectPoolBlock<T>* ptr)
{
    ptr->~ObjectPoolBlock();
    aligned_free(ptr);
}

template <typename T>
ObjectPoolBlock<T>::ObjectPoolBlock(index_t entries_per_block)
    : free_head_index_(0), entries_per_block_(entries_per_block)
{
    index_t* indices = indices_begin();
    for (index_t i = 0; i < entries_per_block; ++i)
    {
        indices[i] = i + 1;
    }
}

template <typename T>
void destruct_all(ObjectPoolBlock<T>&,
    typename std::enable_if<std::is_trivially_destructible<T>::value>::type* = 0)
{
    // skip calling destructors for trivially destructible types
}

template <typename T>
void destruct_all(ObjectPoolBlock<T>& t,
    typename std::enable_if<!std::is_trivially_destructible<T>::value>::type* = 0)
{
    // call destructors on all live objects in the pool
    t.for_each([](T* ptr)
        {
            ptr->~T();
        });
}

template <typename T>
ObjectPoolBlock<T>::~ObjectPoolBlock()
{
    // destruct any allocated objects
    destruct_all(*this);
}

template <typename T>
index_t* ObjectPoolBlock<T>::indices_begin() const
{
    // calculcates the start of the indicies
    return reinterpret_cast<index_t*>(const_cast<ObjectPoolBlock<T>*>(this + 1));
}

template <typename T>
T* ObjectPoolBlock<T>::memory_begin() const
{
    // calculates the start of pool memory
    return reinterpret_cast<T*>(indices_begin() + entries_per_block_);
}

template <typename T>
const T* ObjectPoolBlock<T>::memory_offset() const
{
    return memory_begin();
}

template <typename T>
template <class... P>
T* ObjectPoolBlock<T>::new_object(P&&... params)
{
    // get the head of the free list
    const index_t index = free_head_index_;
    if (index != entries_per_block_)
    {
        index_t* indices = indices_begin();
        // assert that this index is not in use
        assert(indices[index] != index);
        // update head of the free list
        free_head_index_ = indices[index];
        // flag index as used by assigning it's own index
        indices[index] = index;
        // get object memory
        T* ptr = memory_begin() + index;
        // construct the entry
        new (ptr) T(std::forward<P>(params)...);
        return ptr;
    }
    return nullptr;
}

template <typename T>
void ObjectPoolBlock<T>::delete_object(const T* ptr)
{
    if (ptr)
    {
        // assert that pointer is in range
        const T* begin = memory_begin();
        assert(ptr >= begin && ptr < (begin + entries_per_block_));
        // destruct this object
        ptr->~T();
        // get the index of this pointer
        const index_t index = static_cast<index_t>(ptr - begin);
        index_t* indices = indices_begin();
        // assert this index is allocated
        assert(indices[index] == index);
        // remove index from used list
        indices[index] = free_head_index_;
        // store index of next free entry in this entry
        free_head_index_ = index;
    }
}

template <typename T>
template <typename F>
void ObjectPoolBlock<T>::for_each(const F func) const
{
    const index_t* indices = indices_begin();
    T* first = memory_begin();
    for (index_t i = 0, count = entries_per_block_; i != count; ++i)
    {
        if (indices[i] == i)
        {
            func(first + i);
        }
    }
}

template <typename T>
void ObjectPoolBlock<T>::delete_all()
{
    // destruct any allocated objects
    destruct_all(*this);
    free_head_index_ = 0;
    index_t* indices = indices_begin();
    for (index_t i = 0; i < entries_per_block_; ++i)
    {
        indices[i] = i + 1;
    }
}

template <typename T>
index_t ObjectPoolBlock<T>::num_allocations() const
{
    index_t num_allocs = 0;
    for_each([&num_allocs](const T*)
        {
            ++num_allocs;
        });
    return num_allocs;
}

} // namespace detail

template <typename T>
FixedObjectPool<T>::FixedObjectPool(index_t max_entries)
    : block_(Block::create(max_entries))
{
}

template <typename T>
FixedObjectPool<T>::~FixedObjectPool()
{
    assert(calc_stats().num_allocations == 0);
    Block::destroy(block_);
}

template <typename T>
template <class... P>
T* FixedObjectPool<T>::new_object(P&&... params)
{
    return block_->new_object(std::forward<P>(params)...);
}

template <typename T>
void FixedObjectPool<T>::delete_object(const T* ptr)
{
    block_->delete_object(ptr);
}

template <typename T>
void FixedObjectPool<T>::delete_all()
{
    block_->delete_all();
}

template <typename T>
template <typename F>
void FixedObjectPool<T>::for_each(const F func) const
{
    block_->for_each(func);
}

template <typename T>
ObjectPoolStats FixedObjectPool<T>::calc_stats() const
{
    ObjectPoolStats stats;
    stats.num_blocks = 1;
    stats.num_allocations = block_->num_allocations();
    return stats;
}

template <typename T>
DynamicObjectPool<T>::DynamicObjectPool(index_t entries_per_block)
    : block_info_(nullptr),
      num_blocks_(0),
      free_block_index_(0),
      entries_per_block_(entries_per_block)
{
    // always have one block available
    add_block();
}

template <typename T>
DynamicObjectPool<T>::~DynamicObjectPool()
{
    // explicitly delete_object or delete_all before pool goes out of scope
    assert(calc_stats().num_allocations == 0);
}

template <typename T>
typename DynamicObjectPool<T>::BlockInfo* DynamicObjectPool<T>::add_block()
{
    assert(free_block_index_ == num_blocks_);
    if (Block* block = Block::create(entries_per_block_))
    {
        // update the number of blocks
        ++num_blocks_;
        // allocate space for new block info
        block_info_ =
            reinterpret_cast<BlockInfo*>(realloc(block_info_, num_blocks_ * sizeof(BlockInfo)));
        // initialise the new block info structure
        BlockInfo& info = block_info_[free_block_index_];
        info.num_free_ = entries_per_block_;
        info.offset_ = block->memory_offset();
        info.block_ = block;
        return &info;
    }
    return nullptr;
}

template <typename T>
template <typename... P>
T* DynamicObjectPool<T>::new_object(P&&... params)
{
    assert(free_block_index_ < num_blocks_);

    // search for a block with free space
    BlockInfo* p_info = block_info_ + free_block_index_;
    for (const BlockInfo* p_end = block_info_ + num_blocks_;
         p_info != p_end && p_info->num_free_ == 0; ++p_info)
    {
    }

    // update the free block index
    free_block_index_ = static_cast<index_t>(p_info - block_info_);

    // if no free blocks found then create a new one
    if (free_block_index_ == num_blocks_)
    {
        p_info = add_block();
        if (!p_info)
        {
            return nullptr;
        }
    }

    // construct the new object
    T* ptr = p_info->block_->new_object(std::forward<P>(params)...);
    assert(ptr != nullptr);
    // update num free count
    --p_info->num_free_;
    return ptr;
}

template <typename T>
void DynamicObjectPool<T>::delete_object(const T* ptr)
{
    BlockInfo* p_info = block_info_;
    for (auto end = p_info + num_blocks_; p_info != end; ++p_info)
    {
        const T* p_entries_begin = p_info->offset_;
        const T* p_entries_end = p_entries_begin + entries_per_block_;
        if (ptr >= p_entries_begin && ptr < p_entries_end)
        {
            p_info->block_->delete_object(ptr);
            ++p_info->num_free_;
            const index_t free_block = static_cast<index_t>(p_info - block_info_);
            if (free_block < free_block_index_)
            {
                free_block_index_ = free_block;
            }
            return;
        }
    }
}

template <typename T>
void DynamicObjectPool<T>::delete_all()
{
    for (BlockInfo *p_info = block_info_, *p_end = block_info_ + num_blocks_; p_info != p_end;
         ++p_info)
    {
        p_info->block_->delete_all();
        p_info->num_free_ = entries_per_block_;
    }
    free_block_index_ = 0;
}

template <typename T>
void DynamicObjectPool<T>::reclaim_memory()
{
    // loop through all blocks shuffling the used blocks to the front and unused
    // to the back.
    index_t used_index = num_blocks_;
    index_t empty_index = num_blocks_;
    for (index_t index = 0; index < num_blocks_; ++index)
    {
        if (block_info_[index].num_free_ != entries_per_block_)
        {
            used_index = index;
        }
        else if (index < empty_index)
        {
            empty_index = index;
        }

        if (empty_index < used_index && used_index != num_blocks_)
        {
            std::swap(block_info_[empty_index], block_info_[used_index]);
            used_index = empty_index;
            ++empty_index;
        }
    }

    // if no blocks are used, keep one around
    if (used_index == num_blocks_)
    {
        used_index = 0;
        free_block_index_ = 0;
    }

    // free remaining empty blocks
    for (index_t index = used_index + 1; index != num_blocks_; ++index)
    {
        Block::destroy(block_info_[index].block_);
    }

    // resize the block info array
    num_blocks_ = used_index + 1;
    block_info_ =
        reinterpret_cast<BlockInfo*>(realloc(block_info_, sizeof(BlockInfo) * num_blocks_));

    // find the first free block index
    free_block_index_ = num_blocks_;
    for (index_t index = 0; index != num_blocks_; ++index)
    {
        if (block_info_[index].num_free_ != 0)
        {
            free_block_index_ = index;
            break;
        }
    }
}

template <typename T>
template <typename F>
void DynamicObjectPool<T>::for_each(const F func) const
{
    for (const BlockInfo *p_info = block_info_, *p_end = block_info_ + num_blocks_; p_info != p_end;
         ++p_info)
    {
        if (p_info->num_free_ < entries_per_block_)
        {
            p_info->block_->for_each(func);
        }
    }
}

template <typename T>
ObjectPoolStats DynamicObjectPool<T>::calc_stats() const
{
    ObjectPoolStats stats;
    stats.num_blocks = num_blocks_;
    stats.num_allocations = 0;
    for (const BlockInfo *p_info = block_info_, *p_end = block_info_ + num_blocks_; p_info != p_end;
         ++p_info)
    {
        if (p_info->num_free_ < entries_per_block_)
        {
            stats.num_allocations += p_info->block_->num_allocations();
        }
    }
    return stats;
}

#endif // _BITS_OBJECT_POOL_INL_
