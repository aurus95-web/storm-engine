if (NOT TARGET fmod::fmod OR (NOT STORM_USE_CONAN AND NOT STORM_FMOD_DIR AND NOT STORM_SOUND_MINIAUDIO))
    # no FMOD SDK: only the interface headers are provided and the engine runs without sound
    add_library(sound_service INTERFACE)
    target_include_directories(sound_service INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    return()
endif()

STORM_SETUP(
    TARGET_NAME sound_service
    TYPE storm_module
    DEPENDENCIES core fmod::fmod renderer editor
)
