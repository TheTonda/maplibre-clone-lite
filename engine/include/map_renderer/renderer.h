#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "map_renderer/osm_types.h"
#include "map_renderer/platform.h"
#include "map_renderer/tile_cache.h"
#include "map_renderer/tile_id.h"

namespace map_renderer {

// Forward declaration — Camera is implemented in Task 13. The render()
// method takes a const Camera& but Task 10 does not implement it (the body
// is a stub filled in by Task 11).
class Camera;

// Renderer owns the shader program and per-tile GPU resources (VAO/VBO).
// It is driven by the Engine: initialize() once, on_tile_loaded() when the
// TileCache reports a freshly inserted tile, render() each frame, and
// cleanup() on shutdown. Tile eviction frees per-tile GL resources via
// on_tile_evicted() (registered as the TileCache eviction callback).
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Store GL functions, compile the fill shader, and cache uniform
    // locations. Returns false if shader compilation or linking fails.
    bool initialize(PlatformInterface& platform);

    // Delete the shader program. Per-tile VAO/VBO are freed via the
    // TileCache eviction callback (on_tile_evicted) during normal operation;
    // callers that bypass the cache must call on_tile_evicted themselves.
    void cleanup();

    // Render one frame. STUB for Task 10 — Task 11 implements the draw loop.
    void render(const Camera& camera, TileCache& cache,
                const std::vector<TileId>& visible_tiles);

    // Called by TileCache when a tile is evicted: delete the tile's VAO/VBO.
    void on_tile_evicted(const TileId& id, TileData& tile);

    // Called by Engine after a tile is inserted into the cache: build
    // geometry, create VAO+VBO, upload vertices, and mark tile.uploaded.
    void on_tile_loaded(const TileId& id, TileData& tile);

    // Test accessor for the shader program handle.
    uint32_t shader_program() const { return shader_program_; }

private:
    PlatformInterface* platform_ = nullptr;
    const GLFunctions* gl_ = nullptr;

    uint32_t shader_program_ = 0;
    int32_t uniform_proj_ = 0;
    int32_t uniform_view_ = 0;
    int32_t uniform_color_ = 0;
    int32_t uniform_tile_offset_ = 0;

    // Compile + link the fill shader program. Returns false on failure.
    bool compile_shaders();

    // Build tile geometry via GeometryBuilder and upload it to a new VAO/VBO.
    void upload_tile_geometry(TileData& tile);

    // Draw a single tile. STUB for Task 10 — Task 11 implements this.
    void draw_tile(const TileData& tile, const glm::mat4& proj,
                   const glm::mat4& view);

    // Convert a feature type name to an RGBA color via color_table.h.
    glm::vec4 get_color(const std::string& feature_type) const;
};

} // namespace map_renderer
