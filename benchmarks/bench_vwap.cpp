#include <benchmark/benchmark.h>

#include <engine/events.h>
#include <engine/operators/vwap.h>
#include <engine/pipeline.h>

#include <memory>
#include <utility>

static void BM_VWAPDirectCall(benchmark::State &state) {
    engine::VWAPOperator vwap;

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    };

    for ([[maybe_unused]] auto _ : state) {
        vwap.process(event);
        benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(vwap.vwap());
}
BENCHMARK(BM_VWAPDirectCall);

static void BM_PipelineWithVWAP(benchmark::State &state) {
    engine::Pipeline pipeline;

    auto vwap = std::make_unique<engine::VWAPOperator>();
    auto *vwap_ptr = vwap.get();

    pipeline.add(std::move(vwap));

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    };

    for ([[maybe_unused]] auto _ : state) {
        pipeline.process(event);
        benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(vwap_ptr->vwap());
}
BENCHMARK(BM_PipelineWithVWAP);
