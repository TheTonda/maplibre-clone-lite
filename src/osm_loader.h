#pragma once
// osm_loader.h — Lightweight JSON parser and OSM data loader
// Loads extracted OSM geometry (buildings, roads, parks, water) from JSON
// produced by tools/extract_geometry.py and converts lat/lon to Mercator coords.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace osm {

// ─── Simple JSON tokenizer and parser ──────────────────────────────────

namespace json {

inline void skip_ws(const char*& p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
}

inline std::string read_string(const char*& p) {
    if (*p != '"') return {};
    ++p;
    std::string s;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            ++p;
            switch (*p) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case '/': s += '/'; break;
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                default: s += *p; break;
            }
        } else {
            s += *p;
        }
        ++p;
    }
    if (*p == '"') ++p;
    return s;
}

inline double read_number(const char*& p) {
    const char* start = p;
    if (*p == '-') ++p;
    while (*p >= '0' && *p <= '9') ++p;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    return std::atof(start);
}

enum class JsonKind {
    Null, Bool, Number, String, Array, Object
};

struct JsonValue {
    JsonKind kind = JsonKind::Null;
    double num = 0;
    bool bval = false;
    std::string sval;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    bool is_null() const { return kind == JsonKind::Null; }
    bool is_number() const { return kind == JsonKind::Number; }
    bool is_string() const { return kind == JsonKind::String; }
    bool is_array() const { return kind == JsonKind::Array; }
    bool is_object() const { return kind == JsonKind::Object; }
    bool is_bool() const { return kind == JsonKind::Bool; }

    double as_number(double def = 0) const { return is_number() ? num : def; }
    int as_int(int def = 0) const { return is_number() ? static_cast<int>(num) : def; }
    std::string as_string(const std::string& def = "") const { return is_string() ? sval : def; }
    bool as_bool(bool def = false) const { return is_bool() ? bval : def; }

    const JsonValue* find(const std::string& key) const {
        if (kind != JsonKind::Object) return nullptr;
        for (auto& [k, v] : obj) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    const JsonValue& operator[](size_t i) const { return arr[i]; }
    size_t size() const {
        if (kind == JsonKind::Array) return arr.size();
        if (kind == JsonKind::Object) return obj.size();
        return 0;
    }
};

inline JsonValue parse_value(const char*& p);

inline JsonValue parse_object(const char*& p) {
    JsonValue v;
    v.kind = JsonKind::Object;
    if (*p != '{') return v;
    ++p;
    skip_ws(p);
    if (*p == '}') { ++p; return v; }
    while (*p) {
        skip_ws(p);
        if (*p == '}') { ++p; break; }
        if (*p != '"') break;
        std::string key = read_string(p);
        skip_ws(p);
        if (*p == ':') ++p;
        skip_ws(p);
        JsonValue val = parse_value(p);
        v.obj.push_back({std::move(key), std::move(val)});
        skip_ws(p);
        if (*p == ',') ++p;
    }
    return v;
}

inline JsonValue parse_array(const char*& p) {
    JsonValue v;
    v.kind = JsonKind::Array;
    if (*p != '[') return v;
    ++p;
    skip_ws(p);
    if (*p == ']') { ++p; return v; }
    while (*p) {
        skip_ws(p);
        if (*p == ']') { ++p; break; }
        JsonValue val = parse_value(p);
        v.arr.push_back(std::move(val));
        skip_ws(p);
        if (*p == ',') ++p;
    }
    return v;
}

inline JsonValue parse_value(const char*& p) {
    skip_ws(p);
    JsonValue v;
    if (*p == '"' ) {
        v.kind = JsonKind::String;
        v.sval = read_string(p);
    } else if (*p == '{') {
        v = parse_object(p);
    } else if (*p == '[') {
        v = parse_array(p);
    } else if (*p == 't') { v.kind = JsonKind::Bool; v.bval = true; p += 4; }
    else if (*p == 'f') { v.kind = JsonKind::Bool; v.bval = false; p += 5; }
    else if (*p == 'n') { v.kind = JsonKind::Null; p += 4; }
    else if (*p == '-' || (*p >= '0' && *p <= '9')) {
        v.kind = JsonKind::Number;
        v.num = read_number(p);
    }
    return v;
}

inline JsonValue parse(const char* text) {
    const char* p = text;
    return parse_value(p);
}

} // namespace json

// ─── Data structures ───────────────────────────────────────────────────

struct MercatorPoint {
    float x, y;  // web mercator coordinates (world space)
};

struct Building {
    int64_t id = 0;
    std::string name;
    std::string type;
    float height = 0;
    float color[3] = {0.6f, 0.58f, 0.55f};
    std::vector<MercatorPoint> footprint;  // 2D polygon in mercator space
};

struct Road {
    int64_t id = 0;
    std::string name;
    std::string type;
    float color[3] = {0.55f, 0.50f, 0.40f};
    float width = 1.0f;
    std::vector<MercatorPoint> line;  // ordered points
};

struct Park {
    int64_t id = 0;
    std::string name;
    std::string type;
    std::vector<MercatorPoint> polygon;
};

struct WaterPolygon {
    int64_t id = 0;
    std::string name;
    std::vector<MercatorPoint> polygon;
};

struct WaterLine {
    int64_t id = 0;
    std::string name;
    std::string type;
    std::vector<MercatorPoint> line;
};

struct Landuse {
    int64_t id = 0;
    std::string name;
    std::string type;
    std::vector<MercatorPoint> polygon;
};

struct OSMData {
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<Park> parks;
    std::vector<WaterPolygon> water_polygons;
    std::vector<WaterLine> water_lines;
    std::vector<Landuse> landuse;
};

// ─── Web Mercator projection ──────────────────────────────────────────

inline float lon_to_x(double lon, double world_width) {
    return static_cast<float>((lon + 180.0) / 360.0 * world_width);
}

inline float lat_to_y(double lat, double world_height) {
    double lat_rad = lat * M_PI / 180.0;
    double n = std::log(std::tan(M_PI / 4.0 + lat_rad / 2.0));
    return static_cast<float>((1.0 - n / M_PI) / 2.0 * world_height);
}

inline MercatorPoint lonlat_to_mercator(double lon, double lat, double world_width, double world_height) {
    return {lon_to_x(lon, world_width), lat_to_y(lat, world_height)};
}

// ─── JSON → C++ data conversion ───────────────────────────────────────

inline OSMData load_osm_json(const std::string& path, double world_width = 65536.0, double world_height = 65536.0) {
    OSMData data;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        fprintf(stderr, "osm_loader: Failed to open %s\n", path.c_str());
        return data;
    }
    size_t sz = static_cast<size_t>(file.tellg());
    file.seekg(0);
    std::string content(sz, '\0');
    file.read(content.data(), sz);
    file.close();

    auto root = json::parse(content.c_str());

    auto parse_polygon = [&](const json::JsonValue& arr) -> std::vector<MercatorPoint> {
        std::vector<MercatorPoint> pts;
        if (arr.is_array() && arr.size() > 0) {
            // Check if first element is an array (nested [lon, lat] pairs)
            const auto& first = arr[0];
            if (first.is_array() && first.size() >= 2) {
                for (size_t i = 0; i < arr.size(); ++i) {
                    double lon = arr[i][0].as_number();
                    double lat = arr[i][1].as_number();
                    pts.push_back(lonlat_to_mercator(lon, lat, world_width, world_height));
                }
            } else {
                // Flat array [lon, lat, lon, lat, ...]
                for (size_t i = 0; i + 1 < arr.size(); i += 2) {
                    double lon = arr[i].as_number();
                    double lat = arr[i + 1].as_number();
                    pts.push_back(lonlat_to_mercator(lon, lat, world_width, world_height));
                }
            }
        }
        return pts;
    };

    auto parse_line = [&](const json::JsonValue& arr) -> std::vector<MercatorPoint> {
        return parse_polygon(arr);
    };

    // Parse buildings
    if (auto* bldg_arr = root.find("buildings"); bldg_arr && bldg_arr->is_array()) {
        for (size_t i = 0; i < bldg_arr->size(); ++i) {
            const auto& obj = (*bldg_arr)[i];
            Building b;
            if (auto* v = obj.find("id")) b.id = v->as_int();
            if (auto* v = obj.find("name")) b.name = v->as_string();
            if (auto* v = obj.find("type")) b.type = v->as_string(); else b.type = "yes";
            if (auto* v = obj.find("height")) b.height = static_cast<float>(v->as_number(5.0));
            if (auto* c = obj.find("color"); c && c->is_array() && c->size() >= 3) {
                b.color[0] = static_cast<float>((*c)[0].as_number());
                b.color[1] = static_cast<float>((*c)[1].as_number());
                b.color[2] = static_cast<float>((*c)[2].as_number());
            }
            if (auto* poly = obj.find("polygon"); poly && poly->is_array()) {
                b.footprint = parse_polygon(*poly);
            }
            if (!b.footprint.empty()) {
                data.buildings.push_back(std::move(b));
            }
        }
    }

    // Parse roads
    if (auto* roads_arr = root.find("roads"); roads_arr && roads_arr->is_array()) {
        for (size_t i = 0; i < roads_arr->size(); ++i) {
            const auto& obj = (*roads_arr)[i];
            Road r;
            if (auto* v = obj.find("id")) r.id = v->as_int();
            if (auto* v = obj.find("name")) r.name = v->as_string();
            if (auto* v = obj.find("type")) r.type = v->as_string(); else r.type = "residential";
            if (auto* v = obj.find("width")) r.width = static_cast<float>(v->as_number(1.0));
            if (auto* c = obj.find("color"); c && c->is_array() && c->size() >= 3) {
                r.color[0] = static_cast<float>((*c)[0].as_number());
                r.color[1] = static_cast<float>((*c)[1].as_number());
                r.color[2] = static_cast<float>((*c)[2].as_number());
            }
            if (auto* line = obj.find("line"); line && line->is_array()) {
                r.line = parse_line(*line);
            }
            if (!r.line.empty()) {
                data.roads.push_back(std::move(r));
            }
        }
    }

    // Parse parks
    if (auto* parks_arr = root.find("parks"); parks_arr && parks_arr->is_array()) {
        for (size_t i = 0; i < parks_arr->size(); ++i) {
            const auto& obj = (*parks_arr)[i];
            Park p;
            if (auto* v = obj.find("id")) p.id = v->as_int();
            if (auto* v = obj.find("name")) p.name = v->as_string();
            if (auto* v = obj.find("type")) p.type = v->as_string(); else p.type = "park";
            if (auto* poly = obj.find("polygon"); poly && poly->is_array()) {
                p.polygon = parse_polygon(*poly);
            }
            if (!p.polygon.empty()) {
                data.parks.push_back(std::move(p));
            }
        }
    }

    // Parse water polygons
    if (auto* wp_arr = root.find("water_polygons"); wp_arr && wp_arr->is_array()) {
        for (size_t i = 0; i < wp_arr->size(); ++i) {
            const auto& obj = (*wp_arr)[i];
            WaterPolygon w;
            if (auto* v = obj.find("id")) w.id = v->as_int();
            if (auto* v = obj.find("name")) w.name = v->as_string();
            if (auto* poly = obj.find("polygon"); poly && poly->is_array()) {
                w.polygon = parse_polygon(*poly);
            }
            if (!w.polygon.empty()) {
                data.water_polygons.push_back(std::move(w));
            }
        }
    }

    // Parse water lines
    if (auto* wl_arr = root.find("water_lines"); wl_arr && wl_arr->is_array()) {
        for (size_t i = 0; i < wl_arr->size(); ++i) {
            const auto& obj = (*wl_arr)[i];
            WaterLine w;
            if (auto* v = obj.find("id")) w.id = v->as_int();
            if (auto* v = obj.find("name")) w.name = v->as_string();
            if (auto* v = obj.find("type")) w.type = v->as_string(); else w.type = "river";
            if (auto* line = obj.find("line"); line && line->is_array()) {
                w.line = parse_line(*line);
            }
            if (!w.line.empty()) {
                data.water_lines.push_back(std::move(w));
            }
        }
    }

    // Parse landuse
    if (auto* lu_arr = root.find("landuse"); lu_arr && lu_arr->is_array()) {
        for (size_t i = 0; i < lu_arr->size(); ++i) {
            const auto& obj = (*lu_arr)[i];
            Landuse l;
            if (auto* v = obj.find("id")) l.id = v->as_int();
            if (auto* v = obj.find("name")) l.name = v->as_string();
            if (auto* v = obj.find("type")) l.type = v->as_string(); else l.type = "residential";
            if (auto* poly = obj.find("polygon"); poly && poly->is_array()) {
                l.polygon = parse_polygon(*poly);
            }
            if (!l.polygon.empty()) {
                data.landuse.push_back(std::move(l));
            }
        }
    }

    printf("osm_loader: Loaded %zu buildings, %zu roads, %zu parks, %zu water polys, "
           "%zu water lines, %zu landuse\n",
           data.buildings.size(), data.roads.size(), data.parks.size(),
           data.water_polygons.size(), data.water_lines.size(), data.landuse.size());

    return data;
}

} // namespace osm
