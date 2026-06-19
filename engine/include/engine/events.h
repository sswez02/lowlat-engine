#pragma once

#include <cstdint>
#include <type_traits>
#include <variant>

namespace engine {

struct TickEvent {
    uint64_t timestamp{};
    int64_t price{};
    int64_t volume{};
};

struct AudioSample {
    uint64_t timestamp{};
    float amplitude{};
};

struct IMUSample {
    uint64_t timestamp{};
    float acceleration{};
};

static_assert(std::is_trivially_copyable_v<TickEvent>);
static_assert(std::is_trivially_copyable_v<AudioSample>);
static_assert(std::is_trivially_copyable_v<IMUSample>);

using Event = std::variant<std::monostate, TickEvent, AudioSample, IMUSample>;

static_assert(std::is_trivially_copyable_v<Event>);

} // namespace engine
