/// @file style_engine.cpp
/// @brief JSON style engine implementation.

#include "data/style_engine.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// -----------------------------------------------------------------------
// Default style palette (matches HLD / LLD spec)
// -----------------------------------------------------------------------

static const std::unordered_map<std::string, StyleRule> DEFAULTS = {
    {"building",        {"building",       {0.851f, 0.765f, 0.647f, 1.0f}, {}, 0.0f, 1.0f}},
    {"road_primary",    {"road_primary",   {}, {1.0f,   1.0f,  1.0f,   1.0f}, 8.0f, 0.0f}},
    {"road_secondary",  {"road_secondary", {}, {0.957f, 0.957f, 0.957f, 1.0f}, 6.0f, 0.0f}},
    {"road_residential",{"road_residential", {}, {0.9f,  0.9f,  0.9f,   1.0f}, 4.0f, 0.0f}},
    {"road_service",    {"road_service",   {}, {0.8f,   0.8f,  0.8f,   1.0f}, 3.0f, 0.0f}},
    {"water",           {"water",          {0.302f, 0.553f, 0.788f, 1.0f}, {}, 0.0f, 0.0f}},
    {"park",            {"park",           {0.561f, 0.729f, 0.498f, 1.0f}, {}, 0.0f, 0.0f}},
    {"landuse",         {"landuse",        {0.910f, 0.878f, 0.847f, 1.0f}, {}, 0.0f, 0.0f}},
    {"ground",          {"ground",         {0.118f, 0.118f, 0.125f, 1.0f}, {}, 0.0f, 0.0f}},
};

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

void StyleEngine::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "[WARN]  StyleEngine: cannot open '%s'. "
                             "Falling back to defaults.\n", path.c_str());
        insert_defaults();
        return;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    load_from_json(buf.str());
}

void StyleEngine::load_from_json(const std::string& json_content) {
    rules_.clear();

    try {
        auto j = json::parse(json_content);

        if (!j.is_array()) {
            std::fprintf(stderr, "[WARN]  StyleEngine: JSON root must be an "
                                 "array. Falling back to defaults.\n");
            insert_defaults();
            return;
        }

        for (const auto& item : j) {
            StyleRule rule;
            rule.feature_type = item.value("type", "");

            // Parse colours (arrays of 3 or 4 floats 0-255 or 0-1)
            auto parse_color = [](const json& c) -> glm::vec4 {
                if (c.is_array() && c.size() >= 3) {
                    float r = c[0].get<float>();
                    float g = c[1].get<float>();
                    float b = c[2].get<float>();
                    float a = (c.size() >= 4) ? c[3].get<float>() : 1.0f;
                    // If values > 1, assume 0-255 range
                    if (r > 1.0f) r /= 255.0f;
                    if (g > 1.0f) g /= 255.0f;
                    if (b > 1.0f) b /= 255.0f;
                    return {r, g, b, a};
                }
                return glm::vec4(1.0f);
            };

            if (item.contains("fill_color"))
                rule.fill_color = parse_color(item["fill_color"]);
            if (item.contains("line_color"))
                rule.line_color = parse_color(item["line_color"]);
            if (item.contains("line_width_meters"))
                rule.line_width_meters = item["line_width_meters"];
            if (item.contains("fill_extrusion_height"))
                rule.fill_extrusion_height = item["fill_extrusion_height"];

            if (!rule.feature_type.empty())
                rules_[rule.feature_type] = rule;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[WARN]  StyleEngine: JSON parse error: %s. "
                             "Falling back to defaults.\n", e.what());
    }

    insert_defaults();
}

StyleRule StyleEngine::get_rule(const std::string& feature_type) const {
    auto it = rules_.find(feature_type);
    if (it != rules_.end())
        return it->second;

    // Fallback to default if known type
    auto def = DEFAULTS.find(feature_type);
    if (def != DEFAULTS.end())
        return def->second;

    return DEFAULTS.at("ground");
}

StyleEngine StyleEngine::default_style() {
    StyleEngine engine;
    engine.insert_defaults();
    return engine;
}

void StyleEngine::insert_defaults() {
    // Only insert defaults for types not already in rules_
    for (const auto& [type, rule] : DEFAULTS) {
        if (rules_.find(type) == rules_.end())
            rules_[type] = rule;
    }
}
