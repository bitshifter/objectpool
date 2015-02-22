#define BENCH_CONFIG_MAIN  // This tells Bench to provide a main() - only do this in one cpp file
#include "bench.hpp"
#include "memory_pool.hpp"

template <typename T>
void alloc_free(T& pool)
{
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.alloc(i);
    }
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.free(i);
    }
}

template <typename T>
void alloc_memset_free(T& pool)
{
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.alloc(i);
    }
    for (size_t i =0; i < 128; ++i)
    {
        pool.memset(i);
    }
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.free(i);
    }
}

template <size_t N>
struct Sized { char c[N]; };

template <typename PoolT>
class SizedPoolAlloc
{
    typedef typename PoolT::value_type value_type;
    PoolT pool;
    std::vector<value_type *> ptr;
public:
    SizedPoolAlloc(size_t block_size, size_t allocs) :
        pool(block_size),
        ptr(allocs, nullptr) {}
    void alloc(size_t i) {
        ptr[i] = pool.new_object();
    }
    void free(size_t i)
    {
        pool.delete_object(ptr[i]);
        ptr[i] = nullptr;
    }
    void memset(int value)
    {
        const size_t value_size = sizeof(value_type);
        pool.for_each([value, value_size](value_type * ptr) {
                ::memset(ptr, value, value_size);
                });
    }
    size_t count() const
    {
        return ptr.size();
    }
};

#define BENCH_HEAP_ALLOC
#ifdef BENCH_HEAP_ALLOC
template <size_t N>
class SizedHeapAlloc
{
    typedef Sized<N> value_type;
    std::vector<value_type *> ptr;
public:
    SizedHeapAlloc(size_t /*block_size*/, size_t allocs) :
        ptr(allocs, nullptr) {}
    void alloc(size_t i) {
        ptr[i] = new value_type;
    }
    void free(size_t i)
    {
        delete ptr[i];
        ptr[i] = nullptr;
    }
    size_t count() const
    {
        return ptr.size();
    }
    void memset(int value)
    {
        const size_t value_size = sizeof(value_type);
        for (auto p : ptr)
        {
            ::memset(p, value, value_size);
        }
    }
};
#endif // BENCH_HEAP_ALLOC

// I am a bad person. Bad and lazy.
#define _CONFIG_BENCH_TEST(type, prefix, run, value_size, block_size, allocs) \
    static type g_ ## prefix ## _ ## value_size ## x ## block_size ## x ## allocs ## run ## _(block_size, allocs); \
    BENCH_TEST_BYTES(prefix ## _ ## run ## _ ## value_size ## x ## block_size ## x ## allocs, allocs * value_size) { \
        run(g_ ## prefix ## _ ## value_size ## x ## block_size ## x ## allocs ## run ## _); \
    }

#define FIXED_POOL_BENCH_TEST(run, value_size, block_size) \
    _CONFIG_BENCH_TEST(SizedPoolAlloc<FixedMemoryPool<Sized<value_size>>>, fixed_pool, run, value_size, block_size, block_size)
#define DYNAMIC_POOL_BENCH_TEST(run, value_size, block_size, allocs) \
    _CONFIG_BENCH_TEST(SizedPoolAlloc<DynamicMemoryPool<Sized<value_size>>>, dynamic_pool, run, value_size, block_size, allocs)
#define HEAP_BENCH_TEST(run, value_size, block_size) \
    _CONFIG_BENCH_TEST(SizedHeapAlloc<value_size>, heap, run, value_size, block_size, block_size)

FIXED_POOL_BENCH_TEST(alloc_free, 16, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 16, 1000)

FIXED_POOL_BENCH_TEST(alloc_free, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 128, 1000)

FIXED_POOL_BENCH_TEST(alloc_free, 512, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 512, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 16, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 16, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 128, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 512, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 512, 1000)
