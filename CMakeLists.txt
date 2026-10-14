cmake_minimum_required(VERSION 3.18)

# Conan packages are only available for Windows and Linux, Apple platforms use cmake/dependencies.cmake
if (CMAKE_HOST_APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(storm_use_conan_default OFF)
else()
    set(storm_use_conan_default ON)
endif()
option(STORM_USE_CONAN "Use Conan for third-party dependencies" ${storm_use_conan_default})

if (STORM_USE_CONAN)
    list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/conan_provider.cmake")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(storm-engine)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(StormSetup)

option(STORM_ENABLE_CRASH_REPORTS "Enable automatic crash reports" OFF)
option(STORM_ENABLE_STEAM "Enable Steam integration" OFF)
option(STORM_ENABLE_SAFE_MODE "Enable additional runtime checks" OFF)
option(STORM_USE_CONAN_SDL "Use sdl from conan" ${STORM_USE_CONAN})
option(STORM_USE_SENTRY "Use sentry-native for crash reports" ${STORM_USE_CONAN})
option(STORM_USE_MIMALLOC "Use mimalloc as the memory allocator" ${STORM_USE_CONAN})
if (CMAKE_CROSSCOMPILING)
    set(storm_build_tests_default OFF)
else()
    set(storm_build_tests_default ON)
endif()
option(STORM_BUILD_TESTS "Build and run unit tests" ${storm_build_tests_default})
if (NOT WIN32 AND NOT APPLE)
    option(STORM_MESA_NINE "Use Gallium Nine from Mesa for native D3D9 API" OFF)
endif()
if (NOT STORM_USE_CONAN)
    option(STORM_FETCH_SDL "Build SDL2 from source instead of using the system one" ${APPLE})
    set(STORM_FMOD_DIR "" CACHE PATH "Path to the FMOD Engine SDK (contains api/core), sound is disabled when empty")
endif()

### Set up output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug)

normalize_booleans(STORM_ENABLE_CRASH_REPORTS STORM_ENABLE_STEAM STORM_USE_CONAN_SDL)

set(STORM_WATERMARK_FILE ${CMAKE_CURRENT_BINARY_DIR}/watermark.hpp CACHE FILEPATH "Include file containing build revision, etc." FORCE)

if (WIN32)
    set(SDL2_LIBRARIES "sdl")
elseif (APPLE)
    include(apple)
else()
    include(linux)
endif()

set(CONAN_INSTALL_ARGS ${CONAN_INSTALL_ARGS}
        "-o output_directory=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        "-o watermark_file=${STORM_WATERMARK_FILE}"
        "-o crash_reports=${STORM_ENABLE_CRASH_REPORTS}"
        "-o steam=${STORM_ENABLE_STEAM}"
        "-o conan_sdl=${STORM_USE_CONAN_SDL}"
        "-c tools.cmake:cmake_program=${CMAKE_COMMAND}"
)

if (STORM_USE_CONAN)
conan_add_remote(NAME piratesahoy
    URL https://gitlab.com/api/v4/projects/54068744/packages/conan
    VERIFY_SSL True
)

find_package(zlib REQUIRED)
find_package(spdlog REQUIRED)
find_package(FastFloat REQUIRED)
find_package(mimalloc REQUIRED)
find_package(sentry REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cli11 REQUIRED)
find_package(directx REQUIRED)
find_package(fmod REQUIRED)
find_package(Microsoft.GSL REQUIRED)
find_package(ImGui REQUIRED)
find_package(SDL2 REQUIRED)

find_package(Catch2 REQUIRED)
else()
    include(dependencies)
endif()

if (STORM_USE_SENTRY)
    add_compile_definitions(STORM_USE_SENTRY=1)
endif()
if (STORM_USE_MIMALLOC)
    add_compile_definitions(STORM_USE_MIMALLOC=1)
endif()

# modules that can not be built in the current configuration
set(STORM_SKIP_MODULES "" CACHE INTERNAL "")
if (NOT STORM_USE_CONAN AND NOT STORM_FMOD_DIR)
    message(WARNING "STORM_FMOD_DIR is not set, building without sound")
endif()
if (NOT WIN32)
    # the configuration tool is Windows-only (uses the D3D9 ImGui backend and HWND)
    list(APPEND STORM_SKIP_MODULES configapp)
endif()

### Define library ALIASes for use without CONAN_PKG:: prefix
foreach (conan_target ${CONAN_TARGETS})
    string(REPLACE "CONAN_PKG::" "" unprefixed_target ${conan_target})
	add_library(${unprefixed_target} ALIAS ${conan_target})
endforeach()
###

### Set up build scripts
set(SRC_DIRS "src" CACHE STRING "File locations for public header files" FORCE)
set(PUBLIC_INCLUDE_DIRS "include" CACHE STRING "File locations for source files" FORCE)
set(TESTSUITE_DIRS "testsuite" CACHE STRING "File locations for tests" FORCE)
set(RESOURCE_DIRS "rsrc" CACHE STRING "File locations for tests" FORCE)

set(SOURCE_FILE_EXTENSIONS "*.cpp" CACHE STRING "File extensions for source files" FORCE)
set(HEADER_FILE_EXTENSIONS "*.h" "*.hpp" CACHE STRING "File extensions for header files" FORCE)
###

### Copy Shader Effects
file(GLOB_RECURSE glob_techniques LIST_DIRECTORIES false RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src" "src/techniques/**/*.fx")
set(technique_files "")
foreach(effect_file ${glob_techniques})
#    message("${CMAKE_CURRENT_SOURCE_DIR}/src/${effect_file} -> ${CMAKE_CURRENT_BINARY_DIR}/bin/resource/${effect_file}")
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bin/resource/${effect_file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/src/${effect_file}
            ${CMAKE_CURRENT_BINARY_DIR}/bin/resource/${effect_file}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${effect_file}
    )
    list(APPEND technique_files ${CMAKE_CURRENT_BINARY_DIR}/bin/resource/${effect_file})
endforeach()
add_custom_target(engine_techniques DEPENDS ${technique_files})
###

add_subdirectory(src)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT engine)
