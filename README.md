# lowlat-engine

[![CI](https://github.com/sswez02/lowlat-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/sswez02/lowlat-engine/actions/workflows/ci.yml)

A generic low-latency C++20 event/stream processing engine.

## Architecture

See [docs/DESIGN.md](docs/DESIGN.md) for the architectural decision records.

## Status

Current features:

- Event-driven pipeline architecture
- Runtime-polymorphic operator chain
- TickEvent and AudioSample event support
- VWAP, SMA, and RMS operators
- NASDAQ ITCH 5.0 Order Executed With Price source
- WAV PCM audio source for 16-bit mono 44.1/48 kHz files
- Unit tests and Google Benchmark microbenchmarks

## Test

From the build directory:

```bash
cd build
ctest --output-on-failure
```

Or from the project root:

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

## Benchmarks

Build first:

```bash
cmake --build build -j
```

Run benchmarks from the project root:

```bash
./build/benchmarks/engine_bench
```

Some ITCH benchmarks require a NASDAQ ITCH input file. Set `LOWLAT_ITCH_FILE` before running:

```bash
export LOWLAT_ITCH_FILE=/data/file.itch
./build/benchmarks/engine_bench
```

A NASDAQ ITCH sample file can be downloaded from the official NASDAQ sample data page:

[Download NASDAQ ITCH sample data](https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/)

If `LOWLAT_ITCH_FILE` is not set, the ITCH file-source benchmarks will be skipped.
