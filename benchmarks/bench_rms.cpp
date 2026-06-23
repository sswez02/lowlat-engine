#include <benchmark/benchmark.h>

#include <engine/events.h>
#include <engine/operators/rms.h>
#include <engine/pipeline.h>

#include <memory>
#include <utility>

// Window size 1024 is chosen because RMSOperator requires a power-of-two window,
// and 1024 gives a more realistic ring-buffer size than the small test window of 4.

static void BM_RMSDirectCall(benchmark::State &state) {
    engine::RMSOperator rms(1024);

    const engine::Event event = engine::AudioSample{
        .timestamp = 1U,
        .amplitude = 0.5F,
    };

    rms.process(event);
    if (rms.rms() <= 0.0) {
        state.SkipWithError("RMSOperator did not process AudioSample event");
        return;
    }

    for ([[maybe_unused]] auto _ : state) {
        rms.process(event);
        benchmark::DoNotOptimize(rms.rms());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_RMSDirectCall);

static void BM_PipelineWithRMS(benchmark::State &state) {
    engine::Pipeline pipeline;

    auto rms = std::make_unique<engine::RMSOperator>(1024);
    auto *rms_ptr = rms.get();

    pipeline.add(std::move(rms));

    const engine::Event event = engine::AudioSample{
        .timestamp = 1U,
        .amplitude = 0.5F,
    };

    pipeline.process(event);
    if (rms_ptr->rms() <= 0.0) {
        state.SkipWithError("Pipeline did not deliver AudioSample event to RMSOperator");
        return;
    }

    for ([[maybe_unused]] auto _ : state) {
        pipeline.process(event);
        benchmark::DoNotOptimize(rms_ptr->rms());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_PipelineWithRMS);

// TODO: BM_AudioFileSource_RMS on a real WAV file
