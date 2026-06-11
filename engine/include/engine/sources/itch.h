#pragma once

#include <engine/events.h>
#include <engine/pipeline.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace engine {

// Subset of NASDAQ ITCH 5.0 message parsing copied from sswez02/nasdaq-itch-cpp.
// Engine consumes Order Executed With Price ('C' messages only,
// the full parser lives in the standalone repo.

namespace itch_detail {

// Big-endian byte readers.

inline uint16_t read_be16(const uint8_t *p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

inline uint32_t read_be32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline uint64_t read_be48(const uint8_t *p) {
    return (uint64_t(p[0]) << 40) | (uint64_t(p[1]) << 32) | (uint64_t(p[2]) << 24) |
           (uint64_t(p[3]) << 16) | (uint64_t(p[4]) << 8) | uint64_t(p[5]);
}

inline uint64_t read_be64(const uint8_t *p) {
    return (uint64_t(p[0]) << 56) | (uint64_t(p[1]) << 48) | (uint64_t(p[2]) << 40) |
           (uint64_t(p[3]) << 32) | (uint64_t(p[4]) << 24) | (uint64_t(p[5]) << 16) |
           (uint64_t(p[6]) << 8) | uint64_t(p[7]);
}

} // namespace itch_detail

// ITCH 5.0 'C' message: Order Executed With Price (36 bytes).
// See NASDAQ TotalView-ITCH 5.0 specification 1.4.2.
struct OrderExecutedWithPrice {
    char message_type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint64_t timestamp;
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
    char printable;
    uint32_t execution_price;
};

inline OrderExecutedWithPrice parse_order_executed_with_price(const uint8_t *p) {
    OrderExecutedWithPrice oep;
    oep.message_type = p[0];
    oep.stock_locate = itch_detail::read_be16(p + 1);
    oep.tracking = itch_detail::read_be16(p + 3);
    oep.timestamp = itch_detail::read_be48(p + 5);
    oep.order_ref = itch_detail::read_be64(p + 11);
    oep.executed_shares = itch_detail::read_be32(p + 19);
    oep.match_number = itch_detail::read_be64(p + 23);
    oep.printable = p[31];
    oep.execution_price = itch_detail::read_be32(p + 32);
    return oep;
}

class ITCHFileSource {
  public:
    explicit ITCHFileSource(std::string path);

    // Reads the ITCH file, parses messages, pushes TickEvents into the pipeline.
    // Returns the number of TickEvents emitted.
    std::size_t run(Pipeline &pipeline);

    [[nodiscard]] std::size_t messages_processed() const noexcept;
    [[nodiscard]] std::size_t ticks_emitted() const noexcept;

  private:
    std::string path_;
    std::size_t messages_processed_ = 0;
    std::size_t ticks_emitted_ = 0;
};

inline ITCHFileSource::ITCHFileSource(std::string path) : path_(std::move(path)) {}

inline std::size_t ITCHFileSource::messages_processed() const noexcept {
    return messages_processed_;
}

inline std::size_t ITCHFileSource::ticks_emitted() const noexcept {
    return ticks_emitted_;
}

inline std::size_t ITCHFileSource::run(Pipeline &pipeline) {
    messages_processed_ = 0;
    ticks_emitted_ = 0;

    // file reading/parsing
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open ITCH file");
    }

    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(file.tellg());

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("failed to read ITCH file: " + path_);
    }
    std::size_t cursor = 0;

    while (cursor + 2 <= buffer.size()) {
        // First 2 bytes of the message input is the message length.
        const uint16_t message_length = itch_detail::read_be16(buffer.data() + cursor);
        if (message_length == 0) {
            break;
        }
        cursor += 2;

        if (cursor + message_length > buffer.size()) {
            break; // not enough bytes for full body
        }
        const uint8_t *body = buffer.data() + cursor;

        // First byte of the message body is the ITCH message type.
        const char message_type = static_cast<char>(body[0]);

        ++messages_processed_;

        if (message_type == 'C') {
            const OrderExecutedWithPrice oep = parse_order_executed_with_price(body);

            if (oep.printable == 'Y') {
                TickEvent tick{
                    .timestamp = oep.timestamp,
                    .price = static_cast<int64_t>(oep.execution_price),
                    .volume = static_cast<int64_t>(oep.executed_shares),
                };

                pipeline.process(tick);
                ++ticks_emitted_;
            }
        }
        cursor += message_length;
    }
    return ticks_emitted_;
}

} // namespace engine
