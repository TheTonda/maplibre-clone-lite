#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mapbake {

struct StyleRule {
    const char* key;
    const char* value;  // nullptr = any value for that key
    bool area;
    uint32_t rgba;
    float line_width_z14;  // in px at z14, scaled for other zooms
    int layer;             // painter order, lower first
};

// Hardcoded OpenStreetMap palette. Returns nullptr if no rule matches.
const StyleRule* match_style(const std::vector<std::pair<std::string, std::string>>& tags,
                             bool is_closed);

constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (static_cast<uint32_t>(r) << 0) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(a) << 24);
}

}  // namespace mapbake