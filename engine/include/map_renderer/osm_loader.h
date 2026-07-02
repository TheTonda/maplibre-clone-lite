#pragma once

#include <cstdint>
#include <vector>

#include "osm_types.h"
#include "tile_id.h"

namespace map_renderer {

// Pure protobuf deserialization — converts a byte buffer (already
// zstd-decompressed) into a TileData struct. No file I/O, no threading.
class OSMLoader {
public:
    // Deserialize a protobuf byte buffer into TileData.
    // Returns false on parse error (corrupt data, schema mismatch).
    // Reserves feature vectors from the tile header counts before filling
    // them, so there is no reallocation during deserialization.
    // Sets world_offset_x/z from center_lat/lon using the dataset reference
    // point (see LLD §5.3 for the exact ENU formula).
    static bool deserialize(const std::vector<uint8_t>& bytes,
                            const TileId& id,
                            double ref_lat, double ref_lon,
                            TileData& out);
};

} // namespace map_renderer
