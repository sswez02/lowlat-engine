add_executable(engine_tests
    test_smoke.cpp
    test_operator.cpp
    test_pipeline.cpp
    test_vwap.cpp
    test_itch_source.cpp
    test_sma.cpp
    test_rms.cpp
    test_wav_source.cpp
)

target_link_libraries(engine_tests
    PRIVATE
        engine
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)
