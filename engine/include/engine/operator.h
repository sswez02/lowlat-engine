#pragma once

#include <engine/events.h>

#include <string>
#include <variant>

namespace engine {

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Temporary Operator interface used to wire up the first tests and benchmarks.
// Event is defined in events.h according to ADR-002.
class Operator {
  public:
    virtual ~Operator() = default;
    virtual void process(const Event &event) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

class NoOpOperator final : public Operator {
  public:
    void process(const Event &event) override {
        std::visit([this](const auto &active_event) { last_event_ = active_event; }, event);
    }

    [[nodiscard]] std::string name() const override {
        return "NoOp";
    }

    [[nodiscard]] const Event &last_event() const {
        return last_event_;
    }

  private:
    Event last_event_{};
};

class IncrementOperator final : public Operator {
  public:
    void process(const Event &event) override {
        std::visit(overloaded{
                       [this](const TickEvent &) { ++tick_count_; },
                       [this](const AudioSample &) { ++audio_count_; },
                       [this](const IMUSample &) { ++imu_count_; },
                       [this](const std::monostate &) { ++monostate_count_; },
                   },
                   event);
    }

    [[nodiscard]] std::string name() const override {
        return "Increment";
    }

    [[nodiscard]] uint64_t tick_count() const {
        return tick_count_;
    }

    [[nodiscard]] uint64_t audio_count() const {
        return audio_count_;
    }

    [[nodiscard]] uint64_t imu_count() const {
        return imu_count_;
    }

    [[nodiscard]] uint64_t monostate_count() const {
        return monostate_count_;
    }

  private:
    uint64_t tick_count_ = 0;
    uint64_t audio_count_ = 0;
    uint64_t imu_count_ = 0;
    uint64_t monostate_count_ = 0;
};

} // namespace engine
