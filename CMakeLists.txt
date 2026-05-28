cmake_minimum_required(VERSION 3.20)
project(lowlat_engine
    VERSION 0.1.0
    DESCRIPTION "Low-latency C++ event/stream processing engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Warnings as a baseline
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)

# add_subdirectory(engine)
# add_subdirectory(pipelines)
# add_subdirectory(tests)
# add_subdirectory(benchmarks)