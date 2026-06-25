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

// Minimal RIFF/WAV PCM parser for AudioSample ingestion.
// Engine supports 16-bit mono PCM WAV at 44.1 kHz or 48 kHz only,
// unsupported WAV variants are rejected explicitly.

namespace wav_detail {

// WAV/RIFF stores integer fields in little-endian byte order.

inline std::uint16_t read_le16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(p[0]) |
                                      (static_cast<std::uint32_t>(p[1]) << 8U));
}

inline std::uint32_t read_le32(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
        (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U));
}

inline bool fourcc_equals(const std::uint8_t *p, const char *id) {
    return p[0] == static_cast<std::uint8_t>(id[0]) && p[1] == static_cast<std::uint8_t>(id[1]) &&
           p[2] == static_cast<std::uint8_t>(id[2]) && p[3] == static_cast<std::uint8_t>(id[3]);
}

} // namespace wav_detail

// WAV "fmt " chunk payload for PCM.
// For PCM, the payload is normally 16 bytes.
struct WavFormat {
    std::uint16_t audio_format;
    std::uint16_t num_channels;
    std::uint32_t sample_rate;
    std::uint32_t byte_rate;
    std::uint16_t block_align;
    std::uint16_t bits_per_sample;
};

inline WavFormat parse_wav_format(const std::uint8_t *p) {
    WavFormat fmt;
    fmt.audio_format = wav_detail::read_le16(p + 0);
    fmt.num_channels = wav_detail::read_le16(p + 2);
    fmt.sample_rate = wav_detail::read_le32(p + 4);
    fmt.byte_rate = wav_detail::read_le32(p + 8);
    fmt.block_align = wav_detail::read_le16(p + 12);
    fmt.bits_per_sample = wav_detail::read_le16(p + 14);
    return fmt;
}

class WAVFileSource {
  public:
    explicit WAVFileSource(std::string path);

    // Parses an in-memory WAV buffer and pushes AudioSamples into the pipeline.
    // Returns the number of AudioSamples emitted.
    std::size_t run_buffer(const std::uint8_t *data, std::size_t size, Pipeline &pipeline);

    // Reads the WAV file from disk, then parses it through run_buffer.
    // Returns the number of AudioSamples emitted.
    std::size_t run(Pipeline &pipeline);

    [[nodiscard]] std::size_t samples_processed() const noexcept;
    [[nodiscard]] std::size_t samples_emitted() const noexcept;

  private:
    std::string path_;
    std::size_t samples_processed_ = 0;
    std::size_t samples_emitted_ = 0;
};

inline WAVFileSource::WAVFileSource(std::string path) : path_(std::move(path)) {}

inline std::size_t WAVFileSource::samples_processed() const noexcept {
    return samples_processed_;
}

inline std::size_t WAVFileSource::samples_emitted() const noexcept {
    return samples_emitted_;
}

inline std::size_t WAVFileSource::run_buffer(const std::uint8_t *data, std::size_t size,
                                             Pipeline &pipeline) {
    samples_processed_ = 0;
    samples_emitted_ = 0;

    if (size < 12U) {
        throw std::runtime_error("not a WAV file: too small");
    }

    // offset 0-3    "RIFF"     4 bytes
    // offset 4-7    file size  4 bytes
    // offset 8-11   "WAVE"     4 bytes
    // offset 12...  chunks start

    if (!wav_detail::fourcc_equals(data, "RIFF")) {
        throw std::runtime_error("not a RIFF file");
    }
    if (!wav_detail::fourcc_equals(data + 8U, "WAVE")) {
        throw std::runtime_error("not a WAVE file");
    }

    // Loop variables
    std::size_t cursor = 12U;

    bool found_fmt = false;
    bool found_data = false;

    WavFormat format{};

    std::size_t data_offset = 0;
    std::size_t data_size = 0;

    // first "fmt " chunk:
    // offset 12-15   "fmt "       chunk ID
    // offset 16-19   16           chunk payload size
    // offset 20-35   format info  payload

    while (cursor + 8U <= size) {
        const auto *chunk = data + cursor;
        const auto chunk_size = static_cast<std::size_t>(wav_detail::read_le32(chunk + 4U));
        const auto payload_offset = cursor + 8U;

        if (payload_offset + chunk_size > size) {
            throw std::runtime_error("invalid WAV file: chunk extends past end of file");
        }

        if (wav_detail::fourcc_equals(chunk, "fmt ")) {
            if (chunk_size < 16U) {
                throw std::runtime_error("invalid WAV file: fmt chunk too small");
            }

            format = parse_wav_format(data + payload_offset);
            found_fmt = true;

        } else if (wav_detail::fourcc_equals(chunk, "data")) {
            data_offset = payload_offset;
            data_size = chunk_size;
            found_data = true;
        }

        cursor = payload_offset + chunk_size;

        if ((chunk_size & 1U) != 0U) {
            ++cursor;
        }
    }

    if (!found_fmt) {
        throw std::runtime_error("invalid WAV file: missing fmt chunk");
    }

    if (!found_data) {
        throw std::runtime_error("invalid WAV file: missing data chunk");
    }

    if (format.audio_format != 1U) {
        throw std::runtime_error("unsupported WAV: not PCM");
    }

    if (format.num_channels != 1U) {
        throw std::runtime_error("unsupported WAV: not mono");
    }

    if (format.bits_per_sample != 16U) {
        throw std::runtime_error("unsupported WAV: not 16-bit");
    }

    if (format.sample_rate != 44100U && format.sample_rate != 48000U) {
        throw std::runtime_error("unsupported WAV: sample rate");
    }

    if ((data_size % 2U) != 0U) {
        throw std::runtime_error("invalid WAV file: data chunk has partial sample");
    }

    std::uint64_t timestamp = 0;
    cursor = data_offset;
    const auto data_end = data_offset + data_size; // End of the data chunk

    // 2 bytes since we only accept 16 bit mono
    while (cursor + 2U <= data_end) {
        // C++20 defines conversion from uint16_t to int16_t modulo 2^16,
        // so PCM values with the high bit set become negative samples.
        const auto raw_sample = static_cast<std::int16_t>(wav_detail::read_le16(data + cursor));

        // Convert signed 16-bit PCM to float32 -1.0, 1.0.
        const auto amplitude = static_cast<float>(raw_sample) / 32768.0F;

        pipeline.process(Event{AudioSample{
            .timestamp = timestamp,
            .amplitude = amplitude,
        }});

        cursor += 2U;
        ++timestamp;
        ++samples_processed_;
        ++samples_emitted_;
    }

    return samples_emitted_;
}

inline std::size_t WAVFileSource::run(Pipeline &pipeline) {
    // file reading/parsing
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open WAV file");
    }

    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(file.tellg());

    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("failed to read WAV file: " + path_);
    }

    return run_buffer(buffer.data(), buffer.size(), pipeline);
}
} // namespace engine
