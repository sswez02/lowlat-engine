#include <benchmark/benchmark.h>
#include <engine/events.h>
#include <engine/operator.h>
#include <engine/pipeline.h>

#include <memory>

static void BM_PipelineThreeOpNoOp(benchmark::State &state) {
    engine::Pipeline pipeline;

    pipeline.add(std::make_unique<engine::NoOpOperator>());
    pipeline.add(std::make_unique<engine::NoOpOperator>());
    pipeline.add(std::make_unique<engine::NoOpOperator>());

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    for ([[maybe_unused]] auto _ : state) {
        pipeline.process(event);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_PipelineThreeOpNoOp);

static void BM_PipelineThreeOpMixed(benchmark::State &state) {
    engine::Pipeline pipeline;

    pipeline.add(std::make_unique<engine::NoOpOperator>());
    pipeline.add(std::make_unique<engine::IncrementOperator>());
    pipeline.add(std::make_unique<engine::NoOpOperator>());

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    for ([[maybe_unused]] auto _ : state) {
        pipeline.process(event);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_PipelineThreeOpMixed);
