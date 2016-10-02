#define NONIUS_RUNNER
#include "nonius.hpp"

#include "object_pool.hpp"

#ifdef BENCH_BOOST_POOL
#include <boost/pool/object_pool.hpp>
#endif

// I'm using a lot of templates to minimise copy paste for different benchmarking configurations.

namespace 
{

/// Test which allocates a number of objects then frees them all
struct BenchAllocFree
{
    const char* name() const { return "alloc+free"; }
    template <typename HarnessT>
    size_t run(HarnessT& harness) const
    {
        for (size_t i = 0; i < harness.count(); i++)
        {
            harness.new_index(i);
        }
        harness.delete_all();

        return harness.count();
    }
};

/// Test which allocates a number of objects, memsets their contents a number
/// of times then deletes all of the objects again.
struct BenchAllocMemsetFree
{
    const char* name() const { return "alloc+memset+free"; }
    template <typename HarnessT>
    size_t run(HarnessT& harness) const
    {
        for (size_t i = 0; i < harness.count(); i++)
        {
            harness.new_index(i);
        }

        const size_t value_size = sizeof(typename HarnessT::value_t);
        for (int i = 0; i < 1024; ++i)
        {
            harness.for_each([i, value_size](typename HarnessT::value_t* ptr)
                {
                    ::memset(ptr, i, value_size);
                });
        }

        harness.delete_all();

        return harness.count();
    }
};

/// A struct which is the size of the given template parameter
template <size_t N>
struct Sized
{
    char c[N];
};

/// Test harness for running benchmark tests using the object pool allocator.
/// Maintains a std::vector of pointers to match behaviour of the default
/// allocator implementation.
template <typename PoolT>
class ObjectPoolHarness
{
public:
    typedef typename PoolT::value_t value_t;

    ObjectPoolHarness(size_t block_size, size_t allocs)
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
template <typename T>
class HeapAllocHarness
{
public:
    typedef T value_t;

    HeapAllocHarness(size_t /*block_size*/, size_t allocs) : ptr(allocs, nullptr) {}
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
// Test harness for running benchmark tests using boost::object_pool.
template <typename T>
class BoostPoolHarness
{
public:
    typedef T value_t;
    typedef boost::object_pool<value_t> PoolT;

    BoostPoolHarness(size_t block_size, size_t allocs)
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

// templated function for running permuations of test and object sizes
template <size_t Size, typename Test>
void run_for_size(nonius::benchmark_registry& registry, size_t num_allocs)
{
    typedef Sized<Size> SizedN;
    static const size_t label_size = 1024;
    char label[1024] = {};
    const Test bench_test;

    // FixedObjectPool alloc+free bench
    {
        const auto block_size = num_allocs;
        snprintf(label, label_size, "FixedObjectPool<Sized<%zu>> %s", Size, bench_test.name());
        registry.emplace_back(label,
                [&bench_test, block_size, num_allocs](nonius::chronometer meter) {
                ObjectPoolHarness<FixedObjectPool<SizedN>> pool(block_size, num_allocs);
                meter.measure([&bench_test, &pool]{
                        return bench_test.run(pool);
                        });
                });
    }

    // DynamicObjectPool alloc+free benches
    {
        static const size_t block_sizes[3] = {64, 128, 256};
        for (auto block_size : block_sizes)
        {
            snprintf(label, label_size, "DynamicObjectPool<Sized<%zu>> %zu byte blocks %s", Size, block_size, bench_test.name());
            registry.emplace_back(label,
                    [&bench_test, block_size, num_allocs](nonius::chronometer meter) {
                    ObjectPoolHarness<DynamicObjectPool<SizedN>> pool(block_size, num_allocs);
                    meter.measure([&bench_test, &pool]{
                            return bench_test.run(pool);
                            });
                    });
        }
    }

#ifdef BENCH_BOOST_POOL
    // BoostPoolHarness<SizedN> alloc+free bench
    {
        const auto block_size = num_allocs;
        snprintf(label, label_size, "BoostPoolHarness<Sized<%zu>> %s", Size, bench_test.name());
        registry.emplace_back(label,
                [&bench_test, block_size, num_allocs](nonius::chronometer meter) {
                BoostPoolHarness<SizedN> pool(block_size, num_allocs);
                meter.measure([&bench_test, &pool]{
                        return bench_test.run(pool);
                        });
                });
    }
#endif // BENCH_BOOST_POOL

#ifdef BENCH_HEAP_ALLOC
    // HeapAllocHarness<SizedN> alloc+free bench
    {
        const auto block_size = num_allocs;
        snprintf(label, label_size, "HeapAllocHarness<Sized<%zu>> %s", Size, bench_test.name());
        registry.emplace_back(label,
                [&bench_test, block_size, num_allocs](nonius::chronometer meter) {
                HeapAllocHarness<SizedN> pool(block_size, num_allocs);
                meter.measure([&bench_test, &pool]{
                        return bench_test.run(pool);
                        });
                });
    }
#endif // BENCH_HEAP_ALLOC
}

// Auto registers tests with Nonius on static constructon.
struct BenchmarkRegistrar
{
    BenchmarkRegistrar()
    {
        static const size_t num_allocs = 1000;
        auto& registry = nonius::global_benchmark_registry();

        // bench alloc+free
        run_for_size<16, BenchAllocFree>(registry, num_allocs);
        run_for_size<128, BenchAllocFree>(registry, num_allocs);
        run_for_size<512, BenchAllocFree>(registry, num_allocs);

        // bench alloc+memset+free
        run_for_size<16, BenchAllocMemsetFree>(registry, num_allocs);
        run_for_size<128, BenchAllocMemsetFree>(registry, num_allocs);
        run_for_size<512, BenchAllocMemsetFree>(registry, num_allocs);
    }
};
BenchmarkRegistrar g_benchmark_registrar;

} // anonymous namespace

