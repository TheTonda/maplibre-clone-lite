#pragma once

/// @file osm_types.h
/// @brief Internal C++ representation of OSM features, independent of the
///        protobuf serialisation format.

#include <cstdint>
#include <string>
#include <vector>

namespace osm {

/// A 2D point in local ENU (East-North-Up) coordinates (metres).
struct Point {
    float x = 0.0f;  ///< Easting
    float z = 0.0f;  ///< Northing
};

/// How the building height was determined.
enum class HeightSource : uint8_t {
    Tag,       ///< Explicit `height` tag (metres).
    Levels,    ///< Derived from `building:levels` × 3.0 m.
    Default,   ///< Fallback default height (9.0 m).
};

/// A building with an extrudable footprint and height metadata.
struct Building {
    int64_t      id;
    std::vector<Point> footprint;  ///< CCW wound, closed polygon.
    float        height_m      = 9.0f;
    HeightSource height_source = HeightSource::Default;
    std::string  type;              ///< "house", "apartments", etc.
};

/// A road segment defined by a centre-line polyline.
struct Road {
    int64_t      id;
    std::vector<Point> line;   ///< Ordered polyline.
    std::string  type;             ///< "primary", "secondary", etc.
    float        width_m = 6.0f;  ///< Inferred from type if not tagged.
};

/// A closed polygon feature (parks, water bodies, land use areas).
struct PolygonFeature {
    std::vector<Point> polygon;  ///< CCW wound, closed.
    std::string        type;        ///< "park", "water", "landuse"
};

/// Top-level container for all data extracted from an OSM PBF file.
struct OSMData {
    std::vector<Building>       buildings;
    std::vector<Road>           roads;
    std::vector<PolygonFeature> parks;
    std::vector<PolygonFeature> water;
    std::vector<PolygonFeature> landuse;

    // Bounding box (local ENU metres)
    double center_x = 0.0, center_z = 0.0;
    double min_x = 0.0, min_z = 0.0;
    double max_x = 0.0, max_z = 0.0;

    bool has_data() const {
        return !buildings.empty() || !roads.empty() ||
               !parks.empty() || !water.empty() || !landuse.empty();
    }
};

}  // namespace osm
