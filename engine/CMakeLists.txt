find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WebP REQUIRED IMPORTED_TARGET libwebp)

set(MAPRENDER_SOURCES
    src/c_api.cpp
    src/mbtiles_reader.cpp
    src/webp_decoder.cpp
    src/viewport.cpp
    src/tile_cache.cpp
    src/renderer.cpp
)

add_library(maprender STATIC ${MAPRENDER_SOURCES})
target_include_directories(maprender
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(maprender
    PUBLIC
        SQLite3::SQLite3
    PRIVATE
        PkgConfig::WebP
        ZLIB::ZLIB
)

if(MR_BUILD_TESTS)
    # Stb single-header lives in /usr/include/stb on Arch.
    add_library(StbImageWrite INTERFACE IMPORTED)
    find_path(STB_INCLUDE_DIR stb/stb_image_write.h)
    if(NOT STB_INCLUDE_DIR)
        message(FATAL_ERROR "stb_image_write.h not found (install the 'stb' package)")
    endif()
    set_target_properties(StbImageWrite PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${STB_INCLUDE_DIR})
    add_library(StbImageWrite::StbImageWrite ALIAS StbImageWrite)

    find_package(GTest REQUIRED)

    add_executable(make_fixture tests/make_fixture.cpp)
    target_link_libraries(make_fixture PRIVATE
        maprender StbImageWrite::StbImageWrite SQLite3::SQLite3 PkgConfig::WebP)
    target_compile_definitions(make_fixture PRIVATE
        MR_FIXTURE_OUT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

    add_executable(make_highzoom_fixture tests/make_highzoom_fixture.cpp)
    target_link_libraries(make_highzoom_fixture PRIVATE
        maprender SQLite3::SQLite3 PkgConfig::WebP)
    target_compile_definitions(make_highzoom_fixture PRIVATE
        MR_FIXTURE_OUT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

    add_executable(tile_dump tests/tile_dump.cpp)
    target_link_libraries(tile_dump PRIVATE
        maprender StbImageWrite::StbImageWrite SQLite3::SQLite3 PkgConfig::WebP)

    add_executable(snapshot tests/snapshot.cpp)
    target_link_libraries(snapshot PRIVATE
        maprender StbImageWrite::StbImageWrite)

    add_executable(tile_sampler tests/tile_sampler.cpp)
    target_link_libraries(tile_sampler PRIVATE
        maprender StbImageWrite::StbImageWrite)

    add_executable(engine_tests
        tests/test_viewport.cpp
        tests/test_cache.cpp
        tests/test_render.cpp)
    target_link_libraries(engine_tests PRIVATE
        maprender GTest::gtest GTest::gtest_main)

    enable_testing()
    add_test(NAME engine_unit COMMAND engine_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
