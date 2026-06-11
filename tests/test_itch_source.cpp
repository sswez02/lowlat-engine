#include <engine/operator.h>
#include <engine/operators/vwap.h>
#include <engine/pipeline.h>
#include <engine/sources/itch.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Take a 16-bit number, write the high byte first, then write the low byte.
static void write_be16(std::vector<uint8_t> &out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void write_be32(std::vector<uint8_t> &out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void write_be48(std::vector<uint8_t> &out, uint64_t value) {
    out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void write_be64(std::vector<uint8_t> &out, uint64_t value) {
    out.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Makes a test OEP message.
static std::vector<uint8_t> make_oep_message(uint64_t timestamp, uint32_t price, uint32_t shares,
                                             char printable) {
    std::vector<uint8_t> out;

    write_be16(out, 36); // 2-byte length prefix
    out.push_back(static_cast<uint8_t>('C'));

    write_be16(out, 1);         // stock_locate
    write_be16(out, 1);         // tracking
    write_be48(out, timestamp); // timestamp
    write_be64(out, 123);       // order_ref
    write_be32(out, shares);    // executed_shares
    write_be64(out, 456);       // match_number
    out.push_back(static_cast<uint8_t>(printable));
    write_be32(out, price); // execution_price

    EXPECT_EQ(out.size(), 38U);
    return out;
}

// Makes a temporary file.
static std::filesystem::path write_temp_file(const std::string &name,
                                             const std::vector<uint8_t> &bytes) {
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / (name + "_" + unique + ".itch");

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));

    return path;
}

static void append_bytes(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

TEST(ITCHFileSource, OneOEPProducesOneTick) {
    const auto bytes = make_oep_message(123U, 42U, 10U, 'Y');
    const auto path = write_temp_file("OneOEPProducesOneTick", bytes);

    engine::Pipeline pipeline;

    auto noop = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr = noop.get();

    pipeline.add(std::move(noop));

    engine::ITCHFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 1U);
    EXPECT_EQ(source.messages_processed(), 1U);
    EXPECT_EQ(source.ticks_emitted(), 1U);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr->last_event()));

    const auto &tick = std::get<engine::TickEvent>(noop_ptr->last_event());

    EXPECT_EQ(tick.timestamp, 123U);
    EXPECT_EQ(tick.price, 42);
    EXPECT_EQ(tick.volume, 10);

    std::filesystem::remove(path);
}

TEST(ITCHFileSource, NonPrintableOEPSkipped) {
    const auto bytes = make_oep_message(123U, 42U, 10U, 'N');
    const auto path = write_temp_file("NonPrintableOEPSkipped", bytes);

    engine::Pipeline pipeline;

    engine::ITCHFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 0U);
    EXPECT_EQ(source.messages_processed(), 1U);
    EXPECT_EQ(source.ticks_emitted(), 0U);

    std::filesystem::remove(path);
}

TEST(ITCHFileSource, MultipleOEPsAllProcessed) {
    std::vector<uint8_t> bytes;

    append_bytes(bytes, make_oep_message(100U, 10U, 1U, 'Y'));
    append_bytes(bytes, make_oep_message(200U, 20U, 2U, 'Y'));
    append_bytes(bytes, make_oep_message(300U, 30U, 3U, 'Y'));

    const auto path = write_temp_file("MultipleOEPsAllProcessed", bytes);

    engine::Pipeline pipeline;

    auto noop = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr = noop.get();
    pipeline.add(std::move(noop));

    engine::ITCHFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 3U);
    EXPECT_EQ(source.messages_processed(), 3U);
    EXPECT_EQ(source.ticks_emitted(), 3U);

    ASSERT_TRUE(std::holds_alternative<engine::TickEvent>(noop_ptr->last_event()));
    const auto &last_tick = std::get<engine::TickEvent>(noop_ptr->last_event());

    EXPECT_EQ(last_tick.timestamp, 300U);
    EXPECT_EQ(last_tick.price, 30);
    EXPECT_EQ(last_tick.volume, 3);

    std::filesystem::remove(path);
}

TEST(ITCHFileSource, EmptyFileEmitsNoTicks) {
    const std::vector<uint8_t> bytes;
    const auto path = write_temp_file("EmptyFileEmitsNoTicks", bytes);

    engine::Pipeline pipeline;

    engine::ITCHFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 0U);
    EXPECT_EQ(source.messages_processed(), 0U);
    EXPECT_EQ(source.ticks_emitted(), 0U);

    std::filesystem::remove(path);
}

TEST(ITCHFileSource, EndToEndWithVWAP) {
    std::vector<uint8_t> bytes;

    append_bytes(bytes, make_oep_message(100U, 100U, 1U, 'Y'));
    append_bytes(bytes, make_oep_message(200U, 200U, 3U, 'Y'));

    const auto path = write_temp_file("EndToEndWithVWAP", bytes);

    engine::Pipeline pipeline;

    auto vwap = std::make_unique<engine::VWAPOperator>();
    auto *vwap_ptr = vwap.get();
    pipeline.add(std::move(vwap));

    engine::ITCHFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 2U);
    EXPECT_EQ(source.messages_processed(), 2U);
    EXPECT_EQ(source.ticks_emitted(), 2U);

    // VWAP = ((100 * 1) + (200 * 3)) / (1 + 3) = 700 / 4 = 175
    EXPECT_DOUBLE_EQ(vwap_ptr->vwap(), 175.0);

    std::filesystem::remove(path);
}
