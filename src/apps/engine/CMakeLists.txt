# this is something I really don't like but it seems there's not better way to ensure mimalloc is linked before c-runtime
#include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
#if (WIN32)
#  set(MIMALLOC_DEP "$<IF:$<CONFIG:Release>,${CONAN_LIB_DIRS_MIMALLOC_RELEASE}/mimalloc.lib,${CONAN_LIB_DIRS_MIMALLOC_DEBUG}/mimalloc-debug.lib>")
#else()
#  set(MIMALLOC_DEP "$<IF:$<CONFIG:Release>,${CONAN_LIB_DIRS_MIMALLOC_RELEASE}/libmimalloc.so,${CONAN_LIB_DIRS_MIMALLOC_DEBUG}/libmimalloc-debug.so>")
#endif()

if (WIN32)
set(SYSTEM_DEPS
    "comctl32"
    "dbghelp"
    "winhttp"
)
elseif (NOT APPLE)
set(SYSTEM_DEPS "ffi")
endif()

STORM_SETUP(
    TARGET_NAME engine
    TYPE executable
    DEPENDENCIES

    ${MIMALLOC_DEP}

    # internal
    core
    diagnostics
    util
    window

    # external
    mimalloc
    sentry::sentry
    ${SDL2_LIBRARIES}
    zlib
    CLI11::CLI11

    # system
    ${SYSTEM_DEPS}
)

add_dependencies(engine engine_techniques)

if (APPLE)
    storm_apple_bundle_runtime(engine)
endif()

if (IOS)
    set(STORM_IOS_BUNDLE_ID "com.example.stormengine" CACHE STRING "iOS bundle identifier (must be unique for your Apple ID)")
    set_target_properties(engine PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${STORM_IOS_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "Storm Engine"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${STORM_IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    )
endif()
