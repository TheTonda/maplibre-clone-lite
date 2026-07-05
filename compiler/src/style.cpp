#include "style.h"

#include <cstring>

namespace mapbake {

static bool tag_eq(const std::pair<std::string,std::string>& tag,
                   const char* key, const char* value) {
    if (tag.first != key) return false;
    if (!value) return true;
    return tag.second == value;
}

static const StyleRule kRules[] = {
    // Areas
    {"natural", "water",      true,  rgba(0x88, 0xbb, 0xee), 0.0f, 1},
    {"waterway", "riverbank", true,  rgba(0x88, 0xbb, 0xee), 0.0f, 1},
    {"landuse", "forest",     true,  rgba(0xbe, 0xde, 0xad), 0.0f, 2},
    {"landuse", "grass",      true,  rgba(0xbe, 0xde, 0xad), 0.0f, 2},
    {"landuse", "meadow",     true,  rgba(0xbe, 0xde, 0xad), 0.0f, 2},
    {"landuse", "park",       true,  rgba(0xbe, 0xde, 0xad), 0.0f, 2},
    {"landuse", "residential",true,  rgba(0xe8, 0xd8, 0xc0), 0.0f, 2},
    {"landuse", "commercial", true,  rgba(0xe8, 0xd8, 0xc0), 0.0f, 2},
    {"building", nullptr,     true,  rgba(0xc0, 0xa0, 0x80), 0.0f, 4},

    // Lines
    {"highway", "motorway",    false, rgba(0xd4, 0x6a, 0x3a), 4.0f, 3},
    {"highway", "trunk",       false, rgba(0xd4, 0x6a, 0x3a), 4.0f, 3},
    {"highway", "primary",     false, rgba(0xe8, 0x92, 0x2c), 3.0f, 3},
    {"highway", "secondary",   false, rgba(0xfc, 0xd6, 0xa4), 2.0f, 3},
    {"highway", "tertiary",    false, rgba(0xff, 0xff, 0xff), 1.5f, 3},
    {"highway", "residential", false, rgba(0xff, 0xff, 0xff), 1.0f, 3},
    {"highway", "service",     false, rgba(0xff, 0xff, 0xff), 1.0f, 3},
    {"waterway", "river",      false, rgba(0x88, 0xbb, 0xee), 2.0f, 1},
    {"waterway", "stream",     false, rgba(0x88, 0xbb, 0xee), 1.0f, 1},
};

const StyleRule* match_style(const std::vector<std::pair<std::string, std::string>>& tags,
                             bool is_closed) {
    for (const auto& rule : kRules) {
        if (rule.area && !is_closed) continue;
        for (const auto& tag : tags) {
            if (tag_eq(tag, rule.key, rule.value)) return &rule;
        }
    }
    return nullptr;
}

}  // namespace mapbake