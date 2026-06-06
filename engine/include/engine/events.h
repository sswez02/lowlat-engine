#pragma once

#include <cstdint>
#include <type_traits>
#include <variant>

namespace engine {

struct TickEvent {
    uint64_t timestamp = 0;
    int64_t price = 0;
};

struct AudioSample {
    uint64_t timestamp = 0;
    int16_t amplitude = 0;
};

struct IMUSample {
    uint64_t timestamp = 0;
    float acceleration = 0;
};

static_assert(std::is_trivially_copyable_v<TickEvent>);
static_assert(std::is_trivially_copyable_v<AudioSample>);
static_assert(std::is_trivially_copyable_v<IMUSample>);

using Event = std::variant<std::monostate, TickEvent, AudioSample, IMUSample>;

static_assert(std::is_trivially_copyable_v<Event>);

} // namespace engine
