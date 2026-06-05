#include <engine/operator.h>
#include <gtest/gtest.h>
#include <memory>

TEST(Operator, NoOpStoresLastValue) {
    engine::NoOpOperator op;
    op.process(42);
    EXPECT_EQ(op.last_value(), 42);
}

TEST(Operator, VirtualDispatchThroughBasePointer) {
    std::unique_ptr<engine::Operator> op = std::make_unique<engine::NoOpOperator>();
    op->process(7);
    EXPECT_EQ(op->name(), "NoOp");

    auto *noop = dynamic_cast<engine::NoOpOperator *>(op.get());
    ASSERT_NE(noop, nullptr);
    EXPECT_EQ(noop->last_value(), 7);
}

TEST(Operator, IncrementOperatorAccumulatesInput) {
    engine::IncrementOperator op;
    op.process(2);
    op.process(3);

    EXPECT_EQ(op.name(), "Increment");
    EXPECT_EQ(op.sum(), 5);
}
