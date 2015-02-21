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

template <size_t N>
class SizedPoolAlloc
{
    typedef Sized<N> value_type;
    FixedMemoryPool<value_type> pool;
    std::vector<value_type *> ptr;
public:
    SizedPoolAlloc(size_t count) :
        pool(count),
        ptr(count, nullptr) {}
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
    SizedHeapAlloc(size_t count) :
        ptr(count, nullptr) {}
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
#define _CONFIG_BENCH_TEST(type, prefix, run, size, entries) \
    static type<size> g_ ## prefix ## _ ## size ## x ## entries ## run ## _(entries); \
    BENCH_TEST_BYTES(prefix ## _ ## run ## _ ## size ## x ## entries, size * entries) { \
        run(g_ ## prefix ## _ ## size ## x ## entries ## run ## _); \
    }

#define POOL_BENCH_TEST(run, size, entries) \
    _CONFIG_BENCH_TEST(SizedPoolAlloc, pool, run, size, entries)
#define HEAP_BENCH_TEST(run, size, entries) \
    _CONFIG_BENCH_TEST(SizedHeapAlloc, heap, run, size, entries)

POOL_BENCH_TEST(alloc_free, 16, 1000);
HEAP_BENCH_TEST(alloc_free, 16, 1000);
POOL_BENCH_TEST(alloc_free, 128, 1000);
HEAP_BENCH_TEST(alloc_free, 128, 1000);
POOL_BENCH_TEST(alloc_free, 512, 1000);
HEAP_BENCH_TEST(alloc_free, 512, 1000);

POOL_BENCH_TEST(alloc_memset_free, 16, 1000);
HEAP_BENCH_TEST(alloc_memset_free, 16, 1000);
POOL_BENCH_TEST(alloc_memset_free, 128, 1000);
HEAP_BENCH_TEST(alloc_memset_free, 128, 1000);
POOL_BENCH_TEST(alloc_memset_free, 512, 1000);
HEAP_BENCH_TEST(alloc_memset_free, 512, 1000);
