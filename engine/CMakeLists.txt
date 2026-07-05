# Engine static library

add_library(engine STATIC
    src/placeholder.cpp
    src/osm_loader.cpp
    src/tile_loader.cpp
    src/tile_cache.cpp
    src/geometry_builder.cpp
    src/camera.cpp
    src/engine.cpp
    src/renderer.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PROTO_GEN_DIR}
)

target_link_libraries(engine
    PUBLIC
        glm::glm
        PkgConfig::PROTOBUF
        zstd::libzstd
    PRIVATE
        glad_gl_core_33
)
