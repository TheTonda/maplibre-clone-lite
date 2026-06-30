# Map Renderer v2 — API Reference

## mvt_parser.h

Header-only MVT (Mapbox Vector Tile) parser. Parses `.mvt` PBF files using protobuf-lite's `CodedInputStream`.

### Data Structures

#### `mvt::GeomType`
```cpp
enum class GeomType { UNKNOWN = 0, POINT = 1, LINESTRING = 2, POLYGON = 3 };
```

#### `mvt::Value`
```cpp
struct Value {
    enum class Type { NONE, STRING, FLOAT, DOUBLE, INT, UINT, SINT, BOOL };
    Type type = Type::NONE;
    std::string string_value;
    float float_value = 0.0f;
    double double_value = 0.0;
    int32_t int_value = 0;
    uint32_t uint_value = 0;
    int32_t sint_value = 0;
    bool bool_value = false;
};
```

#### `mvt::Feature`
```cpp
struct Feature {
    uint64_t id = 0;
    std::map<uint32_t, std::string> tags;    // key_id -> value (from layer keys)
    mvt::GeomType geom_type = mvt::GeomType::UNKNOWN;
    std::vector<uint32_t> geometry;           // packed geometry command stream
};
```

#### `mvt::Layer`
```cpp
struct Layer {
    std::string name;
    uint32_t version = 0;
    uint32_t extent = 4096;
    std::vector<std::string> keys;            // tag key names
    std::vector<mvt::Value> values;           // tag value lookup table
    std::vector<mvt::Feature> features;
};
```

#### `mvt::Tile`
```cpp
using Tile = std::vector<mvt::Layer>;
```

### Functions

#### `mvt::zigzag_decode(int32_t n) -> int32_t`
Decodes MVT zigzag-encoded integers back to signed values.

#### `mvt::parse_tile(const uint8_t* data, size_t size) -> mvt::Tile`
Main entry point. Reads raw PBF bytes and returns a vector of layers.

**Usage:**
```cpp
// Read file into buffer
std::ifstream file("data.z14.mvt", std::ios::binary | std::ios::ate);
std::vector<uint8_t> data(file.tellg());
file.seekg(0);
file.read(reinterpret_cast<char*>(data.data()), data.size());

// Parse
auto layers = mvt::parse_tile(data.data(), data.size());
```

#### `mvt::print_summary(const mvt::Tile& tile)`
Prints a formatted summary showing each layer name, feature counts by geometry type, and a sample geometry command from the first feature.

---

## render_data.h

Header-only module that converts MVT-parsed geometry into Vulkan-ready vertex/index buffers.

### Data Structures

#### `render_data::LineVertex`
```cpp
struct LineVertex { float x, y; };
```

#### `render_data::PolyVertex`
```cpp
struct PolyVertex { float x, y; };
```

#### `render_data::LineBatch`
```cpp
struct LineBatch {
    std::string layer_name;
    std::vector<LineVertex> vertices;
    std::vector<uint32_t> indices;
};
```

#### `render_data::PolyBatch`
```cpp
struct PolyBatch {
    std::string layer_name;
    std::vector<PolyVertex> vertices;
    std::vector<uint32_t> indices;
};
```

### Functions

#### `render_data::decode_linestring_geometry(const std::vector<uint32_t>& commands, mvt::GeomType type, int extent) -> std::vector<render_data::LineSegment>`
Decodes an MVT geometry command stream into a list of line segments. Handles MoveTo (1), LineTo (2), and ClosePath (7) commands. Uses zigzag-decoded deltas.

#### `render_data::extract_lines(const mvt::Tile& tile, const std::string& layer_name = "", int extent = 4096) -> std::vector<render_data::LineBatch>`
Iterates all tile layers, extracts LINESTRING features, decodes them, and scales tile coords (0..extent) to clip space (-1..1). Returns indexed draw lists with `0xFFFFFFFF` primitive restart markers.

#### `render_data::fan_triangulate(const std::vector<render_data::LineVertex>& ring) -> std::vector<uint32_t>`
Fan-triangulates a single ring into triangle-list indices for polygon rendering.

#### `render_data::decode_polygon_rings(const std::vector<uint32_t>& commands, mvt::GeomType type, int extent) -> std::vector<std::vector<render_data::LineVertex>>`
Reuses the line decoder to decode polygon rings (outer and holes) from geometry commands.

#### `render_data::extract_polygons(const mvt::Tile& tile, const std::string& layer_name = "", int extent = 4096) -> std::vector<render_data::PolyBatch>`
Iterates all tile layers, extracts POLYGON features, takes only the first (outer) ring per feature, fan-triangulates into clip space vertices and indices.

---

## style_engine.h

Header-only simplified MapLibre-style JSON parser. No external JSON library dependency.

### Data Structures

#### `style_engine::StyleRule`
```cpp
struct StyleRule {
    glm::vec3 fill_color{0.0f};
    float fill_opacity = 1.0f;
    glm::vec3 line_color{0.0f};
    float line_width = 1.0f;
    float line_opacity = 1.0f;
};
```

#### `style_engine::PaintProperties`
```cpp
struct PaintProperties {
    std::optional<glm::vec3> fill_color;
    std::optional<float> fill_opacity;
    std::optional<glm::vec3> line_color;
    std::optional<float> line_width;
    std::optional<float> line_opacity;
    std::optional<glm::vec3> background_color;
};
```

#### `style_engine::StyleLayer`
```cpp
struct StyleLayer {
    std::string id;
    std::string type;           // "fill", "line", "background", "symbol"
    PaintProperties paint;
};
```

### Functions

#### `style_engine::parse_hex_color(const std::string& hex) -> glm::vec3`
Parses `#rrggbb` strings to normalized float RGB.

### Class: `style_engine::StyleEngine`

#### `StyleEngine::loadFromJson(const std::string& path) -> bool`
Reads a JSON file, parses it, and loads style layers. Returns true on success.

#### `StyleEngine::matchRule(const std::string& layer_name, const std::string& geom_type_string) -> StyleRule`
Finds a matching style layer by id (or wildcard `*`), returns StyleRule with material parameters.

#### `StyleEngine::matchRule(const std::string& layer_name, mvt::GeomType mvt_geom_type) -> StyleRule`
Maps MVT int geom type to string (`"line"` for LINESTRING, `"fill"` for POLYGON, `"symbol"` for POINT) and delegates to the string-based overload.

#### `StyleEngine::print() const`
Debug output of all loaded layers and their paint properties.

**Usage:**
```cpp
style_engine::StyleEngine engine;
engine.loadFromJson("data/style.json");

auto rule = engine.matchRule("roads", mvt::GeomType::LINESTRING);
// rule.fill_color -> {1.0, 0.8, 0.0}, rule.line_width -> 2.5, etc.
```
