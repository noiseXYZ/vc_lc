cmake_minimum_required(VERSION 3.20)

# static CRT: players don't need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
cmake_policy(SET CMP0091 NEW)

project(vc_lc LANGUAGES C CXX VERSION 1.0.0)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "GTA:SA is 32-bit: configure with -A Win32 (or a 32-bit developer prompt)")
endif ()

include(FetchContent)
FetchContent_Declare(
    kthook
    GIT_REPOSITORY https://github.com/kin4stat/kthook.git
    GIT_TAG 4331456543f408c42122ced22144cfb698031266
)
FetchContent_MakeAvailable(kthook)

add_library(vc_lc SHARED src/vc_lc.cpp)
target_compile_features(vc_lc PRIVATE cxx_std_20)
target_compile_definitions(vc_lc PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX VC_LC_VERSION="${PROJECT_VERSION}")
target_link_libraries(vc_lc PRIVATE kthook windowscodecs ole32)
set_target_properties(vc_lc PROPERTIES SUFFIX ".asi")
