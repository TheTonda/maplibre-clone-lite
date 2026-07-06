#include "geometry_clip.h"
#include "mbtiles_writer.h"
#include "osm_reader.h"
#include "style.h"
#include "tile_rasterizer.h"
#include "webp_encoder.h"

#include <maprender/mercator.h>

#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/relations/manager_util.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct Options {
    std::string in;
    std::string out;
    int min_z = 8;
    int max_z = 14;
    float quality = 0.85f;
    int lossless_zoom = 17;  // zoom levels >= this use lossless WebP
    int threads = static_cast<int>(std::thread::hardware_concurrency());
};

Options parse_args(int argc, char** argv) {
    Options opt;
    if (opt.threads < 1) opt.threads = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-i" && i + 1 < argc) opt.in = argv[++i];
        else if (a == "-o" && i + 1 < argc) opt.out = argv[++i];
        else if (a == "-z" && i + 1 < argc) opt.min_z = std::atoi(argv[++i]);
        else if (a == "-Z" && i + 1 < argc) opt.max_z = std::atoi(argv[++i]);
        else if (a == "-q" && i + 1 < argc) opt.quality = static_cast<float>(std::atof(argv[++i]));
        else if ((a == "--lossless-zoom" || a == "-l") && i + 1 < argc) opt.lossless_zoom = std::atoi(argv[++i]);
        else if ((a == "--threads" || a == "-t") && i + 1 < argc) opt.threads = std::atoi(argv[++i]);
    }
    if (opt.out.empty()) opt.out = "out.webp.mbtiles";
    return opt;
}

void print_usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s -i region.osm.pbf [-o out.webp.mbtiles] [-z min_z] [-Z max_z] [-q quality]\n"
        "       [--lossless-zoom N] [--threads N]\n",
        prog);
}

struct ProjectedRing {
    std::vector<double> wx;
    std::vector<double> wy;
};

struct ProjectedFeature {
    int layer = 0;
    uint32_t color = 0;
    float line_width = 0.0f;
    bool is_area = false;
    std::vector<double> wx;  // world pixels at z_max (outer ring)
    std::vector<double> wy;
    std::vector<ProjectedRing> inner_rings;
    int tx_min = 0;
    int tx_max = 0;
    int ty_min = 0;
    int ty_max = 0;
};

struct TileKey {
    int z = 0;
    int x = 0;
    int y = 0;
    bool operator==(const TileKey& other) const noexcept {
        return z == other.z && x == other.x && y == other.y;
    }
};

struct TileKeyHash {
    std::size_t operator()(const TileKey& k) const noexcept {
        // Simple 3D integer hash.
        std::size_t h = static_cast<std::size_t>(k.z) * 73856093u;
        h ^= static_cast<std::size_t>(k.x) * 19349663u;
        h ^= static_cast<std::size_t>(k.y) * 83492791u;
        return h;
    }
};

struct TileWork {
    TileKey key;
    std::vector<const ProjectedFeature*> features;
};

std::vector<ProjectedFeature> project_features(
    const std::vector<mapbake::Feature>& features, int z_max) {
    std::vector<ProjectedFeature> out;
    out.reserve(features.size());
    for (const auto& f : features) {
        if (f.geometry.empty()) continue;
        ProjectedFeature pf;
        pf.layer = f.layer;
        pf.color = f.color;
        pf.line_width = f.line_width;
        pf.is_area = f.is_area;
        pf.wx.reserve(f.geometry.size());
        pf.wy.reserve(f.geometry.size());
        int tx_min = std::numeric_limits<int>::max();
        int tx_max = std::numeric_limits<int>::min();
        int ty_min = std::numeric_limits<int>::max();
        int ty_max = std::numeric_limits<int>::min();
        auto project_ring = [&](const mapbake::Ring& ring,
                                std::vector<double>& out_wx,
                                std::vector<double>& out_wy) {
            for (const auto& p : ring) {
                const double wx = maprender::lon_to_world_x(p.x, z_max);
                const double wy = maprender::lat_to_world_y(p.y, z_max);
                out_wx.push_back(wx);
                out_wy.push_back(wy);
                const int tx = static_cast<int>(std::floor(wx / maprender::kTileSize));
                const int ty = static_cast<int>(std::floor(wy / maprender::kTileSize));
                tx_min = std::min(tx_min, tx);
                tx_max = std::max(tx_max, tx);
                ty_min = std::min(ty_min, ty);
                ty_max = std::max(ty_max, ty);
            }
        };

        project_ring(f.geometry, pf.wx, pf.wy);
        for (const auto& hole : f.inner_rings) {
            ProjectedRing pr;
            project_ring(hole, pr.wx, pr.wy);
            if (!pr.wx.empty()) pf.inner_rings.push_back(std::move(pr));
        }

        if (pf.wx.empty()) continue;
        pf.tx_min = tx_min;
        pf.tx_max = tx_max;
        pf.ty_min = ty_min;
        pf.ty_max = ty_max;
        out.push_back(std::move(pf));
    }
    return out;
}

std::vector<TileWork> bucket_tiles(
    const std::vector<ProjectedFeature>& features, int z, int z_max) {
    const int diff = z_max - z;
    const int max_xy = static_cast<int>(1u << z);
    std::unordered_map<TileKey, std::vector<const ProjectedFeature*>, TileKeyHash> buckets;
    buckets.reserve(features.size() * 4);

    for (const auto& pf : features) {
        int x0 = (pf.tx_min >> diff) - 1;
        int x1 = (pf.tx_max >> diff) + 1;
        int y0 = (pf.ty_min >> diff) - 1;
        int y1 = (pf.ty_max >> diff) + 1;
        x0 = std::max(0, x0);
        x1 = std::min(max_xy - 1, x1);
        y0 = std::max(0, y0);
        y1 = std::min(max_xy - 1, y1);
        if (x0 > x1 || y0 > y1) continue;

        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                buckets[{z, tx, ty}].push_back(&pf);
            }
        }
    }

    std::vector<TileWork> work;
    work.reserve(buckets.size());
    for (auto& kv : buckets) {
        work.push_back({kv.first, std::move(kv.second)});
    }
    return work;
}

void render_tiles(size_t start, size_t end,
                  const std::vector<TileWork>& work,
                  int z, int z_max,
                  float line_scale,
                  bool lossless, float webp_quality,
                  std::vector<std::vector<uint8_t>>& out) {
    mapbake::TileRasterizer rasterizer;
    constexpr double kClipMargin = 2.0;
    const double scale = 1.0 / static_cast<double>(1u << (z_max - z));
    const double tile_size = maprender::kTileSize;

    for (size_t idx = start; idx < end; ++idx) {
        const auto& tw = work[idx];
        rasterizer.clear();

        // Stable painter order: lower layers first.
        std::vector<const ProjectedFeature*> ordered = tw.features;
        std::sort(ordered.begin(), ordered.end(),
            [](const ProjectedFeature* a, const ProjectedFeature* b) {
                return a->layer < b->layer;
            });

        const double ox = static_cast<double>(tw.key.x) * tile_size;
        const double oy = static_cast<double>(tw.key.y) * tile_size;

        for (const ProjectedFeature* pf : ordered) {
            auto make_local = [&](const std::vector<double>& wx,
                                  const std::vector<double>& wy) {
                mapbake::Ring local;
                local.reserve(wx.size());
                for (size_t i = 0; i < wx.size(); ++i) {
                    local.push_back({wx[i] * scale - ox,
                                     wy[i] * scale - oy});
                }
                return local;
            };

            if (pf->is_area) {
                mapbake::Ring outer = make_local(pf->wx, pf->wy);
                if (outer.front().x != outer.back().x ||
                    outer.front().y != outer.back().y) {
                    outer.push_back(outer.front());
                }
                auto clipped_outer = mapbake::clip_to_rect(
                    outer, maprender::kTileSize, kClipMargin, true);

                std::vector<mapbake::Ring> clipped_holes;
                for (const auto& hole : pf->inner_rings) {
                    mapbake::Ring local_hole = make_local(hole.wx, hole.wy);
                    if (local_hole.front().x != local_hole.back().x ||
                        local_hole.front().y != local_hole.back().y) {
                        local_hole.push_back(local_hole.front());
                    }
                    auto clipped_hole = mapbake::clip_to_rect(
                        local_hole, maprender::kTileSize, kClipMargin, true);
                    if (clipped_hole.size() >= 3) {
                        clipped_holes.push_back(std::move(clipped_hole));
                    }
                }

                if (clipped_outer.size() >= 3) {
                    rasterizer.draw_area(clipped_outer, clipped_holes, pf->color);
                }
            } else {
                mapbake::Ring local = make_local(pf->wx, pf->wy);
                if (local.size() >= 2) {
                    rasterizer.draw_line(local, pf->line_width * line_scale, pf->color);
                }
            }
        }

        out[idx] = lossless
            ? mapbake::encode_webp_lossless(rasterizer.rgba8())
            : mapbake::encode_webp(rasterizer.rgba8(), webp_quality);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(argv[0]); return 2; }
    const Options opt = parse_args(argc, argv);
    if (opt.in.empty()) { std::fprintf(stderr, "missing -i\n"); print_usage(argv[0]); return 2; }
    if (opt.min_z > opt.max_z) { std::fprintf(stderr, "min zoom > max zoom\n"); return 2; }

    try {
        osmium::area::Assembler::config_type assembler_config;
        osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{assembler_config};

        std::fprintf(stderr, "pass 1: reading relations...\n");
        osmium::io::File input_file(opt.in);
        osmium::relations::read_relations(input_file, mp_manager);

        std::fprintf(stderr, "pass 2: reading nodes, ways and assembling areas...\n");
        osmium::io::Reader reader(opt.in,
            osmium::osm_entity_bits::node | osmium::osm_entity_bits::way);
        mapbake::NodeIndex index;
        mapbake::NodeLocations locs(index);
        mapbake::OsmCollector collector;

        auto area_handler = mp_manager.handler([&](const osmium::memory::Buffer& buffer) {
            for (const auto& area : buffer.select<osmium::Area>()) {
                std::vector<std::pair<std::string, std::string>> tags;
                for (const auto& tag : area.tags()) {
                    tags.emplace_back(tag.key(), tag.value());
                }
                const mapbake::StyleRule* rule = mapbake::match_style(tags, true);
                if (!rule) continue;

                for (const auto& outer : area.outer_rings()) {
                    mapbake::Feature f;
                    f.layer = rule->layer;
                    f.color = rule->rgba;
                    f.is_area = true;
                    f.line_width = 0.0f;
                    f.geometry.reserve(outer.size());
                    for (const auto& node : outer) {
                        f.geometry.push_back(mapbake::Point{node.lon(), node.lat()});
                    }
                    for (const auto& inner : area.inner_rings(outer)) {
                        mapbake::Ring hole;
                        hole.reserve(inner.size());
                        for (const auto& node : inner) {
                            hole.push_back(mapbake::Point{node.lon(), node.lat()});
                        }
                        if (hole.size() >= 3) f.inner_rings.push_back(std::move(hole));
                    }
                    if (f.geometry.size() >= 3) {
                        collector.features.push_back(std::move(f));
                    }
                }
            }
        });

        osmium::apply(reader, locs, area_handler, collector);
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

        std::fprintf(stderr, "projecting features once at z=%d...\n", opt.max_z);
        const auto sources = project_features(collector.features, opt.max_z);
        std::fprintf(stderr, "projected %zu features\n", sources.size());

        for (int z = opt.min_z; z <= opt.max_z; ++z) {
            auto work = bucket_tiles(sources, z, opt.max_z);
            std::fprintf(stderr, "z=%d  tiles=%zu\n", z, work.size());
            const int max_xy = static_cast<int>(1u << z);
            const float line_scale = std::pow(2.0f, static_cast<float>(z - 14) * 0.5f);
            const bool lossless = (z >= opt.lossless_zoom);
            const float webp_quality = lossless ? 100.0f : opt.quality * 100.0f;

            std::vector<std::vector<uint8_t>> encoded(work.size());

            const size_t nthreads = static_cast<size_t>(opt.threads);
            std::vector<std::thread> threads;
            threads.reserve(nthreads);
            const size_t chunk = (work.size() + nthreads - 1) / nthreads;
            for (size_t t = 0; t < nthreads; ++t) {
                const size_t start = std::min(t * chunk, work.size());
                const size_t end = std::min((t + 1) * chunk, work.size());
                if (start >= end) continue;
                threads.emplace_back(render_tiles,
                                     start, end,
                                     std::cref(work),
                                     z, opt.max_z,
                                     line_scale,
                                     lossless, webp_quality,
                                     std::ref(encoded));
            }
            for (auto& th : threads) th.join();

            writer.begin_batch();
            for (size_t i = 0; i < work.size(); ++i) {
                if (encoded[i].empty()) continue;
                const int tms_row = max_xy - 1 - work[i].key.y;
                writer.write_tile(z, work[i].key.x, tms_row, encoded[i]);
            }
            writer.end_batch();
        }

        writer.close();
        std::fprintf(stderr, "wrote %s\n", opt.out.c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
