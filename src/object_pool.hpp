/*
 * Copyright (c) 2015 Cameron Hart
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
*/
#ifndef _BITS_OBJECT_POOL_HPP_
#define _BITS_OBJECT_POOL_HPP_

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

/// Internal details - look below this namespace for public classes!
namespace detail
{
/// Default index type, this dictates the maximum number of entries in a
/// single pool block.
typedef uint32_t index_t;

/// Base object pool block. This contains a list of indices of free and used
/// entries and the storage for the entries themselves. Everything is allocated
/// in a single allocation in the static create function, and indices_begin()
/// and memory_begin() methods will return pointers offset from this for their
/// respective data.
template <typename T>
class ObjectPoolBlock
{
    /// Index of the first free entry
    index_t free_head_index_;
    const index_t entries_per_block_;

    /// Constructor and destructor are private as create and destroy should
    /// be used instead.
    ObjectPoolBlock(index_t entries_per_block);
    ~ObjectPoolBlock();

    ObjectPoolBlock(const ObjectPoolBlock&) = delete;
    ObjectPoolBlock& operator=(const ObjectPoolBlock&) = delete;

    /// returns start of indices
    index_t* indices_begin() const;

    /// returns start of pool memory
    T* memory_begin() const;

public:
    /// Creates to ObjectPoolBlock object and storage in a single aligned
    /// allocation.
    static ObjectPoolBlock<T>* create(index_t entries_per_block);

    /// Destroys the ObjectPoolBlock and associated storage.
    static void destroy(ObjectPoolBlock<T>* ptr);

    /// Allocates a new object from this block. Returns nullptr if there is
    /// no available space.
    template <class... P>
    T* new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by this block.
    void delete_object(const T* ptr);

    /// Delete all current allocations and reinitialise the block
    void delete_all();

    /// Calls given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// returns start of pool memory
    const T* memory_offset() const;

    /// Calculates the number of allocated entries
    index_t num_allocations() const;
};

} // namespace detail


/// Object pool statistics structure used for returning information about
/// pool usage.
struct ObjectPoolStats
{
    size_t num_blocks = 0;
    size_t num_allocations = 0;
};


/// FixedObjectPool contains a single ObjectPoolBlock, it will not grow
/// beyond the max number of entries given at construction time.
template <typename T>
class FixedObjectPool
{
public:
    typedef detail::index_t index_t;
    typedef T value_t;

    FixedObjectPool(index_t max_entries);
    ~FixedObjectPool();

    /// Constructs a new object from the pool. Returns nullptr if there is no
    /// available space.
    template <class... P>
    T* new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by the pool.
    void delete_object(const T* ptr);

    /// Delete all current allocations
    void delete_all();

    /// Calls the given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// Calculates object pool stats
    ObjectPoolStats calc_stats() const;

private:
    typedef detail::ObjectPoolBlock<T> Block;
    Block* block_;

    FixedObjectPool(const FixedObjectPool&) = delete;
    FixedObjectPool& operator=(const FixedObjectPool&) = delete;
};


/// DynamicObjectPool contains a dynamic array of ObjectPoolBlocks.
template <typename T>
class DynamicObjectPool
{
public:
    typedef detail::index_t index_t;
    typedef T value_t;

    DynamicObjectPool(index_t entries_per_block);
    ~DynamicObjectPool();

    /// Constructs a new object from the pool. Returns nullptr if there is no
    /// available space.
    template <class... P>
    T* new_object(P&&... params);

    /// Deletes the given pointer. The pointer must be owned by the pool.
    void delete_object(const T* ptr);

    /// Delete all current allocations
    void delete_all();

    /// Reclaim unused object pool blocks
    void reclaim_memory();

    /// Calls the given function for all allocated entries
    template <typename F>
    void for_each(const F func) const;

    /// Calculates object pool stats
    ObjectPoolStats calc_stats() const;

private:
    typedef detail::ObjectPoolBlock<T> Block;

    /// The BlockInfo struct keeps regularly accessed block information
    /// packed together for better memory locality.
    struct BlockInfo
    {
        /// cache the number of free entries for this block
        index_t num_free_;
        /// cache the offset of object memory from the start of the block
        const T* offset_;
        /// pointer to the block itself
        Block* block_;
    };

    /// storage for block info records
    BlockInfo* block_info_;
    /// number of blocks allocated
    index_t num_blocks_;
    /// index of the first block info with space
    index_t free_block_index_;
    /// the number of entries in each block
    const index_t entries_per_block_;

    /// Adds a new block and updates the free_block_index.
    BlockInfo* add_block();

    DynamicObjectPool(const DynamicObjectPool&) = delete;
    DynamicObjectPool& operator=(const DynamicObjectPool&) = delete;
};

#include "object_pool.inl"

#endif // _BITS_OBJECT_POOL_HPP_
