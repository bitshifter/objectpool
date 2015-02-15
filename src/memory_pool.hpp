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

private:
	uint_t max_entries_;
	uint_t entry_size_;
	uint_t num_free_entries_;
	uint_t num_initialised_;
	uint8_t * pool_mem_;
	uint8_t * next_free_;

public:
	MemoryPoolStats get_stats() const;

protected:
	MemoryPoolBase(uint_t entry_size, uint_t max_entries);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;

	void * allocate();
	void deallocate(void * ptr);

	/// Returns element at a given index, no range checking is performed
	uint8_t * element_at(uint_t index);
	const uint8_t * element_at(uint_t index) const;

	/// Returns the index of the given pointer
	uint_t index_of(const uint8_t * ptr) const;

	/// Returns the next valid index from and including the given index.
	size_t next_index(size_t index) const;
	/// Returns the index of the first element past the end of the last block.
	size_t end_index() const;

	/// Returns the current generation for checking iterators are valid
	uint32_t generation() const;

	/// Returns true if the given generation is valid
	bool check_generation(uint32_t generation) const;

private:
	void increment_generation();
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

	/*
	class const_iterator : public std::iterator<std::forward_iterator_tag, T>
	{
		friend class MemoryPool<T>;
		const MemoryPool<T> * pool_;
		size_t index_;
		uint32_t generation_;
	protected:
		const T * get() const
		{
			assert(pool_->check_generation(generation_));
			return reinterpret_cast<const T *>(pool_->element_at(index_));
		}
		void inc()
		{
			assert(pool_->check_generation(generation_));
			index_ = pool_->next_index(index_ + 1);
		}
		const_iterator(const MemoryPool<T> * pool, size_t index) :
			pool_(pool), index_(index), generation_(pool_->generation()) {}
	public:
		const_iterator() : pool_(nullptr), index_(0),
			generation_(MemoryPool<T>::INVALID_GENERATION)  {}
		bool operator!=(const const_iterator & itr) const
		{
			return !(pool_ == itr.pool_ && index_ == itr.index_);
		}
		const T & operator*() const
		{
			return *get();
		}
		const T * operator->() const
		{
			return get();
		}
		const_iterator & operator++()
		{
			inc();
			return *this;
		}
		const_iterator operator++(int)
		{
			const_iterator itr(*this);
			inc();
			return itr;
		}
	};

	class iterator : public const_iterator
	{
		friend class MemoryPool<T>;
		iterator(const MemoryPool<T> * pool, size_t index) :
			const_iterator(pool, index) {}
	public:
		iterator() : const_iterator() {}
		T & operator*() const
		{
			return *const_cast<T *>(const_iterator::get());
		}
		T * operator->() const
		{
			return const_cast<T *>(const_iterator::get());
		}
		iterator & operator++()
		{
			const_iterator::inc();
			return *this;
		}
		iterator operator++(int)
		{
			iterator itr(*this);
			const_iterator::inc();
			return itr;
		}
	};

	const_iterator begin() const
	{
		return const_iterator(this, next_index(0));
	}

	const_iterator end() const
	{
		return const_iterator(this, end_index());
	}

	iterator begin()
	{
		return iterator(this, next_index(0));
	}

	iterator end()
	{
		return iterator(this, end_index());
	}

	template <typename F>
	void for_each(const F func)
	{
		for (auto itr = begin(), last = end(); itr != last; ++itr)
		{
			func(&*itr);
		}
	}
	*/
};

#endif // _BITS_MEMORY_POOL_H_

