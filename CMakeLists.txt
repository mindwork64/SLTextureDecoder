cmake_minimum_required(VERSION 3.16)

project(SLTextureDecoder
    VERSION 0.1.0
    DESCRIPTION "Second Life texture cache decoder (JPEG2000 codestream -> PNG)"
    LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Global build settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
find_package(OpenJPEG 2.5 REQUIRED)
find_package(PNG 1.6 REQUIRED)

# OpenJPEG ships an exported target named "openjp2" (config package), while
# CMake's built-in FindOpenJPEG module provides "OpenJPEG::OpenJPEG".
# Normalise both cases into a single variable used by the targets below.
if(TARGET openjp2)
    set(SLTCD_OPENJPEG_TARGET openjp2)
elseif(TARGET OpenJPEG::OpenJPEG)
    set(SLTCD_OPENJPEG_TARGET OpenJPEG::OpenJPEG)
else()
    message(FATAL_ERROR "OpenJPEG was found but provides no usable target")
endif()

message(STATUS "OpenJPEG target : ${SLTCD_OPENJPEG_TARGET}")
message(STATUS "OpenJPEG headers: ${OPENJPEG_INCLUDE_DIRS}")

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
add_subdirectory(src)

# -----------------------------------------------------------------------------
# Optional Qt 6 GUI frontend
# -----------------------------------------------------------------------------
option(SLTCD_BUILD_GUI "Build the Qt 6 GUI frontend (needs Qt6Widgets)" ON)

if(SLTCD_BUILD_GUI)
    find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_subdirectory(gui)
    else()
        message(STATUS "Qt6 Widgets not found - the GUI frontend is skipped "
                       "(set SLTCD_BUILD_GUI=OFF to silence this)")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
