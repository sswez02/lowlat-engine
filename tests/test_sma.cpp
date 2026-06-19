#include <engine/operator.h>
#include <engine/operators/sma.h>
#include <engine/pipeline.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

TEST(Sma, SMAStartsAtZero) {
    engine::SMAOperator smaop(4);

    EXPECT_DOUBLE_EQ(smaop.sma(), 0.0);
}

TEST(Sma, SMASingleTickEvent) {
    engine::SMAOperator smaop(4);

    smaop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 100,
        .volume = 10,
    });

    EXPECT_DOUBLE_EQ(smaop.sma(), 100.0);
}

TEST(Sma, SMAUnfullWindow) {
    engine::SMAOperator smaop(4);

    smaop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 10,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 20,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 3U,
        .price = 30,
        .volume = 1,
    });

    EXPECT_DOUBLE_EQ(smaop.sma(), 20.0);
}

TEST(Sma, SMAFullWindow) {
    engine::SMAOperator smaop(4);

    smaop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 10,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 20,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 3U,
        .price = 30,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 4U,
        .price = 40,
        .volume = 1,
    });

    EXPECT_DOUBLE_EQ(smaop.sma(), 25.0);
}

TEST(Sma, SMARingBufferRotates) {
    engine::SMAOperator smaop(4);

    smaop.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 10,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 2U,
        .price = 20,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 3U,
        .price = 30,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 4U,
        .price = 40,
        .volume = 1,
    });

    smaop.process(engine::TickEvent{
        .timestamp = 5U,
        .price = 50,
        .volume = 1,
    });

    // After 5th, oldest (10) drops
    //(20+30+40+50)/4 = 35.0
    EXPECT_DOUBLE_EQ(smaop.sma(), 35.0);
}

TEST(Sma, SMAIgnoresNonTickEvents) {
    engine::SMAOperator smaop(4);

    smaop.process(engine::AudioSample{
        .timestamp = 1U,
        .amplitude = 7.0F,
    });

    smaop.process(engine::IMUSample{
        .timestamp = 2U,
        .acceleration = 9.5F,
    });

    EXPECT_DOUBLE_EQ(smaop.sma(), 0.0);
}

TEST(Sma, SMAThroughPipeline) {
    engine::Pipeline pipeline;

    auto smaop = std::make_unique<engine::SMAOperator>(4);
    auto *smapop_ptr = smaop.get();

    pipeline.add(std::move(smaop));

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

    EXPECT_DOUBLE_EQ(smapop_ptr->sma(), 150.0);
}
