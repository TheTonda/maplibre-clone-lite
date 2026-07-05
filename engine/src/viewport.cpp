#include "viewport.h"

#include <algorithm>

namespace maprender {

void Viewport::set_view(double lon, double lat, int z, int sw, int sh) {
    zoom = z;
    screen_w = sw;
    screen_h = sh;
    center_x = lon_to_world_x(lon, z);
    center_y = lat_to_world_y(lat, z);
    {
        const double W = world_width_at(z);
        const double half_w = sw / 2.0;
        const double half_h = sh / 2.0;
        if (center_x - half_w < 0)      center_x = half_w;
        if (center_x + half_w > W)      center_x = std::max(half_w, W - half_w);
        if (center_y - half_h < 0)      center_y = half_h;
        if (center_y + half_h > W)      center_y = std::max(half_h, W - half_h);
    }
}

void Viewport::pan(int dx_px, int dy_px) {
    center_x -= dx_px;
    center_y -= dy_px;
    const double W = world_width_at(zoom);
    const double half_w = screen_w / 2.0;
    const double half_h = screen_h / 2.0;
    center_x = std::clamp(center_x, half_w, std::max(half_w, W - half_w));
    center_y = std::clamp(center_y, half_h, std::max(half_h, W - half_h));
}

bool Viewport::zoom_by(int delta, double anchor_lon, double anchor_lat) {
    const int new_zoom = std::clamp(zoom + delta, 0, 20);
    if (new_zoom == zoom) return false;

    const double ax_old = lon_to_world_x(anchor_lon, zoom);
    const double ay_old = lat_to_world_y(anchor_lat, zoom);
    const double off_x = ax_old - center_x;
    const double off_y = ay_old - center_y;

    zoom = new_zoom;
    const double ax_new = lon_to_world_x(anchor_lon, zoom);
    const double ay_new = lat_to_world_y(anchor_lat, zoom);
    center_x = ax_new - off_x;
    center_y = ay_new - off_y;

    const double W = world_width_at(zoom);
    const double half_w = screen_w / 2.0;
    const double half_h = screen_h / 2.0;
    center_x = std::clamp(center_x, half_w, std::max(half_w, W - half_w));
    center_y = std::clamp(center_y, half_h, std::max(half_h, W - half_h));
    return true;
}

}  // namespace maprender