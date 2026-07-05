// Camera (2D orthographic) — implementation per LLD §5
// ENU formulas MUST match the Python preprocessor (LLD §5.3, §9.2).

#include "map_renderer/camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace map_renderer {

namespace {
constexpr double R = 6371000.0;  // Earth radius in meters
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;

// Clamp value to [lo, hi]
template <typename T>
T clamp_val(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Forward (WGS84 → world ENU):
//   x = R * cos(radians(ref_lat)) * radians(lon - ref_lon)
//   z = R * radians(lat - ref_lat)
[[maybe_unused]] void lat_lon_to_world_enu(double lat, double lon,
                          double ref_lat, double ref_lon,
                          float& out_x, float& out_z) {
    out_x = static_cast<float>(
        R * std::cos(ref_lat * DEG_TO_RAD) * ((lon - ref_lon) * DEG_TO_RAD));
    out_z = static_cast<float>(R * (lat - ref_lat) * DEG_TO_RAD);
}

// Inverse (world ENU meters → WGS84):
//   lat = ref_lat + degrees(z / R)
//   lon = ref_lon + degrees(x / (R * cos(radians(ref_lat))))
void world_enu_to_lat_lon(float x, float z,
                          double ref_lat, double ref_lon,
                          double& out_lat, double& out_lon) {
    out_lat = ref_lat + (static_cast<double>(z) / R) * RAD_TO_DEG;
    out_lon = ref_lon + (static_cast<double>(x) /
                         (R * std::cos(ref_lat * DEG_TO_RAD))) * RAD_TO_DEG;
}

// Standard slippy map tile computation (Web Mercator)
//   tile_x = floor((lon + 180) / 360 * 2^z)
//   tile_y = floor((1 - ln(tan(lat) + 1/cos(lat)) / π) / 2 * 2^z)
int lon_to_tile_x(double lon, uint32_t z) {
    int n = 1 << z;
    double t = (lon + 180.0) / 360.0 * n;
    return static_cast<int>(std::floor(t));
}

int lat_to_tile_y(double lat, uint32_t z) {
    int n = 1 << z;
    double lat_rad = lat * DEG_TO_RAD;
    double t = (1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n;
    return static_cast<int>(std::floor(t));
}
} // namespace

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
    clamp_position();
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
    // Clamp to reasonable range
    visible_span_ = clamp_val(visible_span_, 100.0f, 5000000.0f);
    matrices_valid_ = false;
    dirty_ = true;
}

// ── LLD §5.2: Tile zoom selection ────────────────────────────────────
// visible_span_ = meters across shorter viewport dimension
//   > 500000 → z8, > 50000 → z12, > 5000 → z15, ≤ 5000 → z17
uint32_t Camera::get_tile_zoom() const {
    if (visible_span_ > 500000.0f) return 8;
    if (visible_span_ > 50000.0f) return 12;
    if (visible_span_ > 5000.0f) return 15;
    return 17;
}

// ── LLD §5.3: Visible tile computation ──────────────────────────────
std::vector<TileId> Camera::get_visible_tiles(uint32_t tile_zoom) const {
    // 1. Convert camera center (x_, z_) from world ENU to lat/lon
    double center_lat, center_lon;
    world_enu_to_lat_lon(x_, z_, ref_lat_, ref_lon_, center_lat, center_lon);

    // 2. Compute viewport half-spans in world ENU
    //    (for tile selection we use the full span in each dimension)
    float half_span = visible_span_ / 2.0f;
    // Approximate half-spans symmetric around camera center
    float x_min = x_ - half_span;
    float x_max = x_ + half_span;
    float z_min = z_ - half_span;
    float z_max = z_ + half_span;

    // 3. Convert viewport corners to lat/lon
    double lat_min, lon_min, lat_max, lon_max;
    world_enu_to_lat_lon(x_min, z_min, ref_lat_, ref_lon_, lat_min, lon_min);
    world_enu_to_lat_lon(x_max, z_max, ref_lat_, ref_lon_, lat_max, lon_max);

    // 4. Compute tile x/y range from bounding box
    int tx_min = lon_to_tile_x(lon_min, tile_zoom);
    int tx_max = lon_to_tile_x(lon_max, tile_zoom);
    int ty_min = lat_to_tile_y(lat_max, tile_zoom);  // lat_max = north = smaller y
    int ty_max = lat_to_tile_y(lat_min, tile_zoom);

    // 5. Clamp to [0, 2^z)
    int max_tile = (1 << tile_zoom) - 1;
    tx_min = clamp_val(tx_min, 0, max_tile);
    tx_max = clamp_val(tx_max, 0, max_tile);
    ty_min = clamp_val(ty_min, 0, max_tile);
    ty_max = clamp_val(ty_max, 0, max_tile);

    // 6. Return all TileIds in the range
    std::vector<TileId> tiles;
    for (int ty = ty_min; ty <= ty_max; ++ty) {
        for (int tx = tx_min; tx <= tx_max; ++tx) {
            tiles.push_back(TileId{tile_zoom, static_cast<uint32_t>(tx),
                                    static_cast<uint32_t>(ty)});
        }
    }
    return tiles;
}

void Camera::apply_input(const InputData& input, float /*dt*/) {
    switch (input.type) {
    case InputEvent::PanMove: {
        // Pan proportional to visible span (so drag feels consistent)
        float pan_speed = visible_span_ * 0.002f;  // 0.2% of span per pixel
        pan(-input.x * pan_speed, input.y * pan_speed);
        break;
    }
    case InputEvent::Zoom: {
        // Scroll wheel: zoom in/out by factor
        if (input.delta > 0) {
            zoom_by(0.8f);  // zoom in
        } else {
            zoom_by(1.25f);  // zoom out
        }
        break;
    }
    case InputEvent::KeyZoomIn:
        zoom_by(0.8f);
        break;
    case InputEvent::KeyZoomOut:
        zoom_by(1.25f);
        break;
    case InputEvent::KeyPanLeft:
        pan(-visible_span_ * 0.1f, 0.0f);
        break;
    case InputEvent::KeyPanRight:
        pan(visible_span_ * 0.1f, 0.0f);
        break;
    case InputEvent::KeyPanUp:
        pan(0.0f, visible_span_ * 0.1f);
        break;
    case InputEvent::KeyPanDown:
        pan(0.0f, -visible_span_ * 0.1f);
        break;
    default:
        break;
    }
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
    bounds_set_ = true;
}

void Camera::frame_dataset() {
    // Center on dataset midpoint
    x_ = (min_x_ + max_x_) / 2.0f;
    z_ = (min_z_ + max_z_) / 2.0f;
    // Fit the larger dimension
    float span_x = max_x_ - min_x_;
    float span_z = max_z_ - min_z_;
    visible_span_ = std::max(span_x, span_z) * 1.1f;  // 10% margin
    matrices_valid_ = false;
    dirty_ = true;
}

// ── LLD §5.4: Matrix computation ─────────────────────────────────────
void Camera::recompute_matrices(float aspect) const {
    last_aspect_ = aspect;
    float half_span = visible_span_ / 2.0f;
    float half_w, half_h;

    if (aspect >= 1.0f) {
        // Landscape: span is the shorter dimension (height)
        half_h = half_span;
        half_w = half_span * aspect;
    } else {
        // Portrait: span is the shorter dimension (width)
        half_w = half_span;
        half_h = half_span / aspect;
    }

    proj_ = glm::ortho(
        x_ - half_w, x_ + half_w,
        z_ - half_h, z_ + half_h,
        -1.0f, 1.0f
    );
    view_ = glm::mat4(1.0f);  // identity — no view transform for 2D
    matrices_valid_ = true;
}

glm::mat4 Camera::get_projection_matrix(float aspect) const {
    if (!matrices_valid_ || aspect != last_aspect_) {
        recompute_matrices(aspect);
    }
    return proj_;
}

glm::mat4 Camera::get_view_matrix() const {
    if (!matrices_valid_) {
        recompute_matrices(last_aspect_);
    }
    return view_;
}

bool Camera::is_dirty() const { return dirty_; }
void Camera::clear_dirty() { dirty_ = false; }
void Camera::mark_dirty() { dirty_ = true; }

float Camera::get_x() const { return x_; }
float Camera::get_z() const { return z_; }
float Camera::get_visible_span() const { return visible_span_; }

void Camera::clamp_position() {
    if (!bounds_set_) return;
    x_ = clamp_val(x_, min_x_, max_x_);
    z_ = clamp_val(z_, min_z_, max_z_);
}

} // namespace map_renderer