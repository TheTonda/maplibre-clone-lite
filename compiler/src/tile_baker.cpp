#include "tile_baker.h"

#include <algorithm>
#include <cmath>

namespace mapbake {

static int tile_for(double world_px) {
    return static_cast<int>(std::floor(world_px / maprender::kTileSize));
}

TileFeatures bucket_features(const std::vector<Feature>& features, int z) {
    TileFeatures buckets;
    const int max_xy = static_cast<int>(1u << z);

    for (const auto& feat : features) {
        if (feat.geometry.empty()) continue;

        int x_min = max_xy, x_max = -1, y_min = max_xy, y_max = -1;
        auto expand_bbox = [&](const Ring& ring) {
            for (const auto& p : ring) {
                double wx = maprender::lon_to_world_x(p.x, z);
                double wy = maprender::lat_to_world_y(p.y, z);
                int tx = tile_for(wx);
                int ty = tile_for(wy);
                x_min = std::min(x_min, tx);
                x_max = std::max(x_max, tx);
                y_min = std::min(y_min, ty);
                y_max = std::max(y_max, ty);
            }
        };
        expand_bbox(feat.geometry);
        for (const auto& hole : feat.inner_rings) expand_bbox(hole);
        x_min = std::max(0, x_min - 1);
        x_max = std::min(max_xy - 1, x_max + 1);
        y_min = std::max(0, y_min - 1);
        y_max = std::min(max_xy - 1, y_max + 1);

        for (int ty = y_min; ty <= y_max; ++ty) {
            for (int tx = x_min; tx <= x_max; ++tx) {
                buckets[{z, tx, ty}].push_back(feat);
            }
        }
    }
    return buckets;
}

}  // namespace mapbake