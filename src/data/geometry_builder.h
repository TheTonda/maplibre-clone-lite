#pragma once

/// @file geometry_builder.h
/// @brief Converts OSM data into GPU-ready mesh structures.

#include <vector>
#include <cstdint>

#include "data/osm_types.h"
#include <glm/glm.hpp>

// ---- Vertex formats ---------------------------------------------------

/// Vertex for buildings (3D with normal).
struct BuildingVertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

/// Vertex for 2D fill features (parks, water, landuse – plain colour).
struct FillVertex {
    glm::vec2 pos;  // (x, z) in ENU
};

/// Vertex for roads (3D quad vertices).
struct RoadVertex {
    glm::vec3 pos;
};

// ---- Mesh bundles ------------------------------------------------------

struct BuildingMesh {
    std::vector<BuildingVertex> vertices;
    std::vector<uint32_t>       indices;
};

struct PolygonMesh {
    std::vector<FillVertex>  vertices;
    std::vector<uint32_t>    indices;
};

struct LineMesh {
    std::vector<RoadVertex>  vertices;
    std::vector<uint32_t>    indices;
};

struct GroundMesh {
    std::vector<FillVertex>  vertices;
    std::vector<uint32_t>    indices;
};

/// All geometry produced from a single OSMData pass.
struct GeometryData {
    BuildingMesh buildings;
    PolygonMesh  parks;
    PolygonMesh  water;
    PolygonMesh  landuse;
    LineMesh     roads_primary;
    LineMesh     roads_secondary;
    LineMesh     roads_residential;
    LineMesh     roads_service;
    GroundMesh   ground;
};

// ---- Builder -----------------------------------------------------------

class GeometryBuilder {
public:
    /// Build all geometry from an OSMData set.
    /// @param half_size  Half-size of the ground plane (metres).
    static GeometryData build_all(const osm::OSMData& data,
                                  float half_size = 5000.0f);

    /// Triangulate a single closed 2D polygon (CCW, no holes).
    /// @return  Index list into a shared vertex array.
    static std::vector<uint32_t> triangulate_polygon(
        const std::vector<osm::Point>& polygon,
        std::vector<glm::vec2>& out_vertices);

    /// Extrude a building footprint.
    static BuildingMesh build_building(const osm::Building& building);

    /// Generate a ribbon (2-tri quad strip) from a centre-line.
    static LineMesh build_road_quad(const osm::Road& road);

    /// Create a simple ground quad.
    static GroundMesh build_ground(float half_size);
};
