#pragma once

/// @file style_engine.h
/// @brief JSON-based style engine for feature colours and widths.

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

/// Colour stored as RGBA float (0-1 range).
struct StyleRule {
    std::string feature_type;  ///< e.g. "building", "road_primary", "water"

    glm::vec4 fill_color       = glm::vec4(1.0f);   ///< Default white
    glm::vec4 line_color       = glm::vec4(1.0f);
    float     line_width_meters = 4.0f;
    float     fill_extrusion_height = 0.0f;         ///< 0 = no extrusion
};

/// Loads and provides style rules from a JSON file.
///
/// Rules are keyed by feature type string.  A missing rule falls back to a
/// hard-coded default style.
class StyleEngine {
public:
    /// Load rules from a JSON file on disk.
    /// On failure the engine falls back to the default style (does not crash).
    void load_from_file(const std::string& path);

    /// Load rules from an in-memory JSON string.
    void load_from_json(const std::string& json_content);

    /// Get the style rule for a given feature type.
    /// Falls back to a built-in default if the type is not found.
    StyleRule get_rule(const std::string& feature_type) const;

    /// Build an engine initialised with the hard-coded defaults.
    static StyleEngine default_style();

private:
    void insert_defaults();

    std::unordered_map<std::string, StyleRule> rules_;
};
