#include <benchmark/benchmark.h>

#include <engine/operator.h>
#include <engine/operators/vwap.h>
#include <engine/pipeline.h>
#include <engine/sources/itch.h>

#include <cstdlib>
#include <exception>
#include <memory>

static const char *itch_file_path(benchmark::State &state) {
    const char *path = std::getenv("LOWLAT_ITCH_FILE");

    if (path == nullptr) {
        state.SkipWithError("LOWLAT_ITCH_FILE env var not set");
        return nullptr;
    }

    return path;
}

static void BM_ITCHFileSource_NoOp(benchmark::State &state) {
    const char *path = itch_file_path(state);
    if (path == nullptr) {
        return;
    }

    engine::Pipeline pipeline;

    auto noop = std::make_unique<engine::NoOpOperator>();
    pipeline.add(std::move(noop));

    engine::ITCHFileSource source{path};

    for ([[maybe_unused]] auto _ : state) {
        try {
            source.run(pipeline);
            state.SetItemsProcessed(static_cast<int64_t>(source.messages_processed()));
        } catch (const std::exception &e) {
            state.SkipWithError(e.what());
            return;
        }
    }
}

BENCHMARK(BM_ITCHFileSource_NoOp)->Iterations(1)->Unit(benchmark::kMillisecond);

static void BM_ITCHFileSource_VWAP(benchmark::State &state) {
    const char *path = itch_file_path(state);
    if (path == nullptr) {
        return;
    }

    engine::Pipeline pipeline;

    auto vwap = std::make_unique<engine::VWAPOperator>();
    pipeline.add(std::move(vwap));

    engine::ITCHFileSource source{path};

    for ([[maybe_unused]] auto _ : state) {
        try {
            source.run(pipeline);
            state.SetItemsProcessed(static_cast<int64_t>(source.messages_processed()));
        } catch (const std::exception &e) {
            state.SkipWithError(e.what());
            return;
        }
    }
}

BENCHMARK(BM_ITCHFileSource_VWAP)->Iterations(1)->Unit(benchmark::kMillisecond);
