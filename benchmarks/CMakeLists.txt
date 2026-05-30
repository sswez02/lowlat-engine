add_executable(engine_bench
    bench_smoke.cpp
)

target_link_libraries(engine_bench
    PRIVATE
        benchmark::benchmark
        benchmark::benchmark_main
)
