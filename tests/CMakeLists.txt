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
