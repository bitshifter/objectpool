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
protected:
	MemoryPoolBase(uint32_t entry_size);
	~MemoryPoolBase();

	MemoryPoolBase(const MemoryPoolBase &) = delete;
	MemoryPoolBase & operator=(const MemoryPoolBase &) = delete;

	void * allocate();
	void deallocate(void * ptr);
};

template <typename T>
class MemoryPool : MemoryPoolBase
{
public:
	MemoryPool() : MemoryPoolBase(sizeof(T)) {}

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
};

#endif // _BITS_MEMORY_POOL_H_

