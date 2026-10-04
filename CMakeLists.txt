# pgr3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(pgr3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug aid: PGR3_FP_TRAP_MS=<ms> logs where NaNs are first produced (see src/crash_log.cpp).
option(PGR3_FP_TRAP "Build with the invalid-operation FP trap debug aid" OFF)
if(PGR3_FP_TRAP)
    add_compile_definitions(REX_DEBUG_FP_TRAP)
endif()

include(generated/rexglue.cmake)

# Sources
set(PGR3_SOURCES
    src/main.cpp
    src/crash_log.cpp
    src/test_harness.cpp
    src/launcher.cpp
)

if(WIN32)
    add_executable(pgr3 WIN32 ${PGR3_SOURCES})
else()
    add_executable(pgr3 ${PGR3_SOURCES})
endif()

rexglue_setup_target(pgr3 GPU_PLUGINS xenos)
if(WIN32)
    target_link_libraries(pgr3 PRIVATE comctl32 ole32 shell32)
    target_link_options(pgr3 PRIVATE "LINKER:/MAP:${CMAKE_BINARY_DIR}/pgr3.map")
endif()
