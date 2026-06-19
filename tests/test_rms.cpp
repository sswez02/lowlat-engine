#include <engine/operators/rms.h>
#include <engine/pipeline.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>

TEST(Rms, RMSStartsAtZero) {
    engine::RMSOperator rmsop(4);

    EXPECT_DOUBLE_EQ(rmsop.rms(), 0.0);
}

TEST(Rms, RMSSingleSample) {
    engine::RMSOperator rmsop(4);

    rmsop.process(engine::AudioSample{
        .timestamp = 1U,
        .amplitude = 0.5F,
    });

    EXPECT_DOUBLE_EQ(rmsop.rms(), 0.5);
}

TEST(Rms, RMSWindowFill) {
    engine::RMSOperator rmsop(4);

    rmsop.process(engine::AudioSample{.timestamp = 1U, .amplitude = 1.0F});
    rmsop.process(engine::AudioSample{.timestamp = 2U, .amplitude = 2.0F});
    rmsop.process(engine::AudioSample{.timestamp = 3U, .amplitude = 3.0F});
    rmsop.process(engine::AudioSample{.timestamp = 4U, .amplitude = 4.0F});

    EXPECT_NEAR(rmsop.rms(), std::sqrt(30.0 / 4.0), 1e-9);
}

TEST(Rms, RMSRingBufferRotates) {
    engine::RMSOperator rmsop(4);

    rmsop.process(engine::AudioSample{.timestamp = 1U, .amplitude = 1.0F});
    rmsop.process(engine::AudioSample{.timestamp = 2U, .amplitude = 2.0F});
    rmsop.process(engine::AudioSample{.timestamp = 3U, .amplitude = 3.0F});
    rmsop.process(engine::AudioSample{.timestamp = 4U, .amplitude = 4.0F});
    rmsop.process(engine::AudioSample{.timestamp = 5U, .amplitude = 5.0F});

    // After 5th sample, oldest 1.0 drops:
    // sqrt((2^2 + 3^2 + 4^2 + 5^2) / 4) = sqrt(54 / 4)
    EXPECT_NEAR(rmsop.rms(), std::sqrt(54.0 / 4.0), 1e-9);
}

TEST(Rms, RMSIgnoresNonAudioEvents) {
    engine::RMSOperator rmsop(4);

    rmsop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    });

    rmsop.process(engine::IMUSample{
        .timestamp = 2U,
        .acceleration = 9.5F,
    });

    EXPECT_DOUBLE_EQ(rmsop.rms(), 0.0);
}

TEST(Rms, RMSThroughPipeline) {
    engine::Pipeline pipeline;

    auto rmsop = std::make_unique<engine::RMSOperator>(4);
    auto *rms_ptr = rmsop.get();

    pipeline.add(std::move(rmsop));

    pipeline.process(engine::AudioSample{.timestamp = 1U, .amplitude = 1.0F});
    pipeline.process(engine::AudioSample{.timestamp = 2U, .amplitude = 2.0F});

    EXPECT_NEAR(rms_ptr->rms(), std::sqrt(5.0 / 2.0), 1e-9);
}
