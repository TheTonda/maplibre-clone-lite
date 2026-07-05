#pragma once

#include "geometry_clip.h"
#include "tile_rasterizer.h"

#include <maprender/mercator.h>

#include <map>
#include <string>
#include <vector>

namespace mapbake {

struct TileKey { int z, x, y; };
struct TileKeyLess { bool operator()(const TileKey& a, const TileKey& b) const {
    if (a.z != b.z) return a.z < b.z;
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}};

using TileFeatures = std::map<TileKey, std::vector<Feature>, TileKeyLess>;

// Projects features to world pixels at zoom z and buckets them into the tiles
// they touch (slippy x/y).
TileFeatures bucket_features(const std::vector<Feature>& features, int z);

}  // namespace mapbake