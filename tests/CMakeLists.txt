# Unit and integration tests

# Convenience function for adding a test
function(add_engine_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name}
        PRIVATE
            engine
            GTest::gtest_main
    )
    target_include_directories(${name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/engine/include
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Add placeholder test
add_engine_test(test_placeholder)

# Task 2: Platform abstraction interface
add_engine_test(test_platform)

# Task 3: Tile ID + OSM types
add_engine_test(test_tile_types)

# Task 4: Color table
add_engine_test(test_color_table)

# Task 8: Geometry builder
add_engine_test(test_geometry_builder)

# Task 9: Shader program (embedded GLSL sources)
add_engine_test(test_shaders)
target_include_directories(test_shaders
    PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/src
)

# Task 10: Renderer — GL initialization + tile upload
add_engine_test(test_renderer)

# Task 7: OSM loader + tile loader
# Needs absl for protobuf's transitive deps (module-mode protobuf::libprotobuf
# doesn't propagate them). Link the umbrella absl targets that cover
# log_internal_check_op and log_internal_message.
add_engine_test(test_osm_loader)
target_link_libraries(test_osm_loader
    PRIVATE
        Threads::Threads
        absl::absl_check
        absl::absl_log
        absl::log_internal_check_op
        absl::log_internal_message
)
