/// @file osm_loader.cpp
/// @brief OSM protobuf loader implementation.

#include "data/osm_loader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "osm_data.pb.h"
#include "data/osm_types.h"

static constexpr uint32_t EXPECTED_SCHEMA_VERSION = 2;

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

osm::OSMData OSMLoader::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::fprintf(stderr, "[ERROR] OSMLoader: cannot open '%s'\n", path.c_str());
        return {};
    }

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(size));
    file.close();

    return load_from_bytes(buffer.data(), buffer.size());
}

osm::OSMData OSMLoader::load_from_bytes(const uint8_t* data, size_t size) {
    std::string serialised(reinterpret_cast<const char*>(data), size);
    return load_from_string(serialised);
}

osm::OSMData OSMLoader::load_from_string(const std::string& serialised) {
    osm_proto::OSMDataProto proto;
    if (!proto.ParseFromString(serialised)) {
        std::fprintf(stderr, "[ERROR] OSMLoader: failed to parse protobuf.\n");
        return {};
    }

    // Schema version check
    if (proto.schema_version() != EXPECTED_SCHEMA_VERSION) {
        std::fprintf(stderr, "[WARN]  OSMLoader: schema version %u "
                             "(expected %u). Attempting to load anyway.\n",
                     proto.schema_version(), EXPECTED_SCHEMA_VERSION);
    }

    osm::OSMData result;
    result.center_x = proto.center_x();
    result.center_z = proto.center_z();
    result.min_x    = proto.min_x();
    result.min_z    = proto.min_z();
    result.max_x    = proto.max_x();
    result.max_z    = proto.max_z();

    // --- Buildings ---
    result.buildings.reserve(proto.buildings_size());
    for (const auto& b : proto.buildings()) {
        osm::Building building;
        building.id     = b.id();
        building.height_m = b.height_m();
        building.type   = b.type();

        // Map height source string → enum
        if (b.height_source() == "tag") {
            building.height_source = osm::HeightSource::Tag;
        } else if (b.height_source() == "levels") {
            building.height_source = osm::HeightSource::Levels;
        } else {
            building.height_source = osm::HeightSource::Default;
        }

        building.footprint.reserve(b.footprint_size());
        for (const auto& pt : b.footprint()) {
            building.footprint.push_back({static_cast<float>(pt.x()),
                                          static_cast<float>(pt.z())});
        }
        result.buildings.push_back(std::move(building));
    }

    // --- Roads ---
    result.roads.reserve(proto.roads_size());
    for (const auto& r : proto.roads()) {
        osm::Road road;
        road.id      = r.id();
        road.type    = r.type();
        road.width_m = r.width_m();

        road.line.reserve(r.line_size());
        for (const auto& pt : r.line()) {
            road.line.push_back({static_cast<float>(pt.x()),
                                 static_cast<float>(pt.z())});
        }
        result.roads.push_back(std::move(road));
    }

    // --- Parks ---
    result.parks.reserve(proto.parks_size());
    for (const auto& p : proto.parks()) {
        osm::PolygonFeature pf;
        pf.type = p.type();
        pf.polygon.reserve(p.polygon_size());
        for (const auto& pt : p.polygon()) {
            pf.polygon.push_back({static_cast<float>(pt.x()),
                                  static_cast<float>(pt.z())});
        }
        result.parks.push_back(std::move(pf));
    }

    // --- Water ---
    result.water.reserve(proto.water_size());
    for (const auto& w : proto.water()) {
        osm::PolygonFeature pf;
        pf.type = w.type();
        pf.polygon.reserve(w.polygon_size());
        for (const auto& pt : w.polygon()) {
            pf.polygon.push_back({static_cast<float>(pt.x()),
                                  static_cast<float>(pt.z())});
        }
        result.water.push_back(std::move(pf));
    }

    // --- Landuse ---
    result.landuse.reserve(proto.landuse_size());
    for (const auto& l : proto.landuse()) {
        osm::PolygonFeature pf;
        pf.type = l.type();
        pf.polygon.reserve(l.polygon_size());
        for (const auto& pt : l.polygon()) {
            pf.polygon.push_back({static_cast<float>(pt.x()),
                                  static_cast<float>(pt.z())});
        }
        result.landuse.push_back(std::move(pf));
    }

    validate_bounds(result);
    return result;
}

// -----------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------

void OSMLoader::validate_bounds(osm::OSMData& data) {
    // If bounds are degenerate (all zero), compute from features
    if (data.min_x == 0.0 && data.max_x == 0.0 &&
        data.min_z == 0.0 && data.max_z == 0.0 &&
        data.has_data())
    {
        double mnx = 1e18, mxx = -1e18;
        double mnz = 1e18, mxz = -1e18;

        auto extend = [&](const std::vector<osm::Point>& pts) {
            for (const auto& p : pts) {
                if (p.x < mnx) mnx = p.x;
                if (p.x > mxx) mxx = p.x;
                if (p.z < mnz) mnz = p.z;
                if (p.z > mxz) mxz = p.z;
            }
        };

        for (const auto& b : data.buildings) extend(b.footprint);
        for (const auto& r : data.roads)       extend(r.line);
        for (const auto& p : data.parks)       extend(p.polygon);
        for (const auto& w : data.water)       extend(w.polygon);
        for (const auto& l : data.landuse)     extend(l.polygon);

        if (mnx < 1e17) {
            data.min_x = mnx; data.max_x = mxx;
            data.min_z = mnz; data.max_z = mxz;
            data.center_x = (mnx + mxx) / 2.0;
            data.center_z = (mnz + mxz) / 2.0;
        }
    }
}
