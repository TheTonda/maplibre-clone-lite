cmake_minimum_required(VERSION 3.20)
project(map-renderer-v2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
target_compile_options(map-renderer PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

# Copy data files to build directory
add_custom_command(TARGET map-renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:map-renderer>/data
    COMMENT "Copying data files to build directory"
)
