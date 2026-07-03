#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "map_renderer/platform.h"
#include "map_renderer/tile_id.h"

namespace map_renderer {

enum class CameraMode {
    MODE_2D,  // implemented
    MODE_3D,  // reserved for future, not implemented in v2.0
};

class Camera {
public:
    Camera();

    // Position in world ENU meters (from dataset reference point)
    void set_position(float x, float z);
    void pan(float dx, float dz);

    // Zoom: visible_span = meters across the shorter viewport dimension
    void set_visible_span(float span);
    void zoom_by(float factor);

    // Get matrices (only recomputed if dirty)
    glm::mat4 get_projection_matrix(float aspect) const;
    glm::mat4 get_view_matrix() const;
    bool is_dirty() const;
    void clear_dirty();
    void mark_dirty();

    // Current state
    float get_x() const;
    float get_z() const;
    float get_visible_span() const;

    // Determine which tile zoom level matches the current view
    uint32_t get_tile_zoom() const;

    // Compute visible tile range at the current tile zoom
    std::vector<TileId> get_visible_tiles(uint32_t tile_zoom) const;

    // Input
    void apply_input(const InputData& input, float dt);

    // Set the ENU reference point (from dataset metadata).
    // Required before get_visible_tiles() can convert world ENU -> WGS84.
    void set_reference_point(double ref_lat, double ref_lon);

    // Set dataset bounds (for initial framing and limits)
    void set_dataset_bounds(float min_x, float max_x, float min_z, float max_z);

    // Frame the entire dataset
    void frame_dataset();

private:
    float x_ = 0.0f;       // world ENU east
    float z_ = 0.0f;       // world ENU north
    float visible_span_ = 50000.0f;  // meters across shorter viewport dim

    float min_x_ = 0.0f;
    float max_x_ = 0.0f;
    float min_z_ = 0.0f;
    float max_z_ = 0.0f;

    double ref_lat_ = 0.0;  // ENU reference point (from metadata)
    double ref_lon_ = 0.0;

    bool dirty_ = true;

    // Cached matrices
    mutable glm::mat4 proj_ = glm::mat4(1.0f);
    mutable glm::mat4 view_ = glm::mat4(1.0f);
    mutable bool matrices_valid_ = false;

    void recompute_matrices(float aspect) const;
    void clamp_position();
};

} // namespace map_renderer
