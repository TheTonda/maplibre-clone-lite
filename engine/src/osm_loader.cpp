#include "map_renderer/osm_loader.h"

#include <cmath>
#include "osm_data.pb.h"

namespace map_renderer {

namespace {
constexpr double R = 6371000.0;  // Earth radius in meters
constexpr double DEG_TO_RAD = M_PI / 180.0;

// ENU conversion formula (must match preprocessor exactly — LLD §5.3):
//   x = R * cos(radians(ref_lat)) * radians(lon - ref_lon)
//   z = R * radians(lat - ref_lat)
void compute_world_offset(double center_lat, double center_lon,
                          double ref_lat, double ref_lon,
                          float& out_x, float& out_z) {
    out_x = static_cast<float>(
        R * std::cos(ref_lat * DEG_TO_RAD) * ((center_lon - ref_lon) * DEG_TO_RAD));
    out_z = static_cast<float>(
        R * (center_lat - ref_lat) * DEG_TO_RAD);
}
} // namespace

bool OSMLoader::deserialize(const std::vector<uint8_t>& bytes,
                             const TileId& /*id*/,
                             double ref_lat, double ref_lon,
                             TileData& out) {
    map_renderer_pb::Tile proto_tile;
    if (!proto_tile.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
        return false;
    }

    // Reserve feature vectors from tile header counts
    out.buildings.reserve(proto_tile.building_count());
    out.roads.reserve(proto_tile.road_count());
    out.polygons.reserve(proto_tile.polygon_count());

    // Deserialize buildings
    for (int i = 0; i < proto_tile.buildings_size(); ++i) {
        const auto& pb = proto_tile.buildings(i);
        Building b;
        b.id = pb.id();
        b.height = pb.height_m();
        b.footprint.reserve(pb.footprint_size());
        for (int j = 0; j < pb.footprint_size(); ++j) {
            b.footprint.push_back({pb.footprint(j).x(), pb.footprint(j).z()});
        }
        out.buildings.push_back(std::move(b));
    }

    // Deserialize roads
    for (int i = 0; i < proto_tile.roads_size(); ++i) {
        const auto& pb = proto_tile.roads(i);
        Road r;
        r.id = pb.id();
        r.type = pb.type();
        r.width = pb.width_m();
        r.line.reserve(pb.line_size());
        for (int j = 0; j < pb.line_size(); ++j) {
            r.line.push_back({pb.line(j).x(), pb.line(j).z()});
        }
        out.roads.push_back(std::move(r));
    }

    // Deserialize polygons
    for (int i = 0; i < proto_tile.polygons_size(); ++i) {
        const auto& pb = proto_tile.polygons(i);
        PolygonFeature pf;
        pf.type = pb.type();
        pf.polygon.reserve(pb.polygon_size());
        for (int j = 0; j < pb.polygon_size(); ++j) {
            pf.polygon.push_back({pb.polygon(j).x(), pb.polygon(j).z()});
        }
        out.polygons.push_back(std::move(pf));
    }

    // Compute world offset from tile center lat/lon
    compute_world_offset(proto_tile.center_lat(), proto_tile.center_lon(),
                         ref_lat, ref_lon,
                         out.world_offset_x, out.world_offset_z);

    out.center_lat = proto_tile.center_lat();
    out.center_lon = proto_tile.center_lon();

    return true;
}

} // namespace map_renderer
