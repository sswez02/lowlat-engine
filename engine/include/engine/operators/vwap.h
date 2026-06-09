#pragma once

#include <engine/events.h>
#include <engine/operator.h>

#include <cstdint>
#include <string>
#include <variant>

namespace engine {

class VWAPOperator final : public Operator {
  public:
    void process(const Event &event) override {
        std::visit(overloaded{
                       [this](const TickEvent &tick) {
                           if (tick.volume <= 0) {
                               return;
                           }

                           sum_pv_ += static_cast<__int128>(tick.price) *
                                      static_cast<__int128>(tick.volume);
                           sum_v_ += static_cast<uint64_t>(tick.volume);
                       },
                       [](auto &&) {} // ignore non-tick events
                   },
                   event);
    }

    [[nodiscard]] std::string name() const override {
        return "VWAPOperator";
    }

    [[nodiscard]] double vwap() const noexcept {
        return sum_v_ == 0U ? 0.0 : static_cast<double>(sum_pv_) / static_cast<double>(sum_v_);
    }

  private:
    __int128 sum_pv_ = 0; // accumulated price * volume
    uint64_t sum_v_ = 0;  // accumulated volume
};

} // namespace engine
