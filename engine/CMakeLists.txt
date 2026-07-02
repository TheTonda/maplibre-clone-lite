# Engine static library

add_library(engine STATIC
    src/placeholder.cpp
    src/osm_loader.cpp
    src/tile_loader.cpp
    src/tile_cache.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROTO_GEN_DIR}
)

target_link_libraries(engine
    PUBLIC
        glm::glm
        protobuf::libprotobuf
        zstd::libzstd
    PRIVATE
        glad_gl_core_33
)
