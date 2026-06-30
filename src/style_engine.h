#pragma once
// style_engine.h — Header-only simplified MapLibre-style JSON parser
// Parses layers with id, type, paint (fill-color, line-color, etc.)
// Supports hex colors (#rrggbb). No external JSON library.

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifdef MAP_RENDERER_DEBUG
#define DEBUG_LOG(...) std::printf("[DEBUG] " __VA_ARGS__); std::printf("\n")
#else
#define DEBUG_LOG(...) ((void)0)
#endif

namespace style {

// ─── StyleRule: material parameters for a matched layer ────────────

struct StyleRule {
    float fill_color[3]   = {0.5f, 0.5f, 0.5f};  // RGB 0-1
    float fill_opacity    = 1.0f;
    float line_color[3]   = {1.0f, 1.0f, 1.0f};
    float line_width      = 1.0f;
    float line_opacity    = 1.0f;
    float extrude_color[3] = {0.6f, 0.58f, 0.55f}; // RGB 0-1 for fill-extrusion
    float extrude_opacity  = 0.9f;
};

// ─── Internal layer representation ─────────────────────────────────

struct PaintProperties {
    std::optional<std::array<float, 3>> fill_color;
    std::optional<float>                 fill_opacity;
    std::optional<std::array<float, 3>> line_color;
    std::optional<float>                 line_width;
    std::optional<float>                 line_opacity;
    std::optional<std::array<float, 3>> background_color;
    std::optional<std::array<float, 3>> fill_extrusion_color;
    std::optional<float>                 fill_extrusion_opacity;
};

struct StyleLayer {
    std::string      id;
    std::string      type;   // "fill", "line", "background", "symbol"
    PaintProperties  paint;
};

// ─── Hex color parsing ──────────────────────────────────────────────

/// Parse "#rrggbb" → float RGB.
inline std::optional<std::array<float, 3>> parse_hex_color(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') return std::nullopt;
    for (int i = 1; i < 7; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i])))
            return std::nullopt;
    }
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    float r = static_cast<float>(hex(s[1]) * 16 + hex(s[2])) / 255.0f;
    float g = static_cast<float>(hex(s[3]) * 16 + hex(s[4])) / 255.0f;
    float b = static_cast<float>(hex(s[5]) * 16 + hex(s[6])) / 255.0f;
    return std::array<float, 3>{r, g, b};
}

// ─── Style Engine ──────────────────────────────────────────────────

class StyleEngine {
public:
    StyleEngine() = default;

    /// Load a simplified MapLibre-style JSON from a file path.
    bool loadFromJson(const std::string& path) {
        std::ifstream f(path);
        if (!f) {
            std::fprintf(stderr, "StyleEngine: cannot open %s\n", path.c_str());
            return false;
        }
        std::string src((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
        return parse(src);
    }

    /// Match a style rule by layer name and geometry type string
    /// ("fill", "line", "symbol", "background", "fill-extrusion").
    /// Returns default gray if no match.
    StyleRule matchRule(const std::string& layer_name,
                        const std::string& geom_type) const {
        DEBUG_LOG("matchRule: layer='%s', type='%s', layers=%zu",
                  layer_name.c_str(), geom_type.c_str(), layers_.size());
        for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
            if (it->id == layer_name || it->id == "*") {
                if (it->type == geom_type || it->type == "background" ||
                    it->type == "fill-extrusion") {
                    DEBUG_LOG("  matched: id='%s' type='%s'", it->id.c_str(), it->type.c_str());
                    return buildRule(*it);
                }
            }
        }
        DEBUG_LOG("  no match, returning default");
        return StyleRule{};
    }

    /// Match a style rule by layer name and MVT geometry type int
    /// (1=POINT, 2=LINESTRING, 3=POLYGON).
    StyleRule matchRule(const std::string& layer_name, int mvt_geom_type) const {
        const char* geom = "fill";
        if (mvt_geom_type == 1)      geom = "symbol";
        else if (mvt_geom_type == 2) geom = "line";
        else if (mvt_geom_type == 3) geom = "fill";
        return matchRule(layer_name, std::string(geom));
    }

    const std::vector<StyleLayer>& layers() const { return layers_; }

    /// Print loaded layers (for debugging).
    void print() const {
        std::printf("=== Style Layers (%zu) ===\n", layers_.size());
        for (const auto& l : layers_) {
            std::printf("  id=%-20s type=%-12s", l.id.c_str(), l.type.c_str());
            if (l.paint.fill_color)
                std::printf(" fill=#%02x%02x%02x",
                    int((*l.paint.fill_color)[0]*255),
                    int((*l.paint.fill_color)[1]*255),
                    int((*l.paint.fill_color)[2]*255));
            if (l.paint.line_color)
                std::printf(" line=#%02x%02x%02x",
                    int((*l.paint.line_color)[0]*255),
                    int((*l.paint.line_color)[1]*255),
                    int((*l.paint.line_color)[2]*255));
            if (l.paint.background_color)
                std::printf(" bg=#%02x%02x%02x",
                    int((*l.paint.background_color)[0]*255),
                    int((*l.paint.background_color)[1]*255),
                    int((*l.paint.background_color)[2]*255));
            std::printf("\n");
        }
    }

private:
    std::vector<StyleLayer> layers_;

    StyleRule buildRule(const StyleLayer& layer) const {
        StyleRule rule;
        const auto& p = layer.paint;
        if (p.fill_color) {
            rule.fill_color[0] = (*p.fill_color)[0];
            rule.fill_color[1] = (*p.fill_color)[1];
            rule.fill_color[2] = (*p.fill_color)[2];
        }
        if (p.fill_opacity) rule.fill_opacity = *p.fill_opacity;
        if (p.line_color) {
            rule.line_color[0] = (*p.line_color)[0];
            rule.line_color[1] = (*p.line_color)[1];
            rule.line_color[2] = (*p.line_color)[2];
        }
        if (p.line_width)  rule.line_width  = *p.line_width;
        if (p.line_opacity) rule.line_opacity = *p.line_opacity;
        if (p.background_color && layer.type == "background") {
            rule.fill_color[0] = (*p.background_color)[0];
            rule.fill_color[1] = (*p.background_color)[1];
            rule.fill_color[2] = (*p.background_color)[2];
        }
        if (p.fill_extrusion_color) {
            rule.extrude_color[0] = (*p.fill_extrusion_color)[0];
            rule.extrude_color[1] = (*p.fill_extrusion_color)[1];
            rule.extrude_color[2] = (*p.fill_extrusion_color)[2];
        }
        if (p.fill_extrusion_opacity) rule.extrude_opacity = *p.fill_extrusion_opacity;
        return rule;
    }

    // ─── JSON parser (manual recursive-descent, no library) ────────

    struct Ctx {
        const std::string& src;
        size_t pos = 0;
        bool ok  = true;
    };

    bool parse(const std::string& src) {
        layers_.clear();
        Ctx ctx{src, 0, true};
        skip_ws(ctx);
        if (!expect(ctx, '{')) return false;

        while (ctx.ok && ctx.pos < ctx.src.size()) {
            skip_ws(ctx);
            char c = peek(ctx);
            if (c == '}') { ctx.pos++; break; }
            if (c == ',') { ctx.pos++; continue; }

            std::string key = parse_string(ctx);
            skip_ws(ctx);
            if (!expect(ctx, ':')) return false;
            skip_ws(ctx);

            if (key == "layers") {
                parse_layers(ctx);
            } else {
                skip_value(ctx);
            }
        }
        return ctx.ok;
    }

    void parse_layers(Ctx& ctx) {
        if (!expect(ctx, '[')) return;
        while (ctx.ok && ctx.pos < ctx.src.size()) {
            skip_ws(ctx);
            char c = peek(ctx);
            if (c == ']') { ctx.pos++; return; }
            if (c == ',') { ctx.pos++; continue; }
            StyleLayer layer = parse_layer(ctx);
            if (!layer.id.empty())
                layers_.push_back(std::move(layer));
        }
    }

    StyleLayer parse_layer(Ctx& ctx) {
        StyleLayer layer;
        if (!expect(ctx, '{')) return layer;

        while (ctx.ok && ctx.pos < ctx.src.size()) {
            skip_ws(ctx);
            char c = peek(ctx);
            if (c == '}') { ctx.pos++; break; }
            if (c == ',') { ctx.pos++; continue; }

            std::string key = parse_string(ctx);
            skip_ws(ctx);
            if (!expect(ctx, ':')) break;
            skip_ws(ctx);

            if (key == "id") {
                layer.id = parse_string(ctx);
            } else if (key == "type") {
                layer.type = parse_string(ctx);
            } else if (key == "paint") {
                layer.paint = parse_paint(ctx);
            } else {
                skip_value(ctx);
            }
        }
        return layer;
    }

    PaintProperties parse_paint(Ctx& ctx) {
        PaintProperties pp;
        if (!expect(ctx, '{')) return pp;

        while (ctx.ok && ctx.pos < ctx.src.size()) {
            skip_ws(ctx);
            char c = peek(ctx);
            if (c == '}') { ctx.pos++; break; }
            if (c == ',') { ctx.pos++; continue; }

            std::string key = parse_string(ctx);
            skip_ws(ctx);
            if (!expect(ctx, ':')) break;
            skip_ws(ctx);

            if (key == "fill-color" || key == "line-color" ||
                key == "background-color" || key == "fill-extrusion-color") {
                std::string color_str = parse_string(ctx);
                auto color = parse_hex_color(color_str);
                if (color) {
                    if (key == "fill-color")            pp.fill_color            = color;
                    else if (key == "line-color")       pp.line_color            = color;
                    else if (key == "background-color") pp.background_color      = color;
                    else                                pp.fill_extrusion_color  = color;
                }
            } else if (key == "fill-opacity" || key == "line-opacity") {
                float val = static_cast<float>(parse_number(ctx));
                if (key == "fill-opacity") pp.fill_opacity = val;
                else                       pp.line_opacity = val;
            } else if (key == "fill-extrusion-opacity") {
                pp.fill_extrusion_opacity = static_cast<float>(parse_number(ctx));
            } else if (key == "line-width") {
                pp.line_width = static_cast<float>(parse_number(ctx));
            } else {
                skip_value(ctx);
            }
        }
        return pp;
    }

    // ─── Low-level JSON helpers ────────────────────────────────────

    static void skip_ws(Ctx& ctx) {
        while (ctx.pos < ctx.src.size() &&
               std::isspace(static_cast<unsigned char>(ctx.src[ctx.pos])))
            ctx.pos++;
    }

    static char peek(const Ctx& ctx) {
        size_t p = ctx.pos;
        while (p < ctx.src.size() &&
               std::isspace(static_cast<unsigned char>(ctx.src[p])))
            p++;
        return p < ctx.src.size() ? ctx.src[p] : '\0';
    }

    static bool expect(Ctx& ctx, char c) {
        skip_ws(ctx);
        if (ctx.pos >= ctx.src.size() || ctx.src[ctx.pos] != c) {
            std::fprintf(stderr, "StyleEngine JSON error: expected '%c' at %zu\n",
                         c, ctx.pos);
            ctx.ok = false;
            return false;
        }
        ctx.pos++;
        return true;
    }

    static std::string parse_string(Ctx& ctx) {
        skip_ws(ctx);
        if (ctx.pos >= ctx.src.size() || ctx.src[ctx.pos] != '"') {
            ctx.ok = false;
            return "";
        }
        ctx.pos++; // opening quote
        std::string s;
        while (ctx.pos < ctx.src.size()) {
            char c = ctx.src[ctx.pos];
            if (c == '"') { ctx.pos++; return s; }
            if (c == '\\') {
                ctx.pos++;
                if (ctx.pos < ctx.src.size()) {
                    switch (ctx.src[ctx.pos]) {
                        case '"':  s += '"';  break;
                        case '\\': s += '\\'; break;
                        case '/':  s += '/';  break;
                        case 'n':  s += '\n'; break;
                        case 't':  s += '\t'; break;
                        case 'r':  s += '\r'; break;
                        default:   s += ctx.src[ctx.pos]; break;
                    }
                    ctx.pos++;
                }
            } else {
                s += c;
                ctx.pos++;
            }
        }
        return s;
    }

    static double parse_number(Ctx& ctx) {
        skip_ws(ctx);
        char* end = nullptr;
        double val = std::strtod(ctx.src.c_str() + ctx.pos, &end);
        ctx.pos = static_cast<size_t>(end - ctx.src.c_str());
        return val;
    }

    static void skip_value(Ctx& ctx) {
        skip_ws(ctx);
        if (ctx.pos >= ctx.src.size()) return;
        char c = ctx.src[ctx.pos];

        if (c == '"') {
            parse_string(ctx);
        } else if (c == '{' || c == '[') {
            char open  = c;
            char close = (c == '{') ? '}' : ']';
            int depth  = 1;
            ctx.pos++;
            while (ctx.pos < ctx.src.size() && depth > 0) {
                char ch = ctx.src[ctx.pos];
                if (ch == open)
                    depth++;
                else if (ch == close)
                    depth--;
                else if (ch == '"') {
                    parse_string(ctx);
                    continue; // parse_string already advanced pos
                }
                if (depth == 0) return;
                ctx.pos++;
            }
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            parse_number(ctx);
        } else if (ctx.src.compare(ctx.pos, 4, "true") == 0) {
            ctx.pos += 4;
        } else if (ctx.src.compare(ctx.pos, 5, "false") == 0) {
            ctx.pos += 5;
        } else if (ctx.src.compare(ctx.pos, 4, "null") == 0) {
            ctx.pos += 4;
        } else {
            ctx.pos++;
        }
    }
};

} // namespace style
