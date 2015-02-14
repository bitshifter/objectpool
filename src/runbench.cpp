#define BENCH_CONFIG_MAIN  // This tells Bench to provide a main() - only do this in one cpp file
#include "bench.hpp"

uint64_t factorial(uint64_t n)
{
    return n > 0 ? factorial(n - 1) * n : 1;
}

BENCH_TEST_BYTES(factorial_100)
{
    factorial(100);
}

BENCH_TEST(factorial_1000)
{
    factorial(1000);
}

