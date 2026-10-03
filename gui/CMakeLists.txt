# -----------------------------------------------------------------------------
# Qt 6 desktop frontend. Drives sltcd::batch::run() on a worker thread and shows
# progress, log lines and the decoded PNG files.
# -----------------------------------------------------------------------------
add_executable(sl_texture_decoder_gui WIN32
    BatchWorker.cpp
    BatchWorker.h
    MainWindow.cpp
    MainWindow.h
    main.cpp
)

target_include_directories(sl_texture_decoder_gui
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(sl_texture_decoder_gui
    PRIVATE
        sl_texture_decoder_core
        Qt6::Widgets
)

set_target_properties(sl_texture_decoder_gui PROPERTIES
    OUTPUT_NAME SLTextureDecoderGUI
)
