#include <engine/operator.h>
#include <engine/operators/rms.h>
#include <engine/pipeline.h>
#include <engine/sources/wav.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Take a 16-bit number, write the low byte first, then write the high byte.
static void write_le16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

static void write_le32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

// Writes a 4-byte chunk ID as raw bytes.
static void append_fourcc(std::vector<std::uint8_t> &out, const char *id) {
    out.push_back(static_cast<std::uint8_t>(id[0]));
    out.push_back(static_cast<std::uint8_t>(id[1]));
    out.push_back(static_cast<std::uint8_t>(id[2]));
    out.push_back(static_cast<std::uint8_t>(id[3]));
}

struct WavSpec {
    std::uint16_t channels;
    std::uint16_t bits;
    std::uint32_t sample_rate;
};

// Makes a complete wav test file with header and data chunks.
static std::vector<std::uint8_t> make_wav(const WavSpec &spec,
                                          const std::vector<std::int16_t> &samples) {
    std::vector<std::uint8_t> out;

    // One sample is 2 bytes.
    const auto data_bytes_size = static_cast<std::uint32_t>(samples.size() * 2U);

    const auto bytes_per_sample = static_cast<std::uint16_t>(spec.bits / 8U);
    const auto block_align = static_cast<std::uint16_t>(spec.channels * bytes_per_sample);
    const auto byte_rate = static_cast<std::uint32_t>(spec.sample_rate * block_align);

    append_fourcc(out, "RIFF");

    // RIFF payload size: "WAVE" + fmt chunk header/payload + data chunk header/payload.
    const auto riff_size = 4U + 24U + 8U + data_bytes_size;
    write_le32(out, riff_size);

    // "WAVE" chunk
    append_fourcc(out, "WAVE");

    // fmt chunk
    append_fourcc(out, "fmt ");
    write_le32(out, 16U); // fmt payload size for PCM
    write_le16(out, 1U);  // audio_format: PCM
    write_le16(out, spec.channels);
    write_le32(out, spec.sample_rate);
    write_le32(out, byte_rate);
    write_le16(out, block_align);
    write_le16(out, spec.bits);

    // data chunk
    append_fourcc(out, "data");
    write_le32(out, data_bytes_size);

    for (const auto sample : samples) {
        write_le16(out, static_cast<std::uint16_t>(sample));
    }

    return out;
}

// Makes a temporary file.
static std::filesystem::path write_temp_file(const std::string &name,
                                             const std::vector<std::uint8_t> &bytes) {
    // Unique string for the temporary filename
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / (name + "_" + unique + ".wav");

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));

    return path;
}

static std::filesystem::path make_missing_temp_path(const std::string &name) {
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / (name + "_" + unique + ".wav");
}

TEST(WAVFileSource, OpenNonExistentFileThrows) {
    const auto path = make_missing_temp_path("OpenNonExistentFileThrows");

    engine::Pipeline pipeline;
    engine::WAVFileSource source{path.string()};

    EXPECT_THROW(source.run(pipeline), std::runtime_error);
}

TEST(WAVFileSource, RejectsNon16BitWAV) {
    const auto bytes = make_wav(WavSpec{.channels = 1U, .bits = 8U, .sample_rate = 44100U}, {
                                                                                                0,
                                                                                                100,
                                                                                            });

    const auto path = write_temp_file("RejectsNon16BitWAV", bytes);

    engine::Pipeline pipeline;
    engine::WAVFileSource source{path.string()};

    EXPECT_THROW(source.run(pipeline), std::runtime_error);

    std::filesystem::remove(path);
}

TEST(WAVFileSource, RejectsStereoWAV) {
    const auto bytes =
        make_wav(WavSpec{.channels = 2U, .bits = 16U, .sample_rate = 44100U}, {
                                                                                  0,
                                                                                  100,
                                                                              });

    const auto path = write_temp_file("RejectsStereoWAV", bytes);

    engine::Pipeline pipeline;
    engine::WAVFileSource source{path.string()};

    EXPECT_THROW(source.run(pipeline), std::runtime_error);

    std::filesystem::remove(path);
}

TEST(WAVFileSource, ParsesValidMonoWAV) {
    const auto bytes =
        make_wav(WavSpec{.channels = 1U, .bits = 16U, .sample_rate = 44100U}, {
                                                                                  0,
                                                                                  16384,
                                                                                  -16384,
                                                                                  32767,
                                                                              });
    const auto path = write_temp_file("ParsesValidMonoWAV", bytes);

    engine::Pipeline pipeline;

    auto noop = std::make_unique<engine::NoOpOperator>();
    auto *noop_ptr = noop.get();
    pipeline.add(std::move(noop));

    engine::WAVFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 4U);
    EXPECT_EQ(source.samples_processed(), 4U);
    EXPECT_EQ(source.samples_emitted(), 4U);

    ASSERT_TRUE(std::holds_alternative<engine::AudioSample>(noop_ptr->last_event()));

    const auto &sample = std::get<engine::AudioSample>(noop_ptr->last_event());

    EXPECT_EQ(sample.timestamp, 3U);
    EXPECT_NEAR(sample.amplitude, 32767.0F / 32768.0F, 1e-6F);

    std::filesystem::remove(path);
}

TEST(WAVFileSource, EndToEndWithRMS) {
    const auto bytes =
        make_wav(WavSpec{.channels = 1U, .bits = 16U, .sample_rate = 44100U}, {
                                                                                  16384,
                                                                                  16384,
                                                                                  16384,
                                                                                  16384,
                                                                              });
    const auto path = write_temp_file("EndToEndWithRMS", bytes);

    engine::Pipeline pipeline;

    auto rms = std::make_unique<engine::RMSOperator>(4);
    auto *rms_ptr = rms.get();
    pipeline.add(std::move(rms));

    engine::WAVFileSource source{path.string()};
    const auto emitted = source.run(pipeline);

    EXPECT_EQ(emitted, 4U);
    EXPECT_EQ(source.samples_processed(), 4U);
    EXPECT_EQ(source.samples_emitted(), 4U);

    // Each sample is 16384 / 32768 = 0.5, so the RMS of four identical 0.5-amplitude samples is
    // exactly 0.5.
    EXPECT_NEAR(rms_ptr->rms(), 0.5, 1e-6);

    std::filesystem::remove(path);
}
