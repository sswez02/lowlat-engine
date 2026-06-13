#pragma once

#include <engine/events.h>
#include <engine/operator.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace engine {

/**
 * @brief Simple Moving Average (SMA) Operator.
 * * Maintains a sliding window of the most recent N prices via a fixed-size pre-allocated ring
 * buffer.
 * * @note For O(1) latency optimization, window_size MUST be a strict power of two
 * (e.g., 2, 4, 8, 16, 32). Common windows like 20 or 50 are not supported
 * and will trigger an assertion failure on construction.
 */
class SMAOperator final : public Operator {
  public:
    explicit SMAOperator(std::size_t window_size)
        : window_size_(window_size), // Initialize size
          prices_(window_size, 0) {  // Pre-allocate and zero-fill vector
        assert(window_size > 0 && "Window size must be strictly positive");
        // Assert power of two for the O(1) bitwise ring-buffer wrap.
        assert((window_size & (window_size - 1)) == 0 && "Window size must be a power of two");
    }

    void process(const Event &event) override {
        std::visit(overloaded{
                       [this](const TickEvent &tick) {
                           sum_ -= prices_[cursor_];
                           prices_[cursor_] = tick.price;
                           sum_ += tick.price;

                           cursor_ = (cursor_ + 1) & (window_size_ - 1);
                           if (count_ < window_size_) {
                               ++count_;
                           }
                       },
                       [](const auto &) {}, // ignore non-tick events
                   },
                   event);
    }

    [[nodiscard]] std::string name() const override {
        return "SMAOperator";
    }

    [[nodiscard]] double sma() const noexcept {
        return count_ == 0 ? 0.0 : static_cast<double>(sum_) / static_cast<double>(count_);
    }

  private:
    std::size_t window_size_;
    std::vector<int64_t> prices_;
    std::size_t cursor_ = 0;
    std::size_t count_ = 0;
    int64_t sum_ = 0;
};

} // namespace engine
