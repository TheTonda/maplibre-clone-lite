find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WebP REQUIRED IMPORTED_TARGET libwebp)

set(MAPRENDER_SOURCES
    src/c_api.cpp
    src/mbtiles_reader.cpp
    src/webp_decoder.cpp
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
        SQLite::SQLite3
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

    add_executable(make_fixture tests/make_fixture.cpp)
    target_link_libraries(make_fixture PRIVATE
        maprender StbImageWrite::StbImageWrite SQLite::SQLite3)
    target_compile_definitions(make_fixture PRIVATE
        MR_FIXTURE_OUT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

    add_executable(tile_dump tests/tile_dump.cpp)
    target_link_libraries(tile_dump PRIVATE
        maprender StbImageWrite::StbImageWrite SQLite::SQLite3)
    target_link_directories(tile_dump PRIVATE ${WebP_LIBRARY_DIRS})
    target_include_directories(tile_dump PRIVATE ${WebP_INCLUDE_DIRS})
    target_link_libraries(tile_dump PRIVATE ${WebP_LIBRARIES})

    enable_testing()
    add_test(NAME phase1_smoke_dummy
             COMMAND ${CMAKE_COMMAND} -E echo "phase1 build-only smoke")
endif()