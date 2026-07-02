#pragma once

#include <string>
#include <unordered_map>

namespace map_renderer {

struct Color {
    float r, g, b, a;
};

// Hardcoded feature → color table for v2.0
// Future: replace with JSON-loaded table
inline Color get_color(const std::string& feature_type) {
    static const std::unordered_map<std::string, Color> table = {
        {"ground",         {0.12f, 0.12f, 0.14f, 1.0f}},  // dark gray
        {"water",          {0.30f, 0.55f, 0.79f, 1.0f}},  // blue
        {"park",           {0.56f, 0.73f, 0.50f, 1.0f}},  // green
        {"landuse",        {0.91f, 0.88f, 0.85f, 1.0f}},  // tan
        {"building",       {0.85f, 0.76f, 0.65f, 1.0f}},  // light brown
        {"road",           {0.95f, 0.95f, 0.95f, 1.0f}},  // white
        {"road_primary",   {1.00f, 0.98f, 0.90f, 1.0f}},  // warm white
        {"road_secondary", {0.94f, 0.94f, 0.94f, 1.0f}},  // light gray
    };
    auto it = table.find(feature_type);
    if (it != table.end()) return it->second;
    return {1.0f, 0.0f, 1.0f, 1.0f};  // magenta = unknown (debug indicator)
}

} // namespace map_renderer
