# -----------------------------------------------------------------------------
# Core library: contains all decoding logic, knows nothing about Qt or the CLI.
# -----------------------------------------------------------------------------
add_library(sl_texture_decoder_core STATIC
    VersionInfo.cpp
    cache/CacheLayout.cpp
    cache/TextureEntries.cpp
    utils/UUID.cpp
    utils/FileUtils.cpp
    utils/Logger.cpp
)

target_include_directories(sl_texture_decoder_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(sl_texture_decoder_core
    PUBLIC
        ${SLTCD_OPENJPEG_TARGET}
        PNG::PNG
)

set_target_properties(sl_texture_decoder_core PROPERTIES
    OUTPUT_NAME sl_texture_decoder_core
)

# -----------------------------------------------------------------------------
# CLI frontend
# -----------------------------------------------------------------------------
add_executable(sl_texture_decoder
    main.cpp
)

target_link_libraries(sl_texture_decoder
    PRIVATE sl_texture_decoder_core
)

set_target_properties(sl_texture_decoder PROPERTIES
    OUTPUT_NAME SLTextureDecoder
)
