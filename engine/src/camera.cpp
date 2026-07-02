#include "map_renderer/camera.h"

namespace map_renderer {

Camera::Camera() {
    dirty_ = true;
    matrices_valid_ = false;
}

void Camera::set_position(float x, float z) {
    x_ = x;
    z_ = z;
    matrices_valid_ = false;
    dirty_ = true;
}

void Camera::pan(float dx, float dz) {
    x_ += dx;
    z_ += dz;
    matrices_valid_ = false;
    dirty_ = true;
}

void Camera::set_visible_span(float span) {
    visible_span_ = span;
    matrices_valid_ = false;
    dirty_ = true;
}

void Camera::zoom_by(float factor) {
    visible_span_ *= factor;
    matrices_valid_ = false;
    dirty_ = true;
}

glm::mat4 Camera::get_projection_matrix(float /*aspect*/) const {
    return glm::mat4(1.0f);
}

glm::mat4 Camera::get_view_matrix() const {
    return glm::mat4(1.0f);
}

bool Camera::is_dirty() const {
    return dirty_;
}

void Camera::clear_dirty() {
    dirty_ = false;
}

float Camera::get_x() const {
    return x_;
}

float Camera::get_z() const {
    return z_;
}

float Camera::get_visible_span() const {
    return visible_span_;
}

uint32_t Camera::get_tile_zoom() const {
    return 12;
}

std::vector<TileId> Camera::get_visible_tiles(uint32_t /*tile_zoom*/) const {
    return {};
}

void Camera::apply_input(const InputData& /*input*/, float /*dt*/) {
    dirty_ = true;
}

void Camera::set_reference_point(double ref_lat, double ref_lon) {
    ref_lat_ = ref_lat;
    ref_lon_ = ref_lon;
}

void Camera::set_dataset_bounds(float min_x, float max_x,
                                float min_z, float max_z) {
    min_x_ = min_x;
    max_x_ = max_x;
    min_z_ = min_z;
    max_z_ = max_z;
}

void Camera::frame_dataset() {
    // Stub — implemented in Task 13.
}

void Camera::recompute_matrices(float /*aspect*/) const {
    // Stub — implemented in Task 13.
}

void Camera::clamp_position() {
    // Stub — implemented in Task 13.
}

} // namespace map_renderer
