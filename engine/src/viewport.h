#pragma once

#include <cmath>

namespace maprender {

constexpr int kTileSize = 256;

inline double world_width_at(int z) { return static_cast<double>(kTileSize) * (1ull << z); }

inline double lon_to_world_x(double lon, int z) {
    return (lon + 180.0) / 360.0 * world_width_at(z);
}

inline double lat_to_world_y(double lat_deg, int z) {
    const double lat = lat_deg * M_PI / 180.0;
    const double y = std::log(std::tan(lat) + 1.0 / std::cos(lat));
    return (1.0 - y / M_PI) / 2.0 * world_width_at(z);
}

inline double world_x_to_lon(double px, int z) {
    return px / world_width_at(z) * 360.0 - 180.0;
}

inline double world_y_to_lat(double py, int z) {
    const double n = M_PI - 2.0 * M_PI * py / world_width_at(z);
    return 180.0 / M_PI * std::atan(std::sinh(n));
}

struct Viewport {
    int zoom = 0;
    int screen_w = 0;
    int screen_h = 0;
    double center_x = 0.0;  // world px at current zoom
    double center_y = 0.0;

    void set_view(double lon, double lat, int z, int sw, int sh);
    void pan(int dx_px, int dy_px);
    // Returns false if zoom clamped unchanged.
    bool zoom_by(int delta, double anchor_lon, double anchor_lat);

    // Visible world-pixel bounds (top-left inclusive).
    double world_left()  const { return center_x - screen_w / 2.0; }
    double world_top()   const { return center_y - screen_h / 2.0; }
    int    tile_x_min()  const { return static_cast<int>(std::floor(world_left()  / kTileSize)); }
    int    tile_x_max()  const { return static_cast<int>(std::floor((world_left() + screen_w - 1) / kTileSize)); }
    int    tile_y_min()  const { return static_cast<int>(std::floor(world_top()   / kTileSize)); }
    int    tile_y_max()  const { return static_cast<int>(std::floor((world_top() + screen_h - 1) / kTileSize)); }

    int max_tile_xy() const { return 1 << zoom; }
};

}  // namespace maprender