#pragma once

#include "geometry_clip.h"
#include "style.h"

#include <osmium/handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/flex_mem.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>

#include <string>
#include <vector>

namespace mapbake {

using NodeIndex = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;
using NodeLocations = osmium::handler::NodeLocationsForWays<NodeIndex>;

struct OsmCollector : public osmium::handler::Handler {
    std::vector<Feature> features;

    void way(const osmium::Way& way);
};

}  // namespace mapbake