find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WebP REQUIRED IMPORTED_TARGET libwebp)

set(MAPBAKE_CORE_SOURCES
    src/osm_reader.cpp
    src/style.cpp
    src/tile_baker.cpp
    src/tile_rasterizer.cpp
    src/geometry_clip.cpp
    src/webp_encoder.cpp
    src/mbtiles_writer.cpp
)

add_library(mapbake_core STATIC ${MAPBAKE_CORE_SOURCES})
target_include_directories(mapbake_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/engine/include
        ${CMAKE_SOURCE_DIR}/engine/src
)
target_link_libraries(mapbake_core
    PUBLIC
        SQLite3::SQLite3
        PkgConfig::WebP
        ZLIB::ZLIB
        bz2
        expat
        pthread
)

add_executable(mapbake src/main.cpp)
target_link_libraries(mapbake PRIVATE mapbake_core)

if(MR_BUILD_TESTS)
    add_executable(rasterizer_smoke tests/rasterizer_smoke.cpp)
    target_include_directories(rasterizer_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/engine/include)
    target_link_libraries(rasterizer_smoke PRIVATE mapbake_core)
endif()