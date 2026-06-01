#include <benchmark/benchmark.h>
#include <vector>

static void BM_TrivialIncrement(benchmark::State &state) {
    int x = 0;
    for ([[maybe_unused]] auto _ : state) {
        benchmark::DoNotOptimize(++x);
    }
}
BENCHMARK(BM_TrivialIncrement);

static void BM_VectorWrite(benchmark::State &state) {
    std::vector<int> v(1024);
    for ([[maybe_unused]] auto _ : state) {
        for (std::size_t i = 0; i < 1024; ++i) {
            v[i] = static_cast<int>(i);
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorWrite);
