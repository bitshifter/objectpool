#define BENCH_CONFIG_MAIN  // This tells Bench to provide a main() - only do this in one cpp file
#include "bench.hpp"
#include "memory_pool.hpp"

/*
template <typename T>
void run(T pool, const size_t rounds)
{
    for (size_t i = 0; i < pool.count(); i++)
        pool.alloc(i);

    for (size_t r = 1; r < rounds; r++) {
        for (size_t i = 0; i < pool.count(); i++) {
            pool.free(i);
            pool.alloc(i);
        }
    }

    for (size_t i = 0; i < pool.count(); i++) {
        pool.free(i);
    }
}
*/

template <typename T>
void run(T& pool)
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
    MemoryPool<value_type> pool;
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

BENCH_TEST_BYTES(pool_100x16_bytes, 100 * 16)
{
    SizedPoolAlloc<16> p(100);
    run(p);
}

#ifdef BENCH_HEAP_ALLOC
BENCH_TEST_BYTES(heap_100x16_bytes, 100 * 16)
{
    SizedHeapAlloc<16> p(100);
    run(p);
}
#endif

BENCH_TEST_BYTES(pool_100x128_bytes, 100 * 128)
{
    SizedPoolAlloc<128> p(100);
    run(p);
}

#ifdef BENCH_HEAP_ALLOC
BENCH_TEST_BYTES(heap_100x128_bytes, 100 * 128)
{
    SizedHeapAlloc<128> p(100);
    run(p);
}
#endif

BENCH_TEST_BYTES(pool_100x1024_bytes, 100 * 1024)
{
    SizedPoolAlloc<1024> p(100);
    run(p);
}

#ifdef BENCH_HEAP_ALLOC
BENCH_TEST_BYTES(heap_100x1024_bytes, 100 * 1024)
{
    SizedHeapAlloc<1024> p(100);
    run(p);
}
#endif

