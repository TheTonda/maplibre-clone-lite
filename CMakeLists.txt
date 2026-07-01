cmake_minimum_required(VERSION 3.20)
project(map-renderer VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ Standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler Flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -DMAP_RENDERER_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ---------------------------------------------------------------------------
# Output Directory
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Dependencies – System Packages
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

# Protobuf via pkg-config (correctly brings in all absl transitive deps)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
pkg_check_modules(ABSL_DEPS    REQUIRED IMPORTED_TARGET protobuf)

find_package(GTest REQUIRED)

# ---------------------------------------------------------------------------
# Dependencies – Header-Only (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Protobuf Code Generation (using protoc from system)
# ---------------------------------------------------------------------------
find_program(PROTOC protoc REQUIRED)
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/osm_data.proto)
set(PROTO_SRC  ${CMAKE_CURRENT_BINARY_DIR}/osm_data.pb.cc)
set(PROTO_HDR  ${CMAKE_CURRENT_BINARY_DIR}/osm_data.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRC} ${PROTO_HDR}
  COMMAND ${PROTOC}
    --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/src/proto
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
    ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating protobuf sources..."
)

# ---------------------------------------------------------------------------
# Core Library
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_SRCS
  src/core/*.cpp
  src/data/*.cpp
  src/graphics/*.cpp
  src/render/*.cpp
  ${PROTO_SRC}
)
add_library(map-renderer-lib SHARED ${CORE_SRCS} ${PROTO_HDR})
target_include_directories(map-renderer-lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
)
target_link_libraries(map-renderer-lib
  PUBLIC
    Vulkan::Vulkan
    SDL2::SDL2
    glm::glm
    PkgConfig::PROTOBUF
    nlohmann_json::nlohmann_json
)
target_compile_features(map-renderer-lib PUBLIC cxx_std_23)

# ---------------------------------------------------------------------------
# Main Executable
# ---------------------------------------------------------------------------
add_executable(map-renderer src/main.cpp)
target_link_libraries(map-renderer PRIVATE map-renderer-lib)
target_compile_features(map-renderer PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# Test Executable
# ---------------------------------------------------------------------------
file(GLOB_RECURSE TEST_SRCS tests/*.cpp)
add_executable(map-renderer-tests ${TEST_SRCS})
target_link_libraries(map-renderer-tests
  PRIVATE
    map-renderer-lib
    GTest::gtest_main
    PkgConfig::ABSL_DEPS
)
target_compile_features(map-renderer-tests PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# CTest Integration
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME map-renderer-tests COMMAND map-renderer-tests)
