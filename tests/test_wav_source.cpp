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
void write_le16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_le32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

// Writes a 4-byte chunk ID as raw bytes.
void append_fourcc(std::vector<std::uint8_t> &out, const char *id) {
    out.push_back(static_cast<std::uint8_t>(id[0]));
    out.push_back(static_cast<std::uint8_t>(id[1]));
    out.push_back(static_cast<std::uint8_t>(id[2]));
    out.push_back(static_cast<std::uint8_t>(id[3]));
}

// Writes the wav header chunks
void write_wav_header(std::vector<std::uint8_t> &out, std::uint16_t channels, std::uint16_t bits,
                      std::uint32_t sample_rate, std::uint32_t data_bytes_size) {
    const auto bytes_per_sample = static_cast<std::uint16_t>(bits / 8U);
    const auto block_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    const auto byte_rate = static_cast<std::uint32_t>(sample_rate * block_align);

    append_fourcc(out, "RIFF");

    // RIFF payload size chunk: "WAVE" + fmt chunk header/payload + data chunk header/payload.
    const auto riff_size = 4U + 24U + 8U + data_bytes_size;
    write_le32(out, riff_size);

    // "WAVE" chunk
    append_fourcc(out, "WAVE");

    // fmt chunk
    append_fourcc(out, "fmt ");
    write_le32(out, 16U); // fmt payload size for PCM
    write_le16(out, 1U);  // audio_format: PCM
    write_le16(out, channels);
    write_le32(out, sample_rate);
    write_le32(out, byte_rate);
    write_le16(out, block_align);
    write_le16(out, bits);

    // data chunk
    append_fourcc(out, "data");
    write_le32(out, data_bytes_size);
}

// Makes a complete wav file with header and data chunks.
std::vector<std::uint8_t> make_wav(std::uint16_t channels, std::uint16_t bits,
                                   std::uint32_t sample_rate,
                                   const std::vector<std::int16_t> &samples) {
    std::vector<std::uint8_t> out;

    // One sample is 2 bytes
    const auto data_bytes_size = static_cast<std::uint32_t>(samples.size() * 2U);
    write_wav_header(out, channels, bits, sample_rate, data_bytes_size);

    for (const auto sample : samples) {
        write_le16(out, static_cast<std::uint16_t>(sample));
    }

    return out;
}

// Makes a temporary file.
std::filesystem::path write_temp_file(const std::string &name,
                                      const std::vector<std::uint8_t> &bytes) {
    // Unique string for the temporary filename
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / (name + "_" + unique + ".wav");

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));

    return path;
}

TEST(WAVFileSource, EndToEndWithRMS) {
    const auto bytes = make_wav(1U, 16U, 44100U,
                                {
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
