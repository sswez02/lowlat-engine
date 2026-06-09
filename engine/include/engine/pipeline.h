#pragma once

#include <engine/events.h>
#include <engine/operator.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace engine {

class Pipeline {
  public:
    Pipeline() = default;

    // Non-copyable (holds unique_ptrs), movable.
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    Pipeline(Pipeline &&) = default;
    Pipeline &operator=(Pipeline &&) = default;

    void add(std::unique_ptr<Operator> op) {
        operators_.push_back(std::move(op));
    }

    void process(const Event &event) {
        for (auto &op : operators_) {
            op->process(event);
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return operators_.size();
    }

  private:
    std::vector<std::unique_ptr<Operator>> operators_;
};

} // namespace engine
