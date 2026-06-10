#include <engine/events.h>
#include <engine/operator.h>
#include <engine/pipeline.h>

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <variant>

TEST(Pipeline, EmptyPipelineDoesNotCrash) {
    engine::Pipeline pipeline;

    EXPECT_NO_THROW(pipeline.process(engine::Event{}));

    EXPECT_NO_THROW(pipeline.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    }));

    EXPECT_NO_THROW(pipeline.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7,
    }));

    EXPECT_NO_THROW(pipeline.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    }));
}

TEST(Pipeline, PipelineSizeReflectsAdds) {
    engine::Pipeline pipeline;

    EXPECT_EQ(pipeline.size(), 0U);

    pipeline.add(std::make_unique<engine::NoOpOperator>());
    EXPECT_EQ(pipeline.size(), 1U);

    pipeline.add(std::make_unique<engine::IncrementOperator>());
    pipeline.add(std::make_unique<engine::NoOpOperator>());
    EXPECT_EQ(pipeline.size(), 3U);
}

TEST(Pipeline, SingleOperatorReceivesEvent) {
    engine::Pipeline pipeline;

    auto noop = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr = noop.get();

    pipeline.add(std::move(noop));

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    };

    pipeline.process(event);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr->last_event()));

    const auto &stored_event = std::get<engine::TickEvent>(noop_ptr->last_event());

    EXPECT_EQ(stored_event.timestamp, 1U);
    EXPECT_EQ(stored_event.price, 42);
}

TEST(Pipeline, MultipleOperatorsAllReceiveEvent) {
    engine::Pipeline pipeline;

    auto noop1 = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr1 = noop1.get();
    auto noop2 = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr2 = noop2.get();
    auto noop3 = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr3 = noop3.get();

    pipeline.add(std::move(noop1));
    pipeline.add(std::move(noop2));
    pipeline.add(std::move(noop3));

    const engine::Event event = engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    };

    pipeline.process(event);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr1->last_event()));
    const auto &stored_event1 = std::get<engine::TickEvent>(noop_ptr1->last_event());
    EXPECT_EQ(stored_event1.timestamp, 1U);
    EXPECT_EQ(stored_event1.price, 42);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr2->last_event()));
    const auto &stored_event2 = std::get<engine::TickEvent>(noop_ptr2->last_event());
    EXPECT_EQ(stored_event2.timestamp, 1U);
    EXPECT_EQ(stored_event2.price, 42);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr3->last_event()));
    const auto &stored_event3 = std::get<engine::TickEvent>(noop_ptr3->last_event());
    EXPECT_EQ(stored_event3.timestamp, 1U);
    EXPECT_EQ(stored_event3.price, 42);
}

TEST(Pipeline, HeterogeneousOperators) {
    engine::Pipeline pipeline;

    auto noop1 = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr1 = noop1.get();

    auto increment = std::make_unique<engine::IncrementOperator>();
    auto *increment_ptr = increment.get();

    auto noop2 = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr2 = noop2.get();

    pipeline.add(std::move(noop1));
    pipeline.add(std::move(increment));
    pipeline.add(std::move(noop2));

    pipeline.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    pipeline.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7,
    });

    pipeline.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    });

    EXPECT_EQ(increment_ptr->tick_count(), 1U);
    EXPECT_EQ(increment_ptr->audio_count(), 1U);
    EXPECT_EQ(increment_ptr->imu_count(), 1U);
    EXPECT_EQ(increment_ptr->monostate_count(), 0U);

    ASSERT_TRUE(std::holds_alternative<engine::IMUSample>(noop_ptr1->last_event()));
    ASSERT_TRUE(std::holds_alternative<engine::IMUSample>(noop_ptr2->last_event()));

    const auto &stored_event1 = std::get<engine::IMUSample>(noop_ptr1->last_event());
    const auto &stored_event2 = std::get<engine::IMUSample>(noop_ptr2->last_event());

    EXPECT_EQ(stored_event1.timestamp, 3U);
    EXPECT_FLOAT_EQ(stored_event1.acceleration, 9.5F);

    EXPECT_EQ(stored_event2.timestamp, 3U);
    EXPECT_FLOAT_EQ(stored_event2.acceleration, 9.5F);
}

TEST(Pipeline, PipelineProcessesAllEventTypes) {
    engine::Pipeline pipeline;

    auto increment = std::make_unique<engine::IncrementOperator>();
    auto *increment_ptr = increment.get();

    pipeline.add(std::move(increment));

    pipeline.process(engine::TickEvent{
        .timestamp = 1U,
        .price = 42,
    });

    pipeline.process(engine::AudioSample{
        .timestamp = 2U,
        .amplitude = 7,
    });

    pipeline.process(engine::IMUSample{
        .timestamp = 3U,
        .acceleration = 9.5F,
    });

    EXPECT_EQ(increment_ptr->tick_count(), 1U);
    EXPECT_EQ(increment_ptr->audio_count(), 1U);
    EXPECT_EQ(increment_ptr->imu_count(), 1U);
    EXPECT_EQ(increment_ptr->monostate_count(), 0U);
}
