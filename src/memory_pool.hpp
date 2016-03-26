#ifndef _BITS_MEMORY_POOL_H_
#define _BITS_MEMORY_POOL_H_

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace detail
{
/// Default index type, this dictates the maximum number of entries in a
/// single pool block.
typedef uint32_t index_t;

/// Base memory pool block. This contains a list of indices of free and used
/// entries and the storage for the entries themselves. Everything is allocated
/// in a single allocation in the static create function, and indices_begin()
/// and memory_begin() methods will return pointers offset from this for their
/// respective data.
template <typename T>
class MemoryPoolBlock
{
    /// Index of the first free entry
    index_t free_head_index_;
    const index_t entries_per_block_;

    /// Constructor and destructor are private as create and destroy should
    /// be used instead.
    MemoryPoolBlock(index_t entries_per_block);
    ~MemoryPoolBlock();

    MemoryPoolBlock(const MemoryPoolBlock &) = delete;
    MemoryPoolBlock & operator=(const MemoryPoolBlock &) = delete;

    /// returns start of indices
    index_t * indices_begin() const;

    /// returns start of pool memory
    T * memory_begin() const;

public:
    /// Creates to MemoryPoolBlock object and storage in a single aligned
    /// allocation.
    static MemoryPoolBlock<T> * create(index_t entries_per_block);

    /// Destroys the MemoryPoolBlock and associated storage.
    static void destroy(MemoryPoolBlock<T> * ptr);

    /// Allocates a new object from this block. Returns nullptr if there is
    /// no available space.
    template <class... P>
    T * new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by this block.
    void delete_object(const T * ptr);

    /// Delete all current allocations and reinitialise the block
    void delete_all();

    /// Calls given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// returns start of pool memory
    const T * memory_offset() const;

    /// Calculates the number of allocated entries
    index_t num_allocations() const;
};

} // namespace detail


/// Memory pool statistics structure used for returning information about
/// pool usage.
struct MemoryPoolStats
{
    size_t num_blocks = 0;
    size_t num_allocations = 0;
};


/// FixedMemoryPool contains a single MemoryPoolBlock, it will not grow
/// beyond the max number of entries given at construction time.
template <typename T>
class FixedMemoryPool
{
public:
    typedef detail::index_t index_t;
    typedef T value_type;

    FixedMemoryPool(index_t max_entries);
    ~FixedMemoryPool();

    /// Constructs a new object from the pool. Returns nullptr if there is no
    /// available space.
    template<class... P>
    T * new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by the pool.
    void delete_object(const T * ptr);

    /// Delete all current allocations
    void delete_all();

    /// Calls the given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// Calculates memory pool stats
    MemoryPoolStats calc_stats() const;

private:
    typedef detail::MemoryPoolBlock<T> Block;
    Block * block_;

    FixedMemoryPool(const FixedMemoryPool &) = delete;
    FixedMemoryPool & operator=(const FixedMemoryPool &) = delete;
};


/// DynamicMemoryPool contains a dynamic array of MemoryPoolBlocks.
template <typename T>
class DynamicMemoryPool
{
public:
    typedef detail::index_t index_t;
    typedef T value_type;

    DynamicMemoryPool(index_t entries_per_block);
    ~DynamicMemoryPool();

    /// Constructs a new object from the pool. Returns nullptr if there is no
    /// available space.
    template<class... P>
    T * new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by the pool.
    void delete_object(const T * ptr);

    /// Delete all current allocations
    void delete_all();

    /// Reclaim unused memory pool blocks
    void reclaim_memory();

    /// Calls the given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// Calculates memory pool stats
    MemoryPoolStats calc_stats() const;

private:
    typedef detail::MemoryPoolBlock<T> Block;

    /// The BlockInfo struct keeps regularly accessed block information
    /// packed together for better memory locality.
    struct BlockInfo
    {
        /// cache the number of free entries for this block
        index_t num_free_;
        /// cache the offset of entries memory from the start of the block
        const T * offset_;
        /// pointer to the block itself
        Block * block_;
    };

    /// storage for block info records
    BlockInfo * block_info_;
    /// number of blocks allocated
    index_t num_blocks_;
    /// index of the first block info with space
    index_t free_block_index_;
    /// the number of entries in each block
    const index_t entries_per_block_;

    /// Adds a new block and updates the free_block_index.
    BlockInfo * add_block();

    DynamicMemoryPool(const DynamicMemoryPool &) = delete;
    DynamicMemoryPool & operator=(const DynamicMemoryPool &) = delete;
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
#if _MSC_VER <= 1800
    const size_t entry_align = __alignof(T);
#else
    const size_t entry_align = alignof(T);
#endif
    // extend indices size by alignment of T
    const size_t indices_size =
        align_to(sizeof(index_t) * entries_per_block, entry_align);
    // align block to cache line size, or entry alignment if larger
    const size_t entries_size = sizeof(T) * entries_per_block;
    // block size includes indices + entry alignment + entries
    const size_t block_size = header_size + indices_size + entries_size;
    MemoryPoolBlock<T> * ptr = reinterpret_cast<MemoryPoolBlock<T>*>(
                aligned_malloc(block_size, MIN_BLOCK_ALIGN));
    if (ptr)
    {
        new (ptr) MemoryPoolBlock(entries_per_block);
        assert(reinterpret_cast<uint8_t*>(ptr->indices_begin())
               == reinterpret_cast<uint8_t*>(ptr) + header_size);
        assert(reinterpret_cast<uint8_t*>(ptr->memory_begin())
               == reinterpret_cast<uint8_t*>(ptr) + header_size + indices_size);
    }
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
void destruct_all(MemoryPoolBlock<T> &, typename std::enable_if<
    std::is_trivially_destructible<T>::value>::type* = 0)
{
    // skip calling destructors for trivially destructible types
}

template <typename T>
void destruct_all(MemoryPoolBlock<T> & t, typename std::enable_if<
    !std::is_trivially_destructible<T>::value>::type* = 0)
{
    // call destructors on all live objects in the pool
    t.for_each([](T * ptr){ ptr->~T(); });
}

template <typename T>
MemoryPoolBlock<T>::~MemoryPoolBlock()
{
    // destruct any allocated objects
    destruct_all(*this);
}

template <typename T>
index_t * MemoryPoolBlock<T>::indices_begin() const
{
    // calculcates the start of the indicies
    return reinterpret_cast<index_t*>(
        const_cast<MemoryPoolBlock<T>*>(this + 1));
}

template <typename T>
T * MemoryPoolBlock<T>::memory_begin() const
{
    // calculates the start of pool memory
    return reinterpret_cast<T*>(indices_begin() + entries_per_block_);
}

template <typename T>
const T * MemoryPoolBlock<T>::memory_offset() const
{
    return memory_begin();
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
        // flag index as used by assigning it's own index
        indices[index] = index;
        // get entry memory
        T * ptr = memory_begin() + index;
        // construct the entry
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
        const T * begin = memory_begin();
        assert(ptr >= begin && ptr < (begin + entries_per_block_));
        // destruct this object
        ptr->~T();
        // get the index of this pointer
        const index_t index = ptr - begin;
        index_t * indices = indices_begin();
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
void MemoryPoolBlock<T>::for_each(const F func) const
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

template <typename T>
void MemoryPoolBlock<T>::delete_all()
{
    // destruct any allocated objects
    destruct_all(*this);
    free_head_index_ = 0;
    index_t * indices = indices_begin();
    for (index_t i = 0; i < entries_per_block_; ++i)
    {
        indices[i] = i + 1;
    }
}

template <typename T>
index_t MemoryPoolBlock<T>::num_allocations() const
{
    index_t num_allocs = 0;
    for_each([&num_allocs](const T *){ ++num_allocs; });
    return num_allocs;
}

} // namespace detail

template <typename T>
FixedMemoryPool<T>::FixedMemoryPool(index_t max_entries) :
    block_(Block::create(max_entries)) {}

template <typename T>
FixedMemoryPool<T>::~FixedMemoryPool()
{
    assert(calc_stats().num_allocations == 0);
    Block::destroy(block_);
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
void FixedMemoryPool<T>::delete_all()
{
    block_->delete_all();
}

template <typename T>
template <typename F>
void FixedMemoryPool<T>::for_each(const F func) const
{
    block_->for_each(func);
}

template <typename T>
MemoryPoolStats FixedMemoryPool<T>::calc_stats() const
{
    MemoryPoolStats stats;
    stats.num_blocks = 1;
    stats.num_allocations = block_->num_allocations();
    return stats;
}

template <typename T>
DynamicMemoryPool<T>::DynamicMemoryPool(index_t entries_per_block) :
    block_info_(nullptr),
    num_blocks_(0),
    free_block_index_(0),
    entries_per_block_(entries_per_block)
{
    // always have one block available
    add_block();
}

template <typename T>
DynamicMemoryPool<T>::~DynamicMemoryPool()
{
    // explicitly delete_object or delete_all before pool goes out of scope
    assert(calc_stats().num_allocations == 0);
}

template <typename T>
typename DynamicMemoryPool<T>::BlockInfo * DynamicMemoryPool<T>::add_block()
{
    assert(free_block_index_ == num_blocks_);
    if (Block * block = Block::create(entries_per_block_))
    {
        // update the number of blocks
        ++num_blocks_;
        // allocate space for new block info
        block_info_ = reinterpret_cast<BlockInfo*>(
            realloc(block_info_, num_blocks_ * sizeof(BlockInfo)));
        // initialise the new block info structure
        BlockInfo & info = block_info_[free_block_index_];
        info.num_free_ = entries_per_block_;
        info.offset_ = block->memory_offset();
        info.block_ = block;
        return &info;
    }
    return nullptr;
}

template <typename T>
template <typename... P>
T * DynamicMemoryPool<T>::new_object(P&&... params)
{
    assert(free_block_index_ < num_blocks_);

    // search for a block with free space
    BlockInfo * p_info = block_info_ + free_block_index_;
    for (const BlockInfo * p_end = block_info_ + num_blocks_;
         p_info != p_end && p_info->num_free_ == 0; ++p_info) {}

    // update the free block index
    free_block_index_ = p_info - block_info_;

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
    T * ptr = p_info->block_->new_object(std::forward<P>(params)...);
    assert(ptr != nullptr);
    // update num free count
    --p_info->num_free_;
    return ptr;
}

template <typename T>
void DynamicMemoryPool<T>::delete_object(const T * ptr)
{
    BlockInfo * p_info = block_info_;
    for (auto end = p_info + num_blocks_; p_info != end; ++p_info)
    {
        const T * p_entries_begin = p_info->offset_;
        const T * p_entries_end = p_entries_begin + entries_per_block_;
        if (ptr >= p_entries_begin && ptr < p_entries_end)
        {
            p_info->block_->delete_object(ptr);
            ++p_info->num_free_;
            const index_t free_block = p_info - block_info_;
            if (free_block < free_block_index_)
            {
                free_block_index_ = free_block;
            }
            return;
        }
    }
}

template <typename T>
void DynamicMemoryPool<T>::delete_all()
{
    for (BlockInfo * p_info = block_info_,
         * p_end = block_info_ + num_blocks_; p_info != p_end; ++p_info)
    {
        p_info->block_->delete_all();
        p_info->num_free_ = entries_per_block_;
    }
    free_block_index_ = 0;
}

template <typename T>
void DynamicMemoryPool<T>::reclaim_memory()
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
    block_info_ = reinterpret_cast<BlockInfo*>(
        realloc(block_info_, sizeof(BlockInfo) * num_blocks_));

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
void DynamicMemoryPool<T>::for_each(const F func) const
{
    for (const BlockInfo * p_info = block_info_,
         * p_end = block_info_ + num_blocks_; p_info != p_end; ++p_info)
    {
        if (p_info->num_free_ < entries_per_block_)
        {
            p_info->block_->for_each(func);
        }
    }
}

template <typename T>
MemoryPoolStats DynamicMemoryPool<T>::calc_stats() const
{
    MemoryPoolStats stats;
    stats.num_blocks = num_blocks_;
    stats.num_allocations = 0;
    for (const BlockInfo * p_info = block_info_,
         * p_end = block_info_ + num_blocks_; p_info != p_end; ++p_info)
    {
        if (p_info->num_free_ < entries_per_block_)
        {
            stats.num_allocations += p_info->block_->num_allocations();
        }
    }
    return stats;
}

#endif // _BITS_MEMORY_POOL_H_

