# Applies PATCH to the git checkout in the current directory unless it is already applied.
#
# ExternalProject runs the patch step once per configuration with multi-config generators
# (Ninja Multi-Config, Xcode), but all configurations share one source directory.
#
# Usage: cmake -DPATCH=<file> -P apply-patch.cmake

execute_process(COMMAND git apply --reverse --check --whitespace=nowarn ${PATCH}
    RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if (already_applied EQUAL 0)
    return()
endif()

execute_process(COMMAND git apply --whitespace=nowarn ${PATCH} RESULT_VARIABLE result)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to apply ${PATCH}")
endif()
