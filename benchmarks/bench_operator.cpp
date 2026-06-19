#include <benchmark/benchmark.h>
#include <engine/events.h>
#include <engine/operator.h>

#include <array>
#include <memory>
#include <vector>

// Direct call on the real NoOpOperator type.
// Uses a fixed TickEvent so this remains the baseline.
static void BM_DirectCall(benchmark::State &state) {
    engine::NoOpOperator op;

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    for ([[maybe_unused]] auto _ : state) {
        op.process(event);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_DirectCall);

// Same operation, but called through the base Operator pointer.
// This is the single-concrete-type virtual-looking path.
static void BM_VirtualDispatch(benchmark::State &state) {
    std::unique_ptr<engine::Operator> op = std::make_unique<engine::NoOpOperator>();

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    for ([[maybe_unused]] auto _ : state) {
        op->process(event);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VirtualDispatch);

// Switches between two concrete operator types.
// This prevents the compiler from proving one fixed dynamic type.
static void BM_VirtualDispatchPolymorphic(benchmark::State &state) {
    std::vector<std::unique_ptr<engine::Operator>> ops;

    ops.push_back(std::make_unique<engine::NoOpOperator>());
    ops.push_back(std::make_unique<engine::IncrementOperator>());

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        ops[input & 1]->process(event);
        benchmark::ClobberMemory();
        ++input;
    }
}
BENCHMARK(BM_VirtualDispatchPolymorphic);

// Adversarial version of the polymorphic virtual-dispatch benchmark.
// Raw base pointers are observed before the loop to make devirtualisation harder.
static void BM_VirtualDispatchPolymorphicAdversarial(benchmark::State &state) {
    std::vector<std::unique_ptr<engine::Operator>> ops;

    ops.push_back(std::make_unique<engine::NoOpOperator>());
    ops.push_back(std::make_unique<engine::IncrementOperator>());

    engine::Operator *op0 = ops[0].get();
    engine::Operator *op1 = ops[1].get();

    benchmark::DoNotOptimize(op0);
    benchmark::DoNotOptimize(op1);

    const engine::Event event = engine::TickEvent{
        .timestamp = 1,
        .price = 42,
    };

    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        engine::Operator *op = (input & 1) == 0 ? op0 : op1;
        op->process(event);
        benchmark::ClobberMemory();
        ++input;
    }
}
BENCHMARK(BM_VirtualDispatchPolymorphicAdversarial);

// Direct NoOpOperator call while varying the active Event alternative.
// This measures std::visit/variant dispatch effects without virtual dispatch.
static void BM_DirectCallVariantPolymorphic(benchmark::State &state) {
    engine::NoOpOperator op;

    const std::array<engine::Event, 4> events{
        engine::Event{},
        engine::TickEvent{
            .timestamp = 1,
            .price = 42,
        },
        engine::AudioSample{
            .timestamp = 2,
            .amplitude = 7.0F,
        },
        engine::IMUSample{
            .timestamp = 3,
            .acceleration = 9.5F,
        },
    };

    int input = 0;
    for ([[maybe_unused]] auto _ : state) {
        op.process(events[input & 3]);
        benchmark::ClobberMemory();
        ++input;
    }
}
BENCHMARK(BM_DirectCallVariantPolymorphic);
