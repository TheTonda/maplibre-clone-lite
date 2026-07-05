#pragma once

#include <cmath>

namespace maprender {

constexpr int kTileSize = 256;

inline double world_width_at(int z) {
    return static_cast<double>(kTileSize) * (1ull << z);
}

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

}  // namespace maprender