#include <algorithm>
#include <cassert>
#include <cstring>
#include <chrono>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <memory>

namespace bench
{

typedef void (*BenchFuncPtr)();

struct SourceLineInfo
{

    SourceLineInfo(const char* file, std::size_t line) : file_(file), line_(line) {}
    bool empty() const;
    bool operator==(SourceLineInfo const& other) const;

    const char* file_;
    const std::size_t line_;
};

struct BenchRunDesc
{
    BenchRunDesc(const char* desc, std::size_t bytes = 0) : desc_(desc), bytes_(bytes) {}

    const char* desc_;
    std::size_t bytes_;
};

struct BenchRun
{
    BenchFuncPtr func_;
    const char* desc_;
    std::size_t bytes_;
    SourceLineInfo line_info_;

    BenchRun(BenchFuncPtr function, const BenchRunDesc& bench_desc, const SourceLineInfo& line_info)
        : func_(function), desc_(bench_desc.desc_), bytes_(bench_desc.bytes_), line_info_(line_info)
    {
    }
};

struct AutoReg
{
    AutoReg(BenchFuncPtr function,
        const SourceLineInfo& line_info,
        const BenchRunDesc& name_and_desc);

    AutoReg(const AutoReg&) = delete;
    AutoReg& operator=(const AutoReg&) = delete;
};

} // namespace bench

#ifdef BENCH_CONFIG_MAIN
namespace stats
{

// Helper function: extract a value representing the `pct` percentile of a sorted sample-set, using
// linear interpolation. If samples are not sorted, return nonsensical value.
template <typename T>
T percentile_of_sorted(const std::vector<T>& sorted_samples, T pct)
{
    assert(!sorted_samples.empty());
    if (sorted_samples.size() == 1)
    {
        return sorted_samples[0];
    }
    assert(T(0) <= pct);
    const T hundred(100);
    assert(pct <= hundred);
    if (pct == hundred)
    {
        return sorted_samples[sorted_samples.size() - 1];
    }
    const auto length = (sorted_samples.size() - 1);
    const auto rank = (pct / hundred) * length;
    const auto lrank = std::floor(rank);
    const auto d = rank - lrank;
    const auto n = static_cast<size_t>(lrank);
    const auto lo = sorted_samples[n];
    const auto hi = sorted_samples[n + 1];
    return lo + (hi - lo) * d;
}

// Winsorize a set of sorted samples, replacing values above the `100-pct` percentile and below the
// `pct` percentile with those percentiles themselves. This is a way of minimizing the effect of
// outliers, at the cost of biasing the sample. It differs from trimming in that it does not
// change the number of samples, just changes the values of those that are outliers.
//
// See: http://en.wikipedia.org/wiki/Winsorising
template <typename T>
void winsorize_sorted(std::vector<T>& sorted_samples, T pct)
{
    auto lo = percentile_of_sorted(sorted_samples, pct);
    auto hi = percentile_of_sorted(sorted_samples, 100 - pct);
    for (auto& samp : sorted_samples)
    {
        if (samp > hi)
        {
            samp = hi;
        }
        else if (samp < lo)
        {
            samp = lo;
        }
    }
}

template <typename T>
struct Summary
{
    size_t num_samples;
    size_t iter_per_sample;
    T max;
    T min;
    T median;
    T median_abs_dev;
    T median_abs_dev_pct;

    Summary()
        : num_samples(0),
          iter_per_sample(0),
          max(0),
          min(0),
          median(0),
          median_abs_dev(0),
          median_abs_dev_pct(0)
    {
    }
};

template <typename T>
Summary<T> calculate_summary(const std::vector<T> samples, uint64_t iter_per_sample)
{
    auto summ = Summary<T>();
    std::vector<T> sorted_samples(samples);
    std::sort(sorted_samples.begin(), sorted_samples.end());
    winsorize_sorted(sorted_samples, 5.0);
    summ.num_samples = sorted_samples.size();
    summ.iter_per_sample = iter_per_sample;
    summ.min = sorted_samples.front();
    summ.max = sorted_samples.back();
    summ.median = percentile_of_sorted(sorted_samples, T(50));
    std::vector<T> abs_devs(sorted_samples);
    for (auto& v : abs_devs)
    {
        v = summ.median - v;
    }
    summ.median_abs_dev = percentile_of_sorted(abs_devs, T(50)) * T(1.4826);
    summ.median_abs_dev_pct = T(100) * summ.median_abs_dev / summ.median;
    return summ;
}

} // namespace stats

namespace bench
{

std::chrono::nanoseconds bench_n(uint64_t iterations, BenchFuncPtr func)
{
    auto loop_start = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < iterations; ++i)
    {
        func();
    }
    return (std::chrono::high_resolution_clock::now() - loop_start) / iterations;
}

stats::Summary<double> auto_bench(BenchFuncPtr func)
{
    // initial bench run to get ballpark figure.
    uint64_t n = 1;
    const auto ballpark = bench_n(1, func);

    // try to estimate iteration count for 1ms falling back to 1 million
    // iterations if first run too < 1ns.
    if (ballpark == std::chrono::nanoseconds::zero())
    {
        n = 1000000;
    }
    else
    {
        n = 1000000 / std::max<uint64_t>(ballpark.count(), 1);
    }

    // if the first run took more than 1ms we don't want to just
    // be left doing 0 iterations on every loop. The unfortunate
    // side effect of not being able to do as many runs is
    // automatically handled by the statistical analysis below
    // (i.e. larger error bars).
    if (n == 0)
    {
        n = 1;
    }

    std::chrono::nanoseconds total_run;
    std::vector<double> samples(50);
    std::vector<double> samples5(50);

    for (;;)
    {
        auto loop_start = std::chrono::high_resolution_clock::now();
        for (auto& p : samples)
        {
            auto ns_per_iter = bench_n(n, func);
            p = static_cast<double>(ns_per_iter.count());
        }

        for (auto& p : samples5)
        {
            auto ns_per_iter = bench_n(n * 5, func);
            p = static_cast<double>(ns_per_iter.count());
        }

        // If we've run for 100ms and seem to have converged to a
        // stable median.
        auto loop_run = std::chrono::high_resolution_clock::now() - loop_start;

        const auto summ = stats::calculate_summary<double>(samples, n);
        const auto summ5 = stats::calculate_summary<double>(samples5, n * 5);

        if (loop_run > std::chrono::milliseconds(100) && summ.median_abs_dev_pct < 1.0
            && summ.median - summ5.median < summ5.median_abs_dev)
        {
            return summ5;
        }

        total_run = total_run + loop_run;

        // Longest we ever run for is 3s.
        if (total_run > std::chrono::seconds(3))
        {
            return summ5;
        }

        n *= 2;
    }
}

struct BenchSamples
{
    stats::Summary<double> ns_iter_summ;
    uint64_t mb_s = 0;
};

#if _MSC_VER
#define snprintf _snprintf_s
#endif

void fmt_bench_samples(const BenchSamples& bs, char* buffer, size_t size)
{
    if (bs.mb_s)
    {
        snprintf(buffer,
            size,
            "%9" PRId64 " ns/iter (%4" PRIu64 " iter +/- %5" PRId64 ") = %4" PRIu64 " MB/s",
            static_cast<int64_t>(bs.ns_iter_summ.median),
            static_cast<uint64_t>(bs.ns_iter_summ.iter_per_sample * bs.ns_iter_summ.num_samples),
            static_cast<int64_t>(bs.ns_iter_summ.max - bs.ns_iter_summ.min),
            bs.mb_s);
    }
    else
    {
        snprintf(buffer,
            size,
            "%9" PRId64 " ns/iter (%4" PRIu64 " iter +/- %5" PRId64 ")",
            static_cast<int64_t>(bs.ns_iter_summ.median),
            static_cast<uint64_t>(bs.ns_iter_summ.iter_per_sample * bs.ns_iter_summ.num_samples),
            static_cast<int64_t>(bs.ns_iter_summ.max - bs.ns_iter_summ.min));
    }
}

#if _MSC_VER
#undef snprintf
#endif


BenchSamples benchmark(BenchFuncPtr bench_func, uint64_t bench_bytes)
{
    BenchSamples bs;

    bs.ns_iter_summ = auto_bench(bench_func);

    auto ns_iter = static_cast<uint64_t>(std::max(bs.ns_iter_summ.median, 1.0));
    auto iter_s = 1000000000 / ns_iter;
    bs.mb_s = (bench_bytes * iter_s) / 1000000;

    return bs;
}


class RunRegistry
{
    std::vector<BenchRun> bench_runs_;

public:
    void register_bench_run(const BenchRun& test_case) { bench_runs_.emplace_back(test_case); }
    void run_all_console() const
    {
        int max_desc_len = 20;
        for (const auto& test : bench_runs_)
        {
            max_desc_len = std::max(static_cast<int>(strlen(test.desc_)), max_desc_len);
        }
        char buffer[1024] = {0};
        for (const auto& test : bench_runs_)
        {
            bench::BenchSamples bs = bench::benchmark(test.func_, test.bytes_);
            bench::fmt_bench_samples(bs, buffer, sizeof(buffer));
            printf("%-*s %s\n", max_desc_len, test.desc_, buffer);
        }
    }
};

namespace
{
    inline RunRegistry& get_registry()
    {
        static std::unique_ptr<RunRegistry> s_registry;
        if (!s_registry)
        {
            s_registry.reset(new RunRegistry());
        }
        return *s_registry.get();
    }
} // anonymous namespace

AutoReg::AutoReg(BenchFuncPtr function,
    const SourceLineInfo& line_info,
    const BenchRunDesc& bench_desc)
{
    get_registry().register_bench_run(BenchRun(function, bench_desc, line_info));
}

} // namespace bench
#endif

#define INTERNAL_BENCH_LINEINFO                                                                    \
    ::bench::SourceLineInfo(__FILE__, static_cast<std::size_t>(__LINE__))

#define INTERNAL_BENCH_UNIQUE_NAME_LINE2(name, line) name##line
#define INTERNAL_BENCH_UNIQUE_NAME_LINE(name, line) INTERNAL_BENCH_UNIQUE_NAME_LINE2(name, line)
#define INTERNAL_BENCH_UNIQUE_NAME(name) INTERNAL_BENCH_UNIQUE_NAME_LINE(name, __LINE__)

#define INTERNAL_BENCH_RUN(...)                                                                    \
    static void INTERNAL_BENCH_UNIQUE_NAME(____B_E_N_C_H____T_E_S_T____)();                        \
    namespace                                                                                      \
    {                                                                                              \
        bench::AutoReg INTERNAL_BENCH_UNIQUE_NAME(autoRegistrar)(                                  \
            &INTERNAL_BENCH_UNIQUE_NAME(____B_E_N_C_H____T_E_S_T____),                             \
            INTERNAL_BENCH_LINEINFO,                                                               \
            bench::BenchRunDesc(__VA_ARGS__));                                                     \
    }                                                                                              \
    static void INTERNAL_BENCH_UNIQUE_NAME(____B_E_N_C_H____T_E_S_T____)()

#define BENCH_TEST(...) INTERNAL_BENCH_RUN(__VA_ARGS__)

#ifdef BENCH_CONFIG_MAIN
int main(int /*argc*/, const char* /*argv*/ [])
{
    bench::get_registry().run_all_console();
    return 0;
}
#endif // BENCH_CONFIG_MAIN
