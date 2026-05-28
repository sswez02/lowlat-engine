# lowlat-engine

A generic low-latency C++20 event/stream processing engine.

## Architecture

See [docs/DESIGN.md](docs/DESIGN.md) for the architectural decision records.

## Status

Week 1: repo scaffolding.

## Build

cmake -S . -B build
cmake --build build -j

Requires CMake 3.20+ and a C++20 compiler. Developed on GCC 13, WSL2 Ubuntu 24.04.
