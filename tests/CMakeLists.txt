find_package(GTest REQUIRED)

add_executable(sl_texture_decoder_tests
    test_batch_converter.cpp
    test_cache_layout.cpp
    test_cli_options.cpp
    test_component_converter.cpp
    test_errors.cpp
    test_interrupt.cpp
    test_jpeg2000_decoder.cpp
    test_png_writer.cpp
    test_runner.cpp
    test_texture_assembler.cpp
    test_texture_cache_reader.cpp
    test_texture_entries.cpp
    test_uuid.cpp
    test_version_info.cpp
)

target_compile_definitions(sl_texture_decoder_tests
    PRIVATE SLTCD_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(sl_texture_decoder_tests
    PRIVATE
        sl_texture_decoder_core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(sl_texture_decoder_tests)
