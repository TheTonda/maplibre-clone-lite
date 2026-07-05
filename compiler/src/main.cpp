#include "geometry_clip.h"
#include "mbtiles_writer.h"
#include "osm_reader.h"
#include "style.h"
#include "tile_baker.h"
#include "tile_rasterizer.h"
#include "webp_encoder.h"

#include <maprender/mercator.h>

#include <osmium/io/any_input.hpp>
#include <osmium/visitor.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

struct Options {
    std::string in;
    std::string out;
    int min_z = 8;
    int max_z = 14;
    float quality = 0.85f;
};

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-i" && i + 1 < argc) opt.in = argv[++i];
        else if (a == "-o" && i + 1 < argc) opt.out = argv[++i];
        else if (a == "-z" && i + 1 < argc) opt.min_z = std::atoi(argv[++i]);
        else if (a == "-Z" && i + 1 < argc) opt.max_z = std::atoi(argv[++i]);
        else if (a == "-q" && i + 1 < argc) opt.quality = std::atof(argv[++i]);
    }
    if (opt.out.empty()) opt.out = "out.webp.mbtiles";
    return opt;
}

void print_usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s -i region.osm.pbf [-o out.webp.mbtiles] [-z min_z] [-Z max_z] [-q quality]\n",
        prog);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(argv[0]); return 2; }
    const Options opt = parse_args(argc, argv);
    if (opt.in.empty()) { std::fprintf(stderr, "missing -i\n"); print_usage(argv[0]); return 2; }

    try {
        osmium::io::Reader reader(opt.in, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way);
        mapbake::NodeIndex index;
        mapbake::NodeLocations locs(index);
        mapbake::OsmCollector collector;
        osmium::apply(reader, locs, collector);
        reader.close();

        std::fprintf(stderr, "collected %zu features\n", collector.features.size());

        if (collector.features.empty()) {
            std::fprintf(stderr, "no renderable features\n");
            return 1;
        }

        // Compute bbox from all geometry for metadata.
        double min_lon = 180, max_lon = -180, min_lat = 90, max_lat = -90;
        for (const auto& f : collector.features) {
            for (const auto& p : f.geometry) {
                min_lon = std::min(min_lon, p.x);
                max_lon = std::max(max_lon, p.x);
                min_lat = std::min(min_lat, p.y);
                max_lat = std::max(max_lat, p.y);
            }
        }
        char bounds[256];
        std::snprintf(bounds, sizeof(bounds), "%.5f,%.5f,%.5f,%.5f",
                      min_lon, min_lat, max_lon, max_lat);

        mapbake::MBTilesWriter writer(opt.out);
        if (!writer.open(opt.min_z, opt.max_z, bounds, opt.out)) {
            std::fprintf(stderr, "cannot open output\n");
            return 1;
        }

        mapbake::TileRasterizer rasterizer;
        for (int z = opt.min_z; z <= opt.max_z; ++z) {
            auto buckets = mapbake::bucket_features(collector.features, z);
            std::fprintf(stderr, "z=%d  tiles=%zu\n", z, buckets.size());
            const int max_xy = 1 << z;
            for (auto& kv : buckets) {
                const auto& key = kv.first;
                const auto& feats = kv.second;
                rasterizer.clear();
                // Sort by layer so areas draw before roads.
                std::vector<const mapbake::Feature*> ordered;
                for (const auto& f : feats) ordered.push_back(&f);
                std::sort(ordered.begin(), ordered.end(),
                    [](const mapbake::Feature* a, const mapbake::Feature* b) {
                        return a->layer < b->layer;
                    });

                for (const mapbake::Feature* f : ordered) {
                    mapbake::Ring projected;
                    projected.reserve(f->geometry.size());
                    for (const auto& p : f->geometry) {
                        const double wx = maprender::lon_to_world_x(p.x, z);
                        const double wy = maprender::lat_to_world_y(p.y, z);
                        projected.push_back({wx - key.x * maprender::kTileSize,
                                             wy - key.y * maprender::kTileSize});
                    }
                    if (f->is_area) {
                        if (projected.front().x != projected.back().x ||
                            projected.front().y != projected.back().y) {
                            projected.push_back(projected.front());
                        }
                        mapbake::Ring clipped = mapbake::clip_to_rect(projected, maprender::kTileSize, true);
                        if (clipped.size() >= 3) rasterizer.draw_area(clipped, f->color);
                    } else {
                        mapbake::Ring clipped = mapbake::clip_to_rect(projected, maprender::kTileSize, false);
                        if (clipped.size() >= 2) {
                            // Scale line width from z14 reference.
                            const float scale = std::pow(2.0f, static_cast<float>(z - 14));
                            rasterizer.draw_line(clipped, f->line_width * scale, f->color);
                        }
                    }
                }

                const int tms_row = max_xy - 1 - key.y;
                auto blob = mapbake::encode_webp(rasterizer.rgba8(), opt.quality * 100.0f);
                if (!blob.empty()) {
                    writer.write_tile(z, key.x, tms_row, blob);
                }
            }
        }
        writer.close();
        std::fprintf(stderr, "wrote %s\n", opt.out.c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}