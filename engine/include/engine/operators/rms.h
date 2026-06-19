#pragma once

#include <engine/events.h>
#include <engine/operator.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace engine {

/**
 * @brief Root Mean Square (RMS) Operator.
 *
 * Maintains a sliding window of the most recent N audio sample amplitudes via a fixed-size
 * pre-allocated ring buffer.
 *
 * RMS is computed as:
 * sqrt(sum(amplitude^2) / N)
 *
 * @note For O(1) latency optimization, window_size MUST be a strict power of two
 * (e.g., 2, 4, 8, 16, 32). Common windows like 20 or 50 are not supported
 * and will trigger an assertion failure on construction.
 */
class RMSOperator final : public Operator {
  public:
    explicit RMSOperator(std::size_t window_size)
        : window_size_(window_size),    // Initialize size
          samples_(window_size, 0.0F) { // Pre-allocate and zero-fill vector
        assert(window_size > 0 && "Window size must be strictly positive");
        // Assert power of two for the O(1) bitwise ring-buffer wrap.
        assert((window_size & (window_size - 1)) == 0 && "Window size must be a power of two");
    }

    void process(const Event &event) override {
        std::visit(overloaded{
                       [this](const AudioSample &sample) {
                           const double old_sample = static_cast<double>(samples_[cursor_]);
                           sum_sq_ -= old_sample * old_sample;

                           samples_[cursor_] = sample.amplitude;

                           const double new_sample = static_cast<double>(sample.amplitude);
                           sum_sq_ += new_sample * new_sample;

                           cursor_ = (cursor_ + 1) & (window_size_ - 1);

                           if (count_ < window_size_) {
                               ++count_;
                           }
                       },
                       [](const auto &) {}, // ignore non-audio events
                   },
                   event);
    }

    [[nodiscard]] std::string name() const override {
        return "RMSOperator";
    }

    [[nodiscard]] double rms() const noexcept {
        return count_ == 0U ? 0.0 : std::sqrt(sum_sq_ / static_cast<double>(count_));
    }

  private:
    std::size_t window_size_;
    std::vector<float> samples_;
    double sum_sq_ = 0;
    std::size_t cursor_ = 0;
    std::size_t count_ = 0;
};

} // namespace engine
