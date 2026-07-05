cmake_minimum_required(VERSION 3.16)
project(maprender_v2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fvisibility=hidden)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MR_BUILD_TESTS "Build engine tests and smoke tools" ON)
option(MR_BUILD_DESKTOP "Build FLTK desktop viewer" ON)
option(MR_BUILD_COMPILER "Build mapbake compiler" ON)

add_subdirectory(engine)

if(MR_BUILD_COMPILER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compiler/CMakeLists.txt)
    add_subdirectory(compiler)
endif()

if(MR_BUILD_DESKTOP AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/desktop/CMakeLists.txt)
    add_subdirectory(desktop)
endif()

enable_testing()