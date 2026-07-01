cmake_minimum_required(VERSION 3.20)
project(map-renderer-v2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Unit Tests ───────────────────────────────────────────────────────────
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
endif()

enable_testing()

# Test executable
add_executable(map-renderer-tests
    tests/test_main.cpp
    tests/test_json.cpp
    tests/test_style.cpp
    tests/test_mvt.cpp
    tests/test_render.cpp
    tests/test_osm.cpp
    tests/test_building.cpp
    tests/test_renderer.cpp
    tests/test_integration.cpp
)

target_include_directories(map-renderer-tests PRIVATE src/)
target_link_libraries(map-renderer-tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    protobuf::libprotobuf
    ${GLM_LIBRARIES}
)

target_compile_definitions(map-renderer-tests PRIVATE MAP_RENDERER_DEBUG)
target_compile_options(map-renderer-tests PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

add_test(NAME all_tests COMMAND map-renderer-tests)

# Python tests
add_test(NAME python_tests
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/test_python_extractor.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(GLM QUIET glm)
if(NOT GLM_FOUND)
    find_package(glm CONFIG REQUIRED)
    set(GLM_INCLUDE_DIRS "/usr/include")
endif()
find_package(protobuf CONFIG REQUIRED)

# Shader compilation
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

function(add_shader TARGET SHADER_SRC SHADER_DST)
    set(SHADER_FULL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SRC})
    get_filename_component(SHADER_DST_DIR ${SHADER_DST} DIRECTORY)
    add_custom_command(
        OUTPUT ${SHADER_DST}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DST_DIR}
        COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_FULL_SRC} -o ${SHADER_DST}
        DEPENDS ${SHADER_FULL_SRC}
        COMMENT "Compiling shader: ${SHADER_SRC}"
    )
    target_sources(${TARGET} PRIVATE ${SHADER_DST})
endfunction()

# Main executable
add_executable(map-renderer
    src/main.cpp
)

target_include_directories(map-renderer PRIVATE
    src/
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(map-renderer PRIVATE
    ${VULKAN_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${GLM_LIBRARIES}
    protobuf::libprotobuf
)

# MVT parser test executable (milestone 2)
add_executable(test_mvt
    src/test_mvt.cpp
)

target_include_directories(test_mvt PRIVATE src/)

target_link_libraries(test_mvt PRIVATE
    protobuf::libprotobuf
)

target_compile_options(test_mvt PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

# Shaders
add_shader(map-renderer src/shaders/triangle.vert src/shaders/triangle.vert.spv)
add_shader(map-renderer src/shaders/triangle.frag src/shaders/triangle.frag.spv)
add_shader(map-renderer src/shaders/line.vert     src/shaders/line.vert.spv)
add_shader(map-renderer src/shaders/line.frag     src/shaders/line.frag.spv)
add_shader(map-renderer src/shaders/fill.vert     src/shaders/fill.vert.spv)
add_shader(map-renderer src/shaders/fill.frag     src/shaders/fill.frag.spv)
add_shader(map-renderer src/shaders/building.vert src/shaders/building.vert.spv)
add_shader(map-renderer src/shaders/building.frag src/shaders/building.frag.spv)

# Compiler flags
target_compile_definitions(map-renderer PRIVATE MAP_RENDERER_DEBUG)
target_compile_options(map-renderer PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Minimal test executable
add_executable(minimal_test
    tests/minimal_test.cpp
)

target_include_directories(minimal_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(minimal_test PRIVATE
    ${SDL2_LIBRARIES}
)

target_compile_options(minimal_test PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Minimal Vulkan test executable
add_executable(minimal_vulkan
    tests/minimal_vulkan.cpp
)

target_include_directories(minimal_vulkan PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(minimal_vulkan PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(minimal_vulkan PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Step-by-step Vulkan test
add_executable(step_vulkan
    tests/step_vulkan.cpp
)

target_include_directories(step_vulkan PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(step_vulkan PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(step_vulkan PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Minimal window test
add_executable(minimal_window
    tests/minimal_window.cpp
)

target_include_directories(minimal_window PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(minimal_window PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(minimal_window PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Building test
add_executable(building_test
    tests/building_test.cpp
)

target_include_directories(building_test PRIVATE
    src/
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(building_test PRIVATE
    ${VULKAN_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${GLM_LIBRARIES}
)

target_compile_options(building_test PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Working triangle test
add_executable(working_triangle
    tests/working_triangle.cpp
)

target_include_directories(working_triangle PRIVATE
    src/
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(working_triangle PRIVATE
    ${VULKAN_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${GLM_LIBRARIES}
)

target_compile_options(working_triangle PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Simple triangle test
add_executable(simple_triangle
    tests/simple_triangle.cpp
)

target_include_directories(simple_triangle PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(simple_triangle PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(simple_triangle PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Triangle test
add_executable(triangle_test
    tests/triangle_test.cpp
)

target_include_directories(triangle_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(triangle_test PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(triangle_test PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

target_include_directories(step_vulkan PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(step_vulkan PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(step_vulkan PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

target_include_directories(minimal_vulkan PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(minimal_vulkan PRIVATE
    ${SDL2_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

target_compile_options(minimal_vulkan PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

target_include_directories(minimal_test PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(minimal_test PRIVATE
    ${SDL2_LIBRARIES}
)

target_compile_options(minimal_test PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Triangle test executable
add_executable(test_triangle
    tests/test_triangle.cpp
)

target_include_directories(test_triangle PRIVATE
    src/
    ${VULKAN_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(test_triangle PRIVATE
    ${VULKAN_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${GLM_LIBRARIES}
)

target_compile_options(test_triangle PRIVATE
    -Wall -Wextra -Wpedantic
    -O2
)

# Copy data files to build directory
add_custom_command(TARGET map-renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:map-renderer>/data
    COMMENT "Copying data files to build directory"
)
