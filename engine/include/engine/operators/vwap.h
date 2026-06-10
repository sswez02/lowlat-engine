#pragma once

#include <engine/events.h>
#include <engine/operator.h>

#include <cstdint>
#include <string>
#include <variant>

namespace engine {

__extension__ using int128_t = __int128;

class VWAPOperator final : public Operator {
  public:
    void process(const Event &event) override {
        std::visit(overloaded{
                       [this](const TickEvent &tick) {
                           if (tick.volume <= 0) {
                               return;
                           }

                           sum_pv_ += static_cast<int128_t>(tick.price) *
                                      static_cast<int128_t>(tick.volume);
                           sum_v_ += static_cast<uint64_t>(tick.volume);
                       },
                       [](const auto &) {}, // ignore non-tick events
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
    int128_t sum_pv_ = 0; // accumulated price * volume
    uint64_t sum_v_ = 0;  // accumulated positive volume
};

} // namespace engine
