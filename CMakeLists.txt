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
find_package(Protobuf REQUIRED)
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
# Protobuf Code Generation
# ---------------------------------------------------------------------------
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/proto/osm_data.proto)

# ---------------------------------------------------------------------------
# Shader Compilation
# ---------------------------------------------------------------------------
# Shaders will be compiled by a helper script called from CMake when the
# shader files exist.  For now this section is a placeholder.
file(GLOB SHADER_SRCS src/shaders/2d/*.vert src/shaders/2d/*.frag
                      src/shaders/3d/*.vert src/shaders/3d/*.frag)

# ---------------------------------------------------------------------------
# Core Library
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_SRCS
  src/core/*.cpp
  src/data/*.cpp
  src/graphics/*.cpp
  ${PROTO_SRCS}
)
add_library(map-renderer-lib STATIC ${CORE_SRCS} ${PROTO_HDRS})
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
    protobuf::libprotobuf
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
)
target_compile_features(map-renderer-tests PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# CTest Integration
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME map-renderer-tests COMMAND map-renderer-tests)
