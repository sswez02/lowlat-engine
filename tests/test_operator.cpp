#include <engine/events.h>
#include <engine/operator.h>
#include <gtest/gtest.h>
#include <memory>
#include <variant>

// NoOpOperator stores the most recent Event alternative

TEST(Operator, NoOpStartsWithMonostate) {
    engine::NoOpOperator op;

    EXPECT_TRUE(std::holds_alternative<std::monostate>(op.last_event()));
}

TEST(Operator, NoOpStoresTickEvent) {
    engine::NoOpOperator op;

    op.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    EXPECT_TRUE(std::holds_alternative<engine::TickEvent>(op.last_event()));

    const auto *stored_tick = std::get_if<engine::TickEvent>(&op.last_event());
    ASSERT_NE(stored_tick, nullptr);
    EXPECT_EQ(stored_tick->timestamp, 1U);
    EXPECT_EQ(stored_tick->price, 42);
}

TEST(Operator, NoOpStoresAudioSample) {
    engine::NoOpOperator op;

    op.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7.0F,
    });

    EXPECT_TRUE(std::holds_alternative<engine::AudioSample>(op.last_event()));

    const auto *stored_audio = std::get_if<engine::AudioSample>(&op.last_event());
    ASSERT_NE(stored_audio, nullptr);
    EXPECT_EQ(stored_audio->timestamp, 2U);
    EXPECT_EQ(stored_audio->amplitude, 7.0F);
}

TEST(Operator, NoOpStoresIMUSample) {
    engine::NoOpOperator op;

    op.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    });

    EXPECT_TRUE(std::holds_alternative<engine::IMUSample>(op.last_event()));

    const auto *stored_imu = std::get_if<engine::IMUSample>(&op.last_event());
    ASSERT_NE(stored_imu, nullptr);
    EXPECT_EQ(stored_imu->timestamp, 3U);
    EXPECT_FLOAT_EQ(stored_imu->acceleration, 9.5F);
}

TEST(Operator, NoOpStoresMonostateEvent) {
    engine::NoOpOperator op;

    op.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(op.last_event()));

    op.process(engine::Event{});

    EXPECT_TRUE(std::holds_alternative<std::monostate>(op.last_event()));
}

TEST(Operator, NoOpStoresMostRecentEvent) {
    engine::NoOpOperator op;

    op.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    op.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7.0F,
    });

    EXPECT_TRUE(std::holds_alternative<engine::AudioSample>(op.last_event()));

    const auto *stored_audio = std::get_if<engine::AudioSample>(&op.last_event());
    ASSERT_NE(stored_audio, nullptr);
    EXPECT_EQ(stored_audio->timestamp, 2U);
    EXPECT_EQ(stored_audio->amplitude, 7.0F);
}

// IncrementOperator counts Events by alternative

TEST(Operator, IncrementOperatorStartsWithZeroCounts) {
    engine::IncrementOperator op;

    EXPECT_EQ(op.tick_count(), 0U);
    EXPECT_EQ(op.audio_count(), 0U);
    EXPECT_EQ(op.imu_count(), 0U);
    EXPECT_EQ(op.monostate_count(), 0U);
}

TEST(Operator, IncrementOperatorCountsTickEvent) {
    engine::IncrementOperator op;

    op.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    EXPECT_EQ(op.tick_count(), 1U);
    EXPECT_EQ(op.audio_count(), 0U);
    EXPECT_EQ(op.imu_count(), 0U);
    EXPECT_EQ(op.monostate_count(), 0U);
}

TEST(Operator, IncrementOperatorCountsAudioSample) {
    engine::IncrementOperator op;

    op.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7.0F,
    });

    EXPECT_EQ(op.tick_count(), 0U);
    EXPECT_EQ(op.audio_count(), 1U);
    EXPECT_EQ(op.imu_count(), 0U);
    EXPECT_EQ(op.monostate_count(), 0U);
}

TEST(Operator, IncrementOperatorCountsIMUSample) {
    engine::IncrementOperator op;

    op.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    });

    EXPECT_EQ(op.tick_count(), 0U);
    EXPECT_EQ(op.audio_count(), 0U);
    EXPECT_EQ(op.imu_count(), 1U);
    EXPECT_EQ(op.monostate_count(), 0U);
}

TEST(Operator, IncrementOperatorCountsMonostate) {
    engine::IncrementOperator op;

    op.process(engine::Event{});

    EXPECT_EQ(op.tick_count(), 0U);
    EXPECT_EQ(op.audio_count(), 0U);
    EXPECT_EQ(op.imu_count(), 0U);
    EXPECT_EQ(op.monostate_count(), 1U);
}

TEST(Operator, IncrementOperatorCountsMixedSequence) {
    engine::IncrementOperator op;

    op.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    op.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7.0F,
    });

    op.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    });

    op.process(engine::Event{});

    EXPECT_EQ(op.tick_count(), 1U);
    EXPECT_EQ(op.audio_count(), 1U);
    EXPECT_EQ(op.imu_count(), 1U);
    EXPECT_EQ(op.monostate_count(), 1U);
}

TEST(Operator, IncrementOperatorCountsRepeatedTickEvents) {
    engine::IncrementOperator op;

    for (int i = 0; i < 5; ++i) {
        op.process(engine::TickEvent{
            .timestamp = static_cast<uint64_t>(i),
            .price = 42,
        });
    }

    EXPECT_EQ(op.tick_count(), 5U);
    EXPECT_EQ(op.audio_count(), 0U);
    EXPECT_EQ(op.imu_count(), 0U);
    EXPECT_EQ(op.monostate_count(), 0U);
}

// Operator base pointer dispatch preserves the concrete NoOp behavior

TEST(Operator, VirtualDispatchThroughBasePointer) {
    std::unique_ptr<engine::Operator> op = std::make_unique<engine::NoOpOperator>();

    op->process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    EXPECT_EQ(op->name(), "NoOp");

    auto *noop = dynamic_cast<engine::NoOpOperator *>(op.get());
    ASSERT_NE(noop, nullptr);

    EXPECT_TRUE(std::holds_alternative<engine::TickEvent>(noop->last_event()));

    const auto *stored_tick = std::get_if<engine::TickEvent>(&noop->last_event());
    ASSERT_NE(stored_tick, nullptr);
    EXPECT_EQ(stored_tick->timestamp, 1U);
    EXPECT_EQ(stored_tick->price, 42);
}
