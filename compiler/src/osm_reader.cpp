#include "osm_reader.h"

#include <osmium/osm/tag.hpp>

#include <algorithm>

namespace mapbake {

void OsmCollector::way(const osmium::Way& way) {
    if (way.nodes().size() < 2) return;

    std::vector<std::pair<std::string, std::string>> tags;
    bool implies_area = false;
    for (const auto& tag : way.tags()) {
        tags.emplace_back(tag.key(), tag.value());
        if (std::strcmp(tag.key(), "building") == 0 ||
            std::strcmp(tag.key(), "landuse") == 0 ||
            std::strcmp(tag.key(), "natural") == 0) {
            implies_area = true;
        }
    }

    const bool closed = way.is_closed() || implies_area;
    const StyleRule* rule = match_style(tags, closed);
    if (!rule) return;
    if (rule->area) return;  // Areas come from the multipolygon assembler.

    Feature f;
    f.layer = rule->layer;
    f.color = rule->rgba;
    f.is_area = false;
    f.line_width = rule->line_width_z14;

    for (const auto& nr : way.nodes()) {
        const auto& loc = nr.location();
        if (!loc.valid()) continue;
        f.geometry.push_back({loc.lon_without_check(), loc.lat_without_check()});
    }
    if (f.geometry.size() < 2) return;
    features.push_back(std::move(f));
}

}  // namespace mapbake