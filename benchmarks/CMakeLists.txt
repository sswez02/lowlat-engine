add_executable(engine_bench
    bench_smoke.cpp
    bench_operator.cpp
    bench_pipeline.cpp
    bench_vwap.cpp
    bench_itch_file.cpp
)

target_link_libraries(engine_bench
    PRIVATE
        engine
        benchmark::benchmark
        benchmark::benchmark_main
)
