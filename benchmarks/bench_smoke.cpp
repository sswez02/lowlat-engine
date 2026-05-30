#include <benchmark/benchmark.h>
#include <vector>

static void BM_TrivialIncrement(benchmark::State &state) {
    int x = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(++x);
    }
}
BENCHMARK(BM_TrivialIncrement);

static void BM_VectorWrite(benchmark::State &state) {
    std::vector<int> v(1024);
    for (auto _ : state) {
        for (int i = 0; i < 1024; ++i) {
            v[i] = i;
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorWrite);
