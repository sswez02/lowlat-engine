#include <benchmark/benchmark.h>
#include <engine/operator.h>

#include <memory>
#include <vector>

// Direct call on the real NoOpOperator type.
// This is the baseline before measuring virtual dispatch.
static void BM_DirectCall(benchmark::State &state) {
    engine::NoOpOperator op;
    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        op.process(input);
        benchmark::DoNotOptimize(op.last_value());
        ++input;
    }
}
BENCHMARK(BM_DirectCall);

// Same operation, but called through the base Operator pointer.
// This is the virtual dispatch path we want to measure.
static void BM_VirtualDispatch(benchmark::State &state) {
    std::unique_ptr<engine::Operator> op = std::make_unique<engine::NoOpOperator>();
    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        op->process(input);
        benchmark::DoNotOptimize(op.get());
        ++input;
    }
}
BENCHMARK(BM_VirtualDispatch);

// Switches between two concrete operator types.
// This prevents the compiler from proving one fixed dynamic type.
static void BM_VirtualDispatchPolymorphic(benchmark::State &state) {
    std::vector<std::unique_ptr<engine::Operator>> ops;
    ops.push_back(std::make_unique<engine::NoOpOperator>());
    ops.push_back(std::make_unique<engine::IncrementOperator>());

    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        ops[input & 1]->process(input);
        benchmark::DoNotOptimize(ops.data());
        ++input;
    }
}
BENCHMARK(BM_VirtualDispatchPolymorphic);
