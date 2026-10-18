if (WIN32)
set(SYSTEM_DEPS
    "ddraw"
    "amstrmid"
)
endif()

STORM_SETUP(
    TARGET_NAME xinterface
    TYPE storm_module
    DEPENDENCIES animation core geometry model renderer util ${SYSTEM_DEPS}
    TEST_DEPENDENCIES Catch2::Catch2WithMain
)
if (APPLE)
    # video: AVFoundation (Objective-C++)
    target_sources(xinterface PRIVATE src/aviplayer/apple_video.mm)
    set_source_files_properties(src/aviplayer/apple_video.mm PROPERTIES
                                LANGUAGE CXX COMPILE_OPTIONS "-xobjective-c++;-fobjc-arc")
    target_link_libraries(xinterface PRIVATE "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo"
                          "-framework QuartzCore" "-framework Foundation")
endif()
