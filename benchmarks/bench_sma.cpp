#include <benchmark/benchmark.h>

#include <engine/events.h>
#include <engine/operators/sma.h>
#include <engine/operators/vwap.h>
#include <engine/pipeline.h>

#include <memory>
#include <utility>

// Note: Window size 16 is chosen because it is a strict power-of-two (required for the O(1) bitwise
// wrap) that closely approximates common trading moving average windows (e.g., 10 or 20 ticks).

static void BM_SMADirectCall(benchmark::State &state) {
    engine::SMAOperator sma(16);

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    };

    for ([[maybe_unused]] auto _ : state) {
        sma.process(event);
        benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(sma.sma());
}
BENCHMARK(BM_SMADirectCall);

static void BM_PipelineWithSMA(benchmark::State &state) {
    engine::Pipeline pipeline;

    auto sma = std::make_unique<engine::SMAOperator>(16);
    auto *sma_ptr = sma.get();

    pipeline.add(std::move(sma));

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    };

    for ([[maybe_unused]] auto _ : state) {
        pipeline.process(event);
        benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(sma_ptr->sma());
}
BENCHMARK(BM_PipelineWithSMA);

static void BM_PipelineWithSMAAndVWAP(benchmark::State &state) {
    engine::Pipeline pipeline;

    auto sma = std::make_unique<engine::SMAOperator>(16);
    auto *sma_ptr = sma.get();

    pipeline.add(std::move(sma));

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

    benchmark::DoNotOptimize(sma_ptr->sma());
    benchmark::DoNotOptimize(vwap_ptr->vwap());
}
BENCHMARK(BM_PipelineWithSMAAndVWAP);
