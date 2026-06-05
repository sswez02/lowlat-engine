#pragma once

#include <string>

namespace engine {

// Temporary Operator interface used to wire up the first tests and benchmarks.
// For now process() takes an int; later this will be replaced with real Event/Sink types.
class Operator {
  public:
    virtual ~Operator() = default;
    virtual void process(int input) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

class NoOpOperator final : public Operator {
  public:
    void process(int input) override {
        last_value_ = input;
    }

    [[nodiscard]] std::string name() const override {
        return "NoOp";
    }

    [[nodiscard]] int last_value() const {
        return last_value_;
    }

  private:
    int last_value_ = 0;
};

class IncrementOperator final : public Operator {
  public:
    void process(int input) override {
        sum_ += input;
    }

    [[nodiscard]] std::string name() const override {
        return "Increment";
    }

    [[nodiscard]] int sum() const {
        return sum_;
    }

  private:
    int sum_ = 0;
};

} // namespace engine
