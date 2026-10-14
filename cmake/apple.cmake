# macOS and iOS: D3D9 is provided by DXVK (D3D9 -> Vulkan) running on top of
# MoltenVK (Vulkan -> Metal).
#
# Host requirements (Homebrew): meson ninja glslang pkg-config
# MoltenVK: the LunarG Vulkan SDK (VULKAN_SDK environment variable) or `brew install molten-vk`.
#
# DXVK is the fork used by the GeneralsX macOS/iOS port, which already runs D3D8/9 games
# on MoltenVK. cmake/dxvk-apple.patch makes it find MoltenVK and SDL2 inside the app bundle
# and size the swapchain in pixels on high-DPI displays.

include(ExternalProject)

set(STORM_DXVK_REPOSITORY "https://github.com/fbraz3/dxvk.git" CACHE STRING "DXVK fork with macOS support")
set(STORM_DXVK_TAG "870371520577d3781690f167ec33c45a0f17fd43" CACHE STRING "DXVK commit (branch generalsx-macos-v2.6)")
set(STORM_MOLTENVK_PATH "" CACHE FILEPATH
    "MoltenVK to bundle: libMoltenVK.dylib (macOS) or MoltenVK.framework (iOS). Searched in the Vulkan SDK and Homebrew when empty")

# keep in sync with CMakePresets.json
if (IOS)
    set(storm_min_version 16.0)
    set(STORM_APPLE_MIN_VERSION_FLAG "-miphoneos-version-min=${storm_min_version}")
else()
    set(storm_min_version 12.0)
    set(STORM_APPLE_MIN_VERSION_FLAG "-mmacosx-version-min=${storm_min_version}")
endif()
if (CMAKE_OSX_DEPLOYMENT_TARGET)
    string(REPLACE "${storm_min_version}" "${CMAKE_OSX_DEPLOYMENT_TARGET}" STORM_APPLE_MIN_VERSION_FLAG "${STORM_APPLE_MIN_VERSION_FLAG}")
    set(storm_min_version ${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

# runtime libraries with the names other libraries load them by
set(STORM_APPLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/apple-runtime")
file(MAKE_DIRECTORY ${STORM_APPLE_RUNTIME_DIR})

find_program(MESON_EXECUTABLE meson REQUIRED)
find_program(NINJA_EXECUTABLE ninja REQUIRED)
find_program(GLSLANG_EXECUTABLE NAMES glslangValidator glslang REQUIRED)

### MoltenVK
if (NOT STORM_MOLTENVK_PATH)
    if (IOS)
        find_path(moltenvk_xcframework MoltenVK.xcframework
            PATHS "$ENV{VULKAN_SDK}/../iOS/lib" "$ENV{VULKAN_SDK}/../MoltenVK/dynamic" "$ENV{VULKAN_SDK}/MoltenVK/dynamic"
            NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        if (moltenvk_xcframework)
            set(STORM_MOLTENVK_PATH "${moltenvk_xcframework}/MoltenVK.xcframework/ios-arm64/MoltenVK.framework")
        endif()
    else()
        find_library(moltenvk_dylib NAMES MoltenVK
            PATHS "$ENV{VULKAN_SDK}/lib" /opt/homebrew/lib /usr/local/lib
            NO_CMAKE_FIND_ROOT_PATH)
        set(STORM_MOLTENVK_PATH "${moltenvk_dylib}")
    endif()
endif()
if (NOT STORM_MOLTENVK_PATH OR NOT EXISTS "${STORM_MOLTENVK_PATH}")
    message(FATAL_ERROR "MoltenVK not found: install the Vulkan SDK (and set VULKAN_SDK) or `brew install molten-vk`, "
                        "or set STORM_MOLTENVK_PATH")
endif()
message(STATUS "MoltenVK: ${STORM_MOLTENVK_PATH}")

### DXVK
# DXVK only needs the SDL2 headers: it loads SDL2 at runtime with dlopen
set(dxvk_pkgconfig_dir "${CMAKE_BINARY_DIR}/dxvk-pkgconfig")
file(MAKE_DIRECTORY ${dxvk_pkgconfig_dir})
file(WRITE "${dxvk_pkgconfig_dir}/sdl2.pc"
"Name: sdl2
Description: SDL2 headers for DXVK
Version: 2.32.2
Cflags: -I${sdl2_SOURCE_DIR}/include -I${sdl2_BINARY_DIR}/include -I${sdl2_BINARY_DIR}/include/SDL2
")

if (IOS)
    execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
        OUTPUT_VARIABLE storm_apple_sdk OUTPUT_STRIP_TRAILING_WHITESPACE)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/meson-ios-cross.ini.in ${CMAKE_BINARY_DIR}/meson-ios-cross.ini @ONLY)
    set(dxvk_machine_file --cross-file ${CMAKE_BINARY_DIR}/meson-ios-cross.ini)
else()
    set(dxvk_machine_file "")
endif()

set(dxvk_flags "-arch arm64 ${STORM_APPLE_MIN_VERSION_FLAG}")
ExternalProject_Add(dxvk
    GIT_REPOSITORY    ${STORM_DXVK_REPOSITORY}
    GIT_TAG           ${STORM_DXVK_TAG}
    GIT_SUBMODULES_RECURSE ON
    PATCH_COMMAND     git apply --whitespace=nowarn ${CMAKE_CURRENT_LIST_DIR}/dxvk-apple.patch
    UPDATE_DISCONNECTED ON
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                          CC=clang CXX=clang++
                          "CFLAGS=${dxvk_flags}" "CXXFLAGS=${dxvk_flags}" "LDFLAGS=${dxvk_flags}"
                          PKG_CONFIG_PATH=${dxvk_pkgconfig_dir}
                      ${MESON_EXECUTABLE} setup <BINARY_DIR> <SOURCE_DIR> ${dxvk_machine_file}
                          --buildtype=release
                          -Denable_d3d9=true -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false -Denable_dxgi=false
                          -Ddxvk_native_wsi=sdl2
    # only d3d9 is enabled; the versioned library gets an unversioned symlink at build time
    BUILD_COMMAND     ${NINJA_EXECUTABLE} -C <BINARY_DIR>
    # meson names the library with its ABI version, the copy gets a stable name and install name
    INSTALL_COMMAND   ${CMAKE_COMMAND} -E copy <BINARY_DIR>/src/d3d9/libdxvk_d3d9.dylib ${STORM_APPLE_RUNTIME_DIR}/libdxvk_d3d9.dylib
              COMMAND install_name_tool -id @rpath/libdxvk_d3d9.dylib ${STORM_APPLE_RUNTIME_DIR}/libdxvk_d3d9.dylib
    BUILD_BYPRODUCTS  ${STORM_APPLE_RUNTIME_DIR}/libdxvk_d3d9.dylib
)
ExternalProject_Get_property(dxvk SOURCE_DIR)
set(STORM_DXVK_LIBRARY "${STORM_APPLE_RUNTIME_DIR}/libdxvk_d3d9.dylib")

# DXVK provides the D3D9 and Windows type headers on non-Windows platforms
set(DXVK_NATIVE_INCLUDE_DIRS
    "${SOURCE_DIR}/include/native/directx"
    "${SOURCE_DIR}/include/native/windows"
)
include_directories(${DXVK_NATIVE_INCLUDE_DIRS})
add_custom_target(dependencies ALL DEPENDS dxvk)

add_library(dxvk_d3d9 SHARED IMPORTED GLOBAL)
set_target_properties(dxvk_d3d9 PROPERTIES IMPORTED_LOCATION "${STORM_DXVK_LIBRARY}")
add_dependencies(dxvk_d3d9 dxvk)
set(NATIVE_D3D9_LIBS dxvk_d3d9)

# DXVK loads SDL2 by its soname (see cmake/dxvk-apple.patch)
set(STORM_SDL_LIBRARY "${STORM_APPLE_RUNTIME_DIR}/libSDL2-2.0.0.dylib")
add_custom_command(OUTPUT ${STORM_SDL_LIBRARY}
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_SONAME_FILE:SDL2::SDL2>" "${STORM_SDL_LIBRARY}"
    DEPENDS SDL2
)
add_custom_target(apple_runtime ALL DEPENDS ${STORM_SDL_LIBRARY} dxvk)

# Copies the runtime libraries next to the executable (macOS) or into the app bundle (iOS)
function(storm_apple_bundle_runtime target)
    add_dependencies(${target} apple_runtime)
    if (IOS)
        set_target_properties(${target} PROPERTIES
            XCODE_EMBED_FRAMEWORKS "${STORM_MOLTENVK_PATH};${STORM_DXVK_LIBRARY};${STORM_SDL_LIBRARY}"
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
            XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON
            XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        )
    else()
        set_target_properties(${target} PROPERTIES BUILD_RPATH "@executable_path")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${STORM_MOLTENVK_PATH}" "$<TARGET_FILE_DIR:${target}>/libMoltenVK.dylib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${STORM_DXVK_LIBRARY}" "${STORM_SDL_LIBRARY}" "$<TARGET_FILE_DIR:${target}>/"
            COMMENT "Copying MoltenVK, DXVK and SDL2 next to ${target}"
        )
    endif()
endfunction()
