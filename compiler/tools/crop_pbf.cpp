// Small host-side PBF cropper using libosmium (already a dependency).
// Produces a self-contained .osm.pbf that contains all nodes/ways/relations
// needed to render features inside the requested bbox, including complete
// multipolygon rings that cross the bbox edge.

#include <osmium/io/any_input.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/pbf_output.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/io/writer.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/way.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


struct BBox {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;

    bool contains(const osmium::Location& loc) const {
        return loc.lon() >= min_lon && loc.lon() <= max_lon &&
               loc.lat() >= min_lat && loc.lat() <= max_lat;
    }
};

struct WayRef {
    std::vector<osmium::object_id_type> nodes;
    double min_lon = 180.0;
    double max_lon = -180.0;
    double min_lat = 90.0;
    double max_lat = -90.0;
};

struct RelRef {
    struct Member {
        osmium::item_type type;
        osmium::object_id_type ref;
    };
    std::vector<Member> members;
};

static void print_usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s -i in.osm.pbf -o out.osm.pbf --bbox w,s,e,n\n",
        prog);
}

// Drop ways whose lat/lon span exceeds this many degrees.  Long arterial
// roads crossing the whole city are not needed for local high-zoom testing
// and would otherwise create tiles across a huge area.
constexpr double kMaxWaySpanDeg = 0.025;

int main(int argc, char** argv) {
    std::string in;
    std::string out;
    BBox bbox;
    bool have_bbox = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-i" && i + 1 < argc) {
            in = argv[++i];
        } else if (a == "-o" && i + 1 < argc) {
            out = argv[++i];
        } else if (a == "--bbox" && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%lf,%lf,%lf,%lf",
                            &bbox.min_lon, &bbox.min_lat,
                            &bbox.max_lon, &bbox.max_lat) == 4) {
                have_bbox = true;
            }
        }
    }

    if (in.empty() || out.empty() || !have_bbox) {
        print_usage(argv[0]);
        return 2;
    }

    try {
        std::unordered_map<osmium::object_id_type, osmium::Location> nodes;
        std::unordered_map<osmium::object_id_type, WayRef> ways;
        std::unordered_map<osmium::object_id_type, RelRef> rels;

        // File ordering is not guaranteed, so we read each entity type in a
        // separate pass.
        std::fprintf(stderr, "pass 1a: reading nodes...\n");
        {
            osmium::io::Reader reader(in, osmium::osm_entity_bits::node);
            while (osmium::memory::Buffer buffer = reader.read()) {
                for (const auto& node : buffer.select<osmium::Node>()) {
                    if (node.location().valid()) {
                        nodes[node.id()] = node.location();
                    }
                }
            }
        }

        std::fprintf(stderr, "pass 1b: reading ways...\n");
        {
            osmium::io::Reader reader(in, osmium::osm_entity_bits::way);
            while (osmium::memory::Buffer buffer = reader.read()) {
                for (const auto& way : buffer.select<osmium::Way>()) {
                    WayRef wr;
                    wr.nodes.reserve(way.nodes().size());
                    for (const auto& nr : way.nodes()) {
                        wr.nodes.push_back(nr.ref());
                        const auto it = nodes.find(nr.ref());
                        if (it != nodes.end()) {
                            const osmium::Location& loc = it->second;
                            wr.min_lon = std::min(wr.min_lon, loc.lon());
                            wr.max_lon = std::max(wr.max_lon, loc.lon());
                            wr.min_lat = std::min(wr.min_lat, loc.lat());
                            wr.max_lat = std::max(wr.max_lat, loc.lat());
                        }
                    }
                    ways[way.id()] = std::move(wr);
                }
            }
        }

        std::fprintf(stderr, "pass 1c: reading relations...\n");
        {
            osmium::io::Reader reader(in, osmium::osm_entity_bits::relation);
            while (osmium::memory::Buffer buffer = reader.read()) {
                for (const auto& rel : buffer.select<osmium::Relation>()) {
                    RelRef rr;
                    for (const auto& mem : rel.members()) {
                        rr.members.push_back({mem.type(), mem.ref()});
                    }
                    rels[rel.id()] = std::move(rr);
                }
            }
        }

        std::unordered_set<osmium::object_id_type> keep_nodes;
        std::unordered_set<osmium::object_id_type> keep_ways;
        std::unordered_set<osmium::object_id_type> keep_rels;

        for (const auto& [id, loc] : nodes) {
            if (bbox.contains(loc)) keep_nodes.insert(id);
        }

        std::fprintf(stderr, "pass 2: resolving dependencies...\n");
        for (int iter = 0; iter < 20; ++iter) {
            const size_t before = keep_nodes.size() + keep_ways.size() + keep_rels.size();

            // Ways that touch any kept node and are not huge.
            for (const auto& [id, wr] : ways) {
                if (keep_ways.count(id)) continue;
                const double lon_span = wr.max_lon - wr.min_lon;
                const double lat_span = wr.max_lat - wr.min_lat;
                if (lon_span > kMaxWaySpanDeg || lat_span > kMaxWaySpanDeg) continue;
                for (const auto nid : wr.nodes) {
                    if (keep_nodes.count(nid)) {
                        keep_ways.insert(id);
                        break;
                    }
                }
            }

            // All nodes referenced by kept ways.
            for (const auto wid : keep_ways) {
                const auto it = ways.find(wid);
                if (it == ways.end()) continue;
                for (const auto nid : it->second.nodes) {
                    keep_nodes.insert(nid);
                }
            }

            // Keep only relations whose members are fully available.  This
            // preserves small local multipolygons (buildings, small parks)
            // while avoiding the massive extract blow-up caused by large
            // administrative/landuse relations that span the city.
            for (const auto& [id, rr] : rels) {
                if (keep_rels.count(id)) continue;
                bool all_members_available = true;
                bool touches_bbox = false;
                for (const auto& mem : rr.members) {
                    bool available = false;
                    if (mem.type == osmium::item_type::node) {
                        available = keep_nodes.count(mem.ref);
                        touches_bbox |= available;
                    } else if (mem.type == osmium::item_type::way) {
                        available = keep_ways.count(mem.ref);
                        touches_bbox |= available;
                    } else if (mem.type == osmium::item_type::relation) {
                        available = keep_rels.count(mem.ref);
                        touches_bbox |= available;
                    }
                    if (!available) {
                        all_members_available = false;
                        break;
                    }
                }
                if (all_members_available && touches_bbox) keep_rels.insert(id);
            }

            const size_t after = keep_nodes.size() + keep_ways.size() + keep_rels.size();
            if (after == before) break;
        }

        std::fprintf(stderr, "pass 3: writing output...\n");
        osmium::io::Reader reader(in);
        osmium::io::Header header = reader.header();
        osmium::io::Writer writer(out, header, osmium::io::overwrite::allow);

        while (osmium::memory::Buffer buffer = reader.read()) {
            for (auto& obj : buffer) {
                bool keep = false;
                if (obj.type() == osmium::item_type::node) {
                    keep = keep_nodes.count(static_cast<const osmium::Node&>(obj).id());
                } else if (obj.type() == osmium::item_type::way) {
                    keep = keep_ways.count(static_cast<const osmium::Way&>(obj).id());
                } else if (obj.type() == osmium::item_type::relation) {
                    keep = keep_rels.count(static_cast<const osmium::Relation&>(obj).id());
                }
                if (keep) writer(obj);
            }
        }

        writer.close();
        reader.close();

        std::fprintf(stderr,
            "wrote %s (kept %zu nodes, %zu ways, %zu relations)\n",
            out.c_str(), keep_nodes.size(), keep_ways.size(), keep_rels.size());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
