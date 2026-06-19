#include <engine/events.h>
#include <engine/operators/vwap.h>
#include <engine/pipeline.h>

#include <gtest/gtest.h>

#include <memory>
#include <utility>

TEST(Vwap, VWAPStartsAtZero) {
    engine::VWAPOperator vwapop;

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 0.0);
}

TEST(Vwap, VWAPSingleTickEvent) {
    engine::VWAPOperator vwapop;

    vwapop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    });

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 100.0);
}

TEST(Vwap, VWAPTwoTickEvents) {
    engine::VWAPOperator vwapop;

    vwapop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    });

    vwapop.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 200,
        .volume = 10,
    });

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 150.0);
}

TEST(Vwap, VWAPWeightedByVolume) {
    engine::VWAPOperator vwapop;

    vwapop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 1,
    });

    vwapop.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 200,
        .volume = 99,
    });

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 199.0);
}

TEST(Vwap, VWAPIgnoresNonTickEvents) {
    engine::VWAPOperator vwapop;

    vwapop.process(engine::AudioSample{
        .timestamp = 1U,
        .amplitude = 7.0F,
    });

    vwapop.process(engine::IMUSample{
        .timestamp = 2U,
        .acceleration = 9.5F,
    });

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 0.0);
}

TEST(Vwap, VWAPIgnoresZeroAndNegativeVolume) {
    engine::VWAPOperator vwapop;

    vwapop.process(engine::TickEvent{.price = 100, .volume = 0});
    vwapop.process(engine::TickEvent{.price = 100, .volume = -5});

    EXPECT_DOUBLE_EQ(vwapop.vwap(), 0.0);
}

TEST(Vwap, VWAPThroughPipeline) {
    engine::Pipeline pipeline;

    auto vwapop = std::make_unique<engine::VWAPOperator>();
    auto *vwap_ptr = vwapop.get();

    pipeline.add(std::move(vwapop));

    pipeline.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    });

    pipeline.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 200,
        .volume = 10,
    });

    EXPECT_DOUBLE_EQ(vwap_ptr->vwap(), 150.0);
}
