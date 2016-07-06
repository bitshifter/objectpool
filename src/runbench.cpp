#define BENCH_CONFIG_MAIN // This tells Bench to provide a main() - only do this in one cpp file
#include "bench.hpp"
#include "object_pool.hpp"

#ifdef BENCH_BOOST_POOL
#include <boost/pool/object_pool.hpp>
#endif

/// Test which allocates a number of objects then frees them all
template <typename T>
void alloc_free(T& pool)
{
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.new_index(i);
    }
    pool.delete_all();
}

/// Test which allocates a number of objects, memsets their contents a number
/// of times then deletes all of the objects again.
template <typename T>
void alloc_memset_free(T& pool)
{
    for (size_t i = 0; i < pool.count(); i++)
    {
        pool.new_index(i);
    }

    const size_t value_size = sizeof(typename T::value_t);
    for (int i = 0; i < 128; ++i)
    {
        pool.for_each([i, value_size](typename T::value_t* ptr)
            {
                ::memset(ptr, i, value_size);
            });
    }

    pool.delete_all();
}

/// A struct which is the size of the given template parameter
template <size_t N>
struct Sized
{
    char c[N];
};

/// Test harness for running benchmark tests using the pool allocator.
/// Maintains a std::vector of pointers to match behaviour of the default
/// allocator implementation.
template <typename PoolT>
class SizedPoolAlloc
{
public:
    typedef typename PoolT::value_t value_t;

    SizedPoolAlloc(size_t block_size, size_t allocs)
        : pool(static_cast<typename PoolT::index_t>(block_size)), ptr(allocs, nullptr)
    {
    }
    void new_index(size_t i) { ptr[i] = pool.new_object(); }
    void delete_all()
    {
        // delete individual objects for fair comparison
        pool.delete_all();
        //for (auto & p : ptr)
        //{
        //    pool.delete_object(p);
        //    p = nullptr;
        //}
    }
    template <typename F>
    void for_each(const F func) const
    {
        pool.for_each(func);
    }
    void memset(int value)
    {
        const size_t value_size = sizeof(value_t);
        pool.for_each([value, value_size](value_t* ptr)
            {
                ::memset(ptr, value, value_size);
            });
    }
    size_t count() const { return ptr.size(); }

private:
    PoolT pool;
    std::vector<value_t*> ptr;
};

#define BENCH_HEAP_ALLOC
#ifdef BENCH_HEAP_ALLOC
/// Test harness for running benchmark tests using the default system allocator.
template <size_t N>
class SizedHeapAlloc
{
public:
    typedef Sized<N> value_t;

    SizedHeapAlloc(size_t /*block_size*/, size_t allocs) : ptr(allocs, nullptr) {}
    void new_index(size_t i) { ptr[i] = new value_t; }
    void delete_all()
    {
        for (auto & p : ptr)
        {
            delete p;
         //   p = nullptr;
        }
    }
    template <typename F>
    void for_each(const F func) const
    {
        for (auto p : ptr)
        {
            func(p);
        }
    }
    size_t count() const { return ptr.size(); }
    void memset(int value)
    {
        const size_t value_size = sizeof(value_t);
        for (auto p : ptr)
        {
            ::memset(p, value, value_size);
        }
    }

private:
    std::vector<value_t*> ptr;
};
#endif // BENCH_HEAP_ALLOC

#ifdef BENCH_BOOST_POOL
template <size_t N>
class SizedBoostAlloc
{
public:
    typedef Sized<N> value_t;
    typedef boost::object_pool<value_t> PoolT;

    SizedBoostAlloc(size_t block_size, size_t allocs)
        : pool(new PoolT(block_size, allocs)), ptr(allocs, nullptr), block_size(block_size), allocs(allocs)
    {
    }
    void new_index(size_t i) { ptr[i] = pool->construct(); }
    void delete_all()
    {
        // boost pool cleans up all objects on destruction
        pool.reset(new PoolT(block_size, allocs));
        //for (auto & p : ptr)
        //{
        //    pool.destroy(p);
        //    p = nullptr;
        //}
    }
    template <typename F>
    void for_each(const F func) const
    {
        for (auto p : ptr)
        {
            func(p);
        }
    }
    size_t count() const { return ptr.size(); }
    void memset(int value)
    {
        const size_t value_size = sizeof(value_t);
        for (auto p : ptr)
        {
            ::memset(p, value, value_size);
        }
    }

private:
    std::unique_ptr<PoolT> pool;
    std::vector<value_t*> ptr;
    size_t block_size;
    size_t allocs;
};
#endif // BENCH_BOOST_POOL

// I am a bad person. Bad and lazy.

#define STRINGIFY2(expr) #expr
#define STRINGIFY(expr) STRINGIFY2(expr)

#define _CONFIG_BENCH_TEST(type, prefix, run, value_size, block_size, allocs)                      \
    static type g_##prefix##_##value_size##x##block_size##x##allocs##run##_(block_size, allocs);   \
    BENCH_TEST(                                                                                    \
        STRINGIFY(prefix##_##run##_##value_size##x##block_size##x##allocs), allocs* value_size)    \
    {                                                                                              \
        run(g_##prefix##_##value_size##x##block_size##x##allocs##run##_);                          \
    }


#define FIXED_POOL_BENCH_TEST(run, value_size, block_size)                                         \
    _CONFIG_BENCH_TEST(SizedPoolAlloc<FixedObjectPool<Sized<value_size>>>,                         \
        fixed_pool,                                                                                \
        run,                                                                                       \
        value_size,                                                                                \
        block_size,                                                                                \
        block_size)
#define DYNAMIC_POOL_BENCH_TEST(run, value_size, block_size, allocs)                               \
    _CONFIG_BENCH_TEST(SizedPoolAlloc<DynamicObjectPool<Sized<value_size>>>,                       \
        dynamic_pool,                                                                              \
        run,                                                                                       \
        value_size,                                                                                \
        block_size,                                                                                \
        allocs)
#define HEAP_BENCH_TEST(run, value_size, block_size)                                               \
    _CONFIG_BENCH_TEST(SizedHeapAlloc<value_size>, heap, run, value_size, block_size, block_size)
#if BENCH_BOOST_POOL
#define BOOST_BENCH_TEST(run, value_size, block_size)                                              \
    _CONFIG_BENCH_TEST(SizedBoostAlloc<value_size>, boost_pool, run, value_size, block_size, block_size)
#else
#define BOOST_BENCH_TEST(run, value_size, block_size)
#endif

FIXED_POOL_BENCH_TEST(alloc_free, 16, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 16, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 16, 1000)
BOOST_BENCH_TEST(alloc_free, 16, 1000)

FIXED_POOL_BENCH_TEST(alloc_free, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 128, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 128, 1000)
BOOST_BENCH_TEST(alloc_free, 128, 1000)

FIXED_POOL_BENCH_TEST(alloc_free, 512, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_free, 512, 256, 1000)
HEAP_BENCH_TEST(alloc_free, 512, 1000)
BOOST_BENCH_TEST(alloc_free, 512, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 16, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 16, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 16, 1000)
BOOST_BENCH_TEST(alloc_memset_free, 16, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 128, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 128, 1000)
BOOST_BENCH_TEST(alloc_memset_free, 128, 1000)

FIXED_POOL_BENCH_TEST(alloc_memset_free, 512, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 64, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 128, 1000)
DYNAMIC_POOL_BENCH_TEST(alloc_memset_free, 512, 256, 1000)
HEAP_BENCH_TEST(alloc_memset_free, 512, 1000)
BOOST_BENCH_TEST(alloc_memset_free, 512, 1000)
