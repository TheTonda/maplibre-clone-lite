#pragma once

#include "maprender/mercator.h"

#include <algorithm>
#include <cmath>

namespace maprender {

struct Viewport {
    int zoom = 0;
    int screen_w = 0;
    int screen_h = 0;
    double center_x = 0.0;  // world px at current zoom
    double center_y = 0.0;

    void set_view(double lon, double lat, int z, int sw, int sh);
    void pan(int dx_px, int dy_px);
    bool zoom_by(int delta, double anchor_lon, double anchor_lat);

    double world_left()  const { return center_x - screen_w / 2.0; }
    double world_top()   const { return center_y - screen_h / 2.0; }
    int    tile_x_min()  const { return static_cast<int>(std::floor(world_left()  / kTileSize)); }
    int    tile_x_max()  const { return static_cast<int>(std::floor((world_left() + screen_w - 1) / kTileSize)); }
    int    tile_y_min()  const { return static_cast<int>(std::floor(world_top()   / kTileSize)); }
    int    tile_y_max()  const { return static_cast<int>(std::floor((world_top() + screen_h - 1) / kTileSize)); }

    int max_tile_xy() const { return 1 << zoom; }
};

}  // namespace maprender