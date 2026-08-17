cmake_minimum_required(VERSION 3.16)

project(XenonDS VERSION 0.4.1 LANGUAGES CXX)

option(XENONDS_BUILD_TESTS "Build XenonDS host tests" ON)
option(XENONDS_BUILD_TOOLS "Build XenonDS host tools" ON)
option(XENONDS_WITH_DESMUME "Build the experimental DeSmuME adapter" OFF)

add_library(xenonds_core
    src/checksum.cpp
    src/controller_mapper.cpp
    src/nds_header.cpp
    src/screen_layout.cpp
    src/session.cpp
    src/video_compositor.cpp
)

target_include_directories(xenonds_core PUBLIC include)
target_compile_features(xenonds_core PUBLIC cxx_std_11)

if(MSVC)
    target_compile_options(xenonds_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(xenonds_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(XENONDS_WITH_DESMUME)
    if(NOT DEFINED DESMUME_SOURCE_DIR)
        message(FATAL_ERROR "Set DESMUME_SOURCE_DIR to the DeSmuME desmume/src directory")
    endif()

    add_library(xenonds_desmume_adapter src/backends/desmume_backend.cpp)
    target_include_directories(xenonds_desmume_adapter
        PUBLIC include
        PRIVATE
            "${DESMUME_SOURCE_DIR}"
            "${DESMUME_SOURCE_DIR}/libretro-common/include"
    )
    target_link_libraries(xenonds_desmume_adapter PUBLIC xenonds_core)
    target_compile_features(xenonds_desmume_adapter PUBLIC cxx_std_11)
endif()

if(XENONDS_BUILD_TOOLS)
    add_executable(xenonds-rominfo tools/rominfo.cpp)
    target_link_libraries(xenonds-rominfo PRIVATE xenonds_core)

    add_executable(xenonds-layout-demo tools/layout_demo.cpp)
    target_link_libraries(xenonds-layout-demo PRIVATE xenonds_core)
endif()

if(XENONDS_BUILD_TESTS)
    enable_testing()
    add_executable(xenonds-tests tests/test_main.cpp)
    target_link_libraries(xenonds-tests PRIVATE xenonds_core)
    add_test(NAME xenonds-tests COMMAND xenonds-tests)
endif()
