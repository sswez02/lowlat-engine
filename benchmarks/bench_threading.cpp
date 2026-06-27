#include <benchmark/benchmark.h>

#include <engine/operator.h>
#include <engine/operators/rms.h>
#include <engine/operators/vwap.h>
#include <engine/sources/itch.h>
#include <engine/sources/wav.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <pthread.h>

// Used to pin the current worker thread.
static void pin_current_thread_to_core(int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

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

static std::vector<std::uint8_t> make_synthetic_wav(std::size_t sample_count) {
    std::vector<std::uint8_t> out;

    constexpr std::uint16_t channels = 1U;
    constexpr std::uint16_t bits = 16U;
    constexpr std::uint32_t sample_rate = 48000U;

    // One sample is 2 bytes.
    const auto data_bytes_size = static_cast<std::uint32_t>(sample_count * 2U);
    const auto bytes_per_sample = static_cast<std::uint16_t>(bits / 8U);
    const auto block_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    const auto byte_rate = static_cast<std::uint32_t>(sample_rate * block_align);

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
    write_le16(out, channels);
    write_le32(out, sample_rate);
    write_le32(out, byte_rate);
    write_le16(out, block_align);
    write_le16(out, bits);

    // data chunk
    append_fourcc(out, "data");
    write_le32(out, data_bytes_size);

    for (std::size_t i = 0; i < sample_count; ++i) {
        write_le16(out, static_cast<std::uint16_t>(16384));
    }

    return out;
}

static bool load_itch_slice(std::vector<std::uint8_t> &out, std::string &error) {
    // Use LOWLAT_ITCH_FILE.
    const char *path = std::getenv("LOWLAT_ITCH_FILE");
    if (path == nullptr) {
        error = "LOWLAT_ITCH_FILE env var not set";
        return false;
    }

    constexpr std::size_t itch_slice_bytes = 26U * 1024U * 1024U; // 26 MB

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "failed to open LOWLAT_ITCH_FILE";
        return false;
    }

    out.resize(itch_slice_bytes);

    // Load up to 26 MB into out.
    file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));

    // Shrink to the actual number of bytes read if the file is smaller than the slice.
    const auto bytes_read = static_cast<std::size_t>(file.gcount());
    out.resize(bytes_read);

    if (out.empty()) {
        error = "LOWLAT_ITCH_FILE was empty";
        return false;
    }

    return true;
}

// Google Benchmark fixture used to prepare shared input buffers before each benchmark runs.
// SetUp fills the ITCH and WAV byte buffers so the timed loop measures pipeline parsing/processing,
// not file I/O or WAV generation.
class ThreadingBench : public benchmark::Fixture {
  public:
    // Override Google Benchmark's fixture setup. This runs before the benchmark body and prepares
    // the input data used by the timed loop.
    void SetUp(const benchmark::State &state) override {
        static_cast<void>(state);

        // If the ITCH slice cannot be loaded, record the setup error.
        // The benchmark body will call SkipWithError using this message.
        if (!load_itch_slice(itch_buffer_, setup_error_)) {
            return;
        }

        // Generate a valid in-memory 16-bit mono WAV buffer for the WAV pipeline.
        constexpr std::size_t wav_sample_count = 500000U;
        wav_buffer_ = make_synthetic_wav(wav_sample_count);
    }

  protected:
    std::vector<std::uint8_t> itch_buffer_;
    std::vector<std::uint8_t> wav_buffer_;
    std::string setup_error_;
};

BENCHMARK_DEFINE_F(ThreadingBench, OnePipelineITCH)(benchmark::State &state) {
    if (!setup_error_.empty()) {
        state.SkipWithError(setup_error_.c_str());
        return;
    }

    std::int64_t items_per_iteration = 0;

    for ([[maybe_unused]] auto _ : state) {
        engine::Pipeline itch_pipeline;

        auto vwap = std::make_unique<engine::VWAPOperator>();
        itch_pipeline.add(std::move(vwap));

        engine::ITCHFileSource itch_source{""};

        itch_source.run_buffer(itch_buffer_.data(), itch_buffer_.size(), itch_pipeline);
        const auto itch_messages = itch_source.messages_processed();

        if (itch_messages == 0U) {
            state.SkipWithError("ITCH pipeline processed zero messages");
            return;
        }

        items_per_iteration = static_cast<std::int64_t>(itch_messages);
    }

    state.SetItemsProcessed(state.iterations() * items_per_iteration);
}

BENCHMARK_DEFINE_F(ThreadingBench, OnePipelineWAV)(benchmark::State &state) {
    if (!setup_error_.empty()) {
        state.SkipWithError(setup_error_.c_str());
        return;
    }

    std::int64_t items_per_iteration = 0;

    for ([[maybe_unused]] auto _ : state) {
        engine::Pipeline wav_pipeline;

        auto rms = std::make_unique<engine::RMSOperator>(1024U);
        wav_pipeline.add(std::move(rms));

        engine::WAVFileSource wav_source{""};

        wav_source.run_buffer(wav_buffer_.data(), wav_buffer_.size(), wav_pipeline);
        const auto wav_samples = wav_source.samples_processed();

        if (wav_samples == 0U) {
            state.SkipWithError("WAV pipeline processed zero samples");
            return;
        }

        items_per_iteration = static_cast<std::int64_t>(wav_samples);
    }

    state.SetItemsProcessed(state.iterations() * items_per_iteration);
}

BENCHMARK_DEFINE_F(ThreadingBench, TwoPipelinesSequential)(benchmark::State &state) {
    if (!setup_error_.empty()) {
        state.SkipWithError(setup_error_.c_str());
        return;
    }

    std::int64_t items_per_iteration = 0;

    for ([[maybe_unused]] auto _ : state) {
        engine::Pipeline itch_pipeline;
        auto vwap = std::make_unique<engine::VWAPOperator>();
        itch_pipeline.add(std::move(vwap));

        engine::Pipeline wav_pipeline;
        auto rms = std::make_unique<engine::RMSOperator>(1024U);
        wav_pipeline.add(std::move(rms));

        engine::ITCHFileSource itch_source{""};
        engine::WAVFileSource wav_source{""};

        itch_source.run_buffer(itch_buffer_.data(), itch_buffer_.size(), itch_pipeline);
        const auto itch_messages = itch_source.messages_processed();

        wav_source.run_buffer(wav_buffer_.data(), wav_buffer_.size(), wav_pipeline);
        const auto wav_samples = wav_source.samples_processed();

        if (itch_messages == 0U || wav_samples == 0U) {
            state.SkipWithError("pipeline processed zero events");
            return;
        }

        items_per_iteration = static_cast<std::int64_t>(itch_messages + wav_samples);
    }

    state.SetItemsProcessed(state.iterations() * items_per_iteration);
}

BENCHMARK_DEFINE_F(ThreadingBench, TwoPipelinesParallel)(benchmark::State &state) {
    if (!setup_error_.empty()) {
        state.SkipWithError(setup_error_.c_str());
        return;
    }

    std::int64_t items_per_iteration = 0;

    for ([[maybe_unused]] auto _ : state) {
        std::size_t itch_messages = 0;
        std::size_t wav_samples = 0;

        std::thread itch_thread([&] {
            pin_current_thread_to_core(0);

            engine::Pipeline itch_pipeline;

            auto vwap = std::make_unique<engine::VWAPOperator>();
            itch_pipeline.add(std::move(vwap));

            engine::ITCHFileSource itch_source{""};
            itch_source.run_buffer(itch_buffer_.data(), itch_buffer_.size(), itch_pipeline);

            itch_messages = itch_source.messages_processed();
        });

        std::thread wav_thread([&] {
            pin_current_thread_to_core(2);

            engine::Pipeline wav_pipeline;

            auto rms = std::make_unique<engine::RMSOperator>(1024U);
            wav_pipeline.add(std::move(rms));

            engine::WAVFileSource wav_source{""};
            wav_source.run_buffer(wav_buffer_.data(), wav_buffer_.size(), wav_pipeline);

            wav_samples = wav_source.samples_processed();
        });

        itch_thread.join();
        wav_thread.join();

        if (itch_messages == 0U || wav_samples == 0U) {
            state.SkipWithError("pipeline processed zero events");
            return;
        }

        items_per_iteration = static_cast<std::int64_t>(itch_messages + wav_samples);
    }

    state.SetItemsProcessed(state.iterations() * items_per_iteration);
}

BENCHMARK_REGISTER_F(ThreadingBench, OnePipelineITCH)->Unit(benchmark::kMillisecond)->UseRealTime();

BENCHMARK_REGISTER_F(ThreadingBench, OnePipelineWAV)->Unit(benchmark::kMillisecond)->UseRealTime();

BENCHMARK_REGISTER_F(ThreadingBench, TwoPipelinesSequential)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_REGISTER_F(ThreadingBench, TwoPipelinesParallel)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();
