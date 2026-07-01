# Low-Level Design (LLD) Specification
## Interactive 3D Map Renderer

**Version:** 1.2  
**Date:** July 2, 2026  
**Status:** Specification - Optimized after second review

---

## 1. Module Breakdown

### 1.1 Module Hierarchy

```
src/
├── main.cpp                    # Application entry point
├── core/
│   ├── window.h/.cpp          # SDL window management
│   ├── input_state.h/.cpp     # Unified input state
│   ├── vulkan_context.h/.cpp  # Vulkan initialization
│   ├── renderer.h/.cpp        # Main renderer class
│   └── camera.h/.cpp          # Camera system
├── data/
│   ├── osm_loader.h/.cpp      # OSM protobuf loader
│   ├── geometry_builder.h/.cpp # 3D geometry generation
│   └── style_engine.h/.cpp    # Style parser
├── graphics/
│   ├── pipeline.h/.cpp        # Graphics pipeline
│   ├── shader.h/.cpp          # Shader management
│   ├── buffer.h/.cpp          # GPU buffer management
│   └── descriptor.h/.cpp      # Descriptor sets
├── proto/
│   └── osm_data.proto         # Protobuf schema for preprocessed OSM data
└── shaders/
    ├── 2d/
    │   ├── fill.vert/.frag
    │   ├── line.vert/.frag
    │   └── ground.vert/.frag
    └── 3d/
        ├── building.vert/.frag
        ├── ground3d.vert/.frag
        └── feature.vert/.frag
```

---

## 2. Core Modules

### 2.1 Window Module (`core/window.h`)

**Purpose:** Manage SDL window and Vulkan surface

**Interface:**
```cpp
class Window {
public:
    bool initialize(const std::string& title, int width, int height);
    void poll_events(InputState& input);
    bool should_close() const;
    void close();

    SDL_Window* get_sdl_window() const;
    VkSurfaceKHR get_surface() const;
    VkExtent2D get_extent() const;

private:
    SDL_Window* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
```

**Responsibilities:**
- Create SDL window with Vulkan support
- Create Vulkan surface via `SDL_Vulkan_CreateSurface` (Window owns the surface)
- Handle window events (resize, close)
- Populate an `InputState` object each frame via `poll_events(InputState&)`

### 2.2 Vulkan Context (`core/vulkan_context.h`)

**Purpose:** Initialize Vulkan instance, device, and swapchain

**Interface:**
```cpp
class VulkanContext {
public:
    bool initialize(Window& window);
    void cleanup();
    
    VkInstance get_instance() const;
    VkPhysicalDevice get_physical_device() const;
    VkDevice get_device() const;
    VkQueue get_graphics_queue() const;
    VkSwapchainKHR get_swapchain() const;
    
    uint32_t get_image_count() const;
    VkImage get_swapchain_image(uint32_t index) const;
    VkImageView get_swapchain_image_view(uint32_t index) const;
    VkFormat get_swapchain_format() const;
    VkExtent2D get_swapchain_extent() const;
    VkRenderPass get_render_pass() const;
    VkFramebuffer get_framebuffer(uint32_t index) const;
    
    uint32_t find_memory_type(uint32_t type_filter, 
                              VkMemoryPropertyFlags properties) const;
    
private:
    void create_instance();
    void create_device();
    void create_swapchain(Window& window);
    void create_image_views();
    void create_depth_resources();
    void create_render_pass();
    void create_framebuffers();
    void create_command_pool();
    
    VkInstance instance_;
    VkPhysicalDevice physical_device_;
    VkDevice device_;
    VkQueue graphics_queue_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    VkFormat swapchain_format_;
    VkExtent2D swapchain_extent_;
    VkRenderPass render_pass_;
    VkImage depth_image_;
    VkDeviceMemory depth_image_memory_;
    VkImageView depth_image_view_;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool command_pool_;
};
```

**Responsibilities:**
- Create Vulkan instance with required extensions
- Select physical device (prefer discrete GPU)
- Create logical device with graphics queue
- Create swapchain with optimal settings
- Create depth image and view
- Create render pass with color + depth attachments
- Create framebuffers for each swapchain image
- Create command pool

---

## 3. Input System

### 3.1 Input State (`core/input_state.h`)

**Purpose:** Collect all per-frame input in one place so the camera and UI can read it cleanly.

**Interface:**
```cpp
struct InputState {
    // Keyboard
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool zoom_in = false;   // '+' key
    bool zoom_out = false;  // '-' key
    bool tilt_up = false;   // E
    bool tilt_down = false; // Q
    bool rotate_left = false;  // A
    bool rotate_right = false; // D
    bool switch_2d = false; // F1
    bool switch_3d = false; // F2

    // Mouse
    bool left_mouse_down = false;
    bool left_mouse_pressed = false;  // edge this frame
    bool left_mouse_released = false; // edge this frame
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    float mouse_delta_x = 0.0f;
    float mouse_delta_y = 0.0f;
    float scroll_delta = 0.0f;

    // Time
    float dt = 0.0f;
};
```

---

## 4. Camera System

### 4.1 Camera Module (`core/camera.h`)

**Purpose:** Manage 2D and 3D camera state and matrices

**Interface:**
```cpp
enum class CameraMode { MODE_2D, MODE_3D };

class Camera {
public:
    Camera();
    
    // Position (local ENU meters, relative to data center)
    void set_position(float x, float z);
    void pan(float dx, float dz);

    // Zoom (2D mode: orthographic scale, 3D mode: distance in meters)
    void set_zoom(float zoom);
    void zoom_by(float delta);

    // 3D mode parameters
    void set_distance(float distance);
    void set_tilt(float degrees);  // 0 = top-down, 90 = horizontal
    void set_rotation(float degrees);  // Azimuth

    void set_mode(CameraMode mode);
    CameraMode get_mode() const;

    // Get matrices
    glm::mat4 get_projection_matrix(float aspect) const;
    glm::mat4 get_view_matrix() const;

    // Get current state
    float get_x() const;
    float get_z() const;  // north-south axis
    float get_zoom() const;
    float get_distance() const;
    float get_tilt() const;
    float get_rotation() const;

    // Update from input
    void update_from_input(const InputState& input);

    // Reset to frame the given bounds
    void frame_bounds(float min_x, float max_x, float min_z, float max_z);

private:
    CameraMode mode_ = CameraMode::MODE_2D;

    // Look-at point in local ENU meters relative to data center
    float x_ = 0.0f;  // east
    float z_ = 0.0f;  // north

    // Data extent (set by frame_bounds, used for projection)
    float data_extent_x_ = 100.0f;
    float data_extent_z_ = 100.0f;

    // 2D mode
    float zoom_ = 1.0f;  // 1.0 = show full data extent

    // 3D mode
    float distance_ = 500.0f;  // meters from look-at point
    float tilt_ = 45.0f;        // degrees, 0 = top-down
    float rotation_ = 0.0f;     // degrees, azimuth

    // Constraints
    static constexpr float MIN_ZOOM_2D = 0.1f;
    static constexpr float MAX_ZOOM_2D = 20.0f;
    static constexpr float MIN_DISTANCE_3D = 50.0f;
    static constexpr float MAX_DISTANCE_3D = 5000.0f;
    static constexpr float MIN_TILT = 0.0f;
    static constexpr float MAX_TILT = 85.0f;

    void clamp_values();
};
```

**Coordinate System:**
- All positions are in **local ENU meters** centered on the dataset center.
- `x_`: east-west (positive = east)
- `z_`: north-south (positive = north)
- `y`: up (positive = up), generated by geometry builder

**Note on z-axis naming:** The 2D camera stores the north-south axis in `z_` to match the 3D world where the ground plane is the XZ plane.

**Why this matters:**
Because x/z are real meters, a building height of 20 m always looks 20 m tall relative to its footprint, regardless of whether the data is in Delhi, Oslo, or the equator. The camera’s zoom, distance, pan speed, and near/far planes are all expressed in intuitive meter units.

**Matrix Calculations:**

*2D Orthographic:*
```cpp
// zoom_ == 1.0 means the full dataset fits in the shorter viewport dimension.
float base_half_w = data_extent_x / 2.0f;
float base_half_h = data_extent_z / 2.0f;
float visible_half_w = base_half_w / zoom_;
float visible_half_h = base_half_h / zoom_;

// Preserve aspect ratio by fitting to the smaller axis
float scale = std::min(visible_half_w, visible_half_h * aspect);
proj = glm::ortho(x_ - scale, x_ + scale,
                  z_ - scale / aspect, z_ + scale / aspect,
                  -1.0f, 1.0f);
view = glm::mat4(1.0f);  // Identity
```

*Camera update from input:*
```cpp
void Camera::update_from_input(const InputState& input) {
    float pan_speed = base_pan_speed / zoom_;  // meters per second at current zoom
    if (mode_ == MODE_3D) {
        pan_speed = base_pan_speed * (distance_ / 500.0f);
    }
    if (input.up)    z_ -= pan_speed * input.dt;
    if (input.down)  z_ += pan_speed * input.dt;
    if (input.left)  x_ -= pan_speed * input.dt;
    if (input.right) x_ += pan_speed * input.dt;

    if (mode_ == MODE_2D) {
        zoom_by(input.scroll_delta * zoom_sensitivity);
    } else {
        set_distance(distance_ - input.scroll_delta * distance_sensitivity);
        set_tilt(tilt_ + (input.tilt_up ? 1.0f : 0.0f) * tilt_speed * input.dt
                      - (input.tilt_down ? 1.0f : 0.0f) * tilt_speed * input.dt);
        set_rotation(rotation_ + (input.rotate_right ? 1.0f : 0.0f) * rotate_speed * input.dt
                                - (input.rotate_left ? 1.0f : 0.0f) * rotate_speed * input.dt);
    }

    // Mouse drag pan: convert screen delta to world meters using current projection
    if (input.left_mouse_down) {
        x_ -= input.mouse_delta_x * pan_speed * input.dt;
        z_ -= input.mouse_delta_y * pan_speed * input.dt;
    }

    clamp_values();
}
```

*Mode switching helper:*
```cpp
void Camera::set_mode(CameraMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    if (mode_ == MODE_2D) {
        // Pick a 2D zoom that shows the same ground area as the current 3D view
        zoom_ = std::clamp(500.0f / distance_, MIN_ZOOM_2D, MAX_ZOOM_2D);
    } else {
        // Pick a 3D distance that matches the current 2D zoom
        distance_ = std::clamp(500.0f / zoom_, MIN_DISTANCE_3D, MAX_DISTANCE_3D);
        tilt_ = 45.0f;
    }
    clamp_values();
}
```

*3D Perspective:*
```cpp
proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10000.0f);

float tilt_rad = glm::radians(tilt_);
float rot_rad = glm::radians(rotation_);

float cam_height = distance_ * std::cos(tilt_rad);
float horizontal_dist = distance_ * std::sin(tilt_rad);
float cam_x_offset = horizontal_dist * std::sin(rot_rad);
float cam_z_offset = horizontal_dist * std::cos(rot_rad);

glm::vec3 cam_pos(x_ + cam_x_offset, cam_height, z_ + cam_z_offset);
glm::vec3 look_at(x_, 0.0f, z_);
view = glm::lookAt(cam_pos, look_at, glm::vec3(0.0f, 1.0f, 0.0f));
```

**Why no gimbal lock:**
Tilt is clamped to `[0°, 85°]`, so the up vector `(0, 1, 0)` never aligns with the view direction.

---

## 5. Data Pipeline

### 5.1 Protobuf Schema (`proto/osm_data.proto`)

The Python preprocessor emits a compact binary protobuf file. The C++ loader deserializes it directly into the internal `OSMData` structure.

```protobuf
syntax = "proto3";
package map_renderer;

message Point2D {
    double x = 1;  // local ENU east coordinate in meters
    double z = 2;  // local ENU north coordinate in meters
}

message Building {
    int64 id = 1;
    repeated Point2D footprint = 2;
    float height_m = 3;
    string height_source = 4;  // "tag", "levels", "default"
    string type = 5;
}

message Road {
    int64 id = 1;
    repeated Point2D line = 2;
    string type = 3;
    float width_m = 4;
}

message PolygonFeature {
    repeated Point2D polygon = 1;
    string type = 2;
}

message OSMDataProto {
    uint32 schema_version = 1;
    repeated Building buildings = 2;
    repeated Road roads = 3;
    repeated PolygonFeature parks = 4;
    repeated PolygonFeature water = 5;
    repeated PolygonFeature landuse = 6;

    double center_x = 7;  // ENU east (meters)
    double center_z = 8;  // ENU north (meters)
    double min_x = 9;
    double max_x = 10;
    double min_z = 11;
    double max_z = 12;
}
```

### 5.2 OSM Loader (`data/osm_loader.h`)

**Purpose:** Load and parse OSM protobuf data

**Data Structures:**
```cpp
namespace osm {
    struct Point {
        float x, z;  // local ENU meters: x=east, z=north
    };

    enum class HeightSource { Tag, Levels, Default };

    struct Building {
        int64_t id;
        std::vector<Point> footprint;
        float height;
        HeightSource height_source;
        std::string type;
    };

    struct Road {
        int64_t id;
        std::vector<Point> line;
        std::string type;
        float width;
    };

    struct PolygonFeature {
        std::vector<Point> polygon;
        std::string type;
    };

    struct OSMData {
        std::vector<Building> buildings;
        std::vector<Road> roads;
        std::vector<PolygonFeature> parks;
        std::vector<PolygonFeature> water;
        std::vector<PolygonFeature> landuse;

        // Bounds (local ENU meters)
        float min_x, min_z, max_x, max_z;

        // Center (local ENU meters, usually 0,0)
        float center_x, center_z;
    };
}
```

**Height Fallback Policy:**
1. If the OSM `height` tag exists and is valid, use it. Set `height_source = Tag`.
2. Else if `building:levels` exists, compute `height = levels × 3.0 m`. Set `height_source = Levels`.
3. Else use a configurable default (e.g., 9.0 m). Set `height_source = Default`.

**Interface:**
```cpp
class OSMLoader {
public:
    static osm::OSMData load_from_file(const std::string& path);
    static osm::OSMData load_from_proto(const std::vector<uint8_t>& bytes);

private:
    // Validates that protobuf bounds are sane; recomputes only if invalid
    static void validate_bounds(osm::OSMData& data);
};
```

### 5.3 Geometry Builder (`data/geometry_builder.h`)

**Purpose:** Convert 2D OSM data to GPU-ready geometry in local ENU meters

**Building Vertex:**
```cpp
struct BuildingVertex {
    float x, y, z;      // Position (local ENU meters)
    float nx, ny, nz;   // Normal (unit vector)
};
```

**Interface:**
```cpp
class GeometryBuilder {
public:
    // Build 3D building geometry from 2D footprints (already in ENU meters)
    static BuildingMesh build_buildings(
        const std::vector<osm::Building>& buildings
    );

    // Build 2D polygon fills (parks, water, landuse)
    static PolygonMesh build_polygons(
        const std::vector<osm::PolygonFeature>& features
    );

    // Build 2D road quads from line strings
    static LineMesh build_road_quads(
        const std::vector<osm::Road>& roads,
        float default_width_meters = 6.0f
    );

    // Build ground plane
    static GroundMesh build_ground(float half_size_meters);
};
```

**Mesh Types:**
```cpp
struct BuildingMesh {
    std::vector<BuildingVertex> vertices;
    std::vector<uint32_t> indices;
};

struct PolygonMesh {
    std::vector<float> vertices;  // x, z pairs
    std::vector<uint32_t> indices;
};

struct LineMesh {
    std::vector<float> vertices;  // x, y, z triplets (road quads)
    std::vector<uint32_t> indices;
};

struct GroundMesh {
    std::vector<float> vertices;  // x, z pairs
};

struct GeometryData {
    BuildingMesh buildings;
    PolygonMesh polygons;       // parks + water + landuse combined
    LineMesh roads;
    GroundMesh ground;
};
```

**Building Extrusion Algorithm:**
```
For each building footprint (assume outer ring is counter-clockwise when viewed from above):
    1. Bottom ring at y = 0
    2. Top ring at y = building.height
    3. For each edge, create two triangles forming the side wall
       (outward normal = normalized(cross(edge_dir, vec3(0,1,0))))
    4. Triangulate the top cap using ear-clipping
    5. Skip the bottom cap (hidden by the ground plane)
```

**Front face convention:**
- Front faces are counter-clockwise when viewed from outside.
- PipelineConfig uses `VK_FRONT_FACE_COUNTER_CLOCKWISE`.

**Polygon Triangulation:**
- Implement ear-clipping for general simple polygons.
- Document that polygons with holes are not supported in v1.0; outer ring only.
- Validate winding order and emit counter-clockwise triangles for backface culling.

**Road Quad Generation:**
- For each segment, compute the perpendicular direction in the xz plane.
- Extrude left/right by half the road width.
- Emit two triangles per segment, with shared vertices at joins to avoid gaps.
- Fallback to `default_width_meters` when OSM width is missing.

---

## 6. Style Engine

### 6.1 Style Engine (`data/style_engine.h`)

**Purpose:** Map feature types (and optionally zoom) to colors and line widths.

**Interface:**
```cpp
struct StyleRule {
    std::string feature_type;  // e.g., "building", "road_primary", "water"
    glm::vec4 fill_color = glm::vec4(1.0f);
    glm::vec4 line_color = glm::vec4(1.0f);
    float line_width_meters = 1.0f;
    float fill_extrusion_height = 0.0f;  // 0 = not extruded
};

class StyleEngine {
public:
    bool load_from_file(const std::string& path);
    bool load_from_json(const std::string& json_content);

    StyleRule get_rule(const std::string& feature_type) const;

    // Built-in fallback rules for v1.0
    static StyleEngine default_style();

private:
    std::unordered_map<std::string, StyleRule> rules_;
};

// Minimal nlohmann/json dependency is approved; style file is human-readable.
```

**Default style (v1.0):**
| Feature | Fill color | Line color | Notes |
|---------|------------|------------|-------|
| building | #D9C3A5 | - | extruded |
| road_primary | - | #FEFEFE | 8 m |
| road_secondary | - | #F4F4F4 | 6 m |
| water | #4D8DC9 | - | - |
| park | #8FBA7F | - | - |
| landuse | #E8E0D8 | - | - |
| ground | #1E1E20 | - | grid |

---

## 7. Graphics Pipeline

### 7.0 GPU Buffer and Descriptor Wrappers (`graphics/buffer.h`, `graphics/descriptor.h`)

**Buffer (`graphics/buffer.h`):**
```cpp
class Buffer {
public:
    bool create(VkDevice device, VkPhysicalDevice phys_dev,
                VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties);
    void destroy();

    void upload(VkDevice device, const void* data, VkDeviceSize size) const;

    VkBuffer get_handle() const;
    VkDeviceMemory get_memory() const;
    VkDeviceSize get_size() const;

private:
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
};
```

**DescriptorSet (`graphics/descriptor.h`):**
```cpp
class DescriptorSet {
public:
    bool create(VkDevice device,
                const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void destroy();

    VkDescriptorSetLayout get_layout() const;
    VkDescriptorSet get_set() const;
    VkDescriptorPool get_pool() const;

    void update_buffer(VkDevice device, uint32_t binding,
                       VkBuffer buffer, VkDeviceSize offset,
                       VkDeviceSize range) const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
};
```

### 7.1 Pipeline Manager (`graphics/pipeline.h`)

**Purpose:** Create and manage Vulkan graphics pipelines

**Interface:**
```cpp
struct PipelineConfig {
    std::string name;
    std::string vertex_shader;
    std::string fragment_shader;

    // Vertex input
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // Rasterization
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Depth
    bool depth_test = true;
    bool depth_write = true;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS;

    // Blending
    bool blend_enable = false;

    // Push constants
    uint32_t push_constant_size = 0;

    // Descriptor sets
    std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings;
};

class PipelineManager {
public:
    bool initialize(VkDevice device, VkRenderPass render_pass);
    void cleanup();
    
    VkPipeline create_pipeline(const PipelineConfig& config);
    VkPipelineLayout get_layout(const std::string& name) const;
    
private:
    VkDevice device_;
    VkRenderPass render_pass_;
    std::unordered_map<std::string, VkPipeline> pipelines_;
    std::unordered_map<std::string, VkPipelineLayout> layouts_;
};
```

### 7.2 Shader Management (`graphics/shader.h`)

**Purpose:** Compile and load GLSL shaders

**Interface:**
```cpp
class ShaderManager {
public:
    bool initialize(VkDevice device);
    void cleanup();
    
    VkShaderModule load_from_file(const std::string& path);
    VkShaderModule load_from_source(const std::string& source);
    
private:
    VkDevice device_;
    std::unordered_map<std::string, VkShaderModule> shaders_;
};
```

---

## 8. Rendering

### 8.1 Main Renderer (`core/renderer.h`)

**Purpose:** Orchestrate the rendering pipeline

**Interface:**
```cpp
class Renderer {
public:
    bool initialize(VulkanContext& vk_ctx, Window& window);
    void cleanup();

    // Acquire image, record scene, submit, present. Returns false if swapchain must be recreated.
    bool render_frame(const Camera& camera);

    void set_data(const osm::OSMData& data, const GeometryData& geometry);

private:
    void record_command_buffer(uint32_t image_index, const Camera& camera);

    VulkanContext* vk_ctx_;
    Window* window_;

    // GPU resources
    Buffer uniform_buffer_;
    DescriptorSet camera_descriptor_;

    // Pipelines
    VkPipeline ground_pipeline_2d_;
    VkPipeline fill_pipeline_2d_;
    VkPipeline road_pipeline_2d_;

    VkPipeline ground_pipeline_3d_;
    VkPipeline building_pipeline_3d_;
    VkPipeline feature_pipeline_3d_;

    // Geometry buffers
    Buffer building_vb_, building_ib_;
    Buffer ground_vb_;
    Buffer feature_vb_, feature_ib_;
    Buffer road_vb_, road_ib_;

    // Command buffers
    std::vector<VkCommandBuffer> command_buffers_;

    // Per-frame-in-flight sync objects (kMaxFramesInFlight = 2 or 3)
    static constexpr int kMaxFramesInFlight = 2;
    std::array<VkSemaphore, kMaxFramesInFlight> image_available_;
    std::array<VkSemaphore, kMaxFramesInFlight> render_finished_;
    std::array<VkFence, kMaxFramesInFlight> in_flight_fences_;

    // Per-swapchain-image fences to detect when the GPU is done presenting an image
    std::vector<VkFence> image_in_flight_;

    uint32_t current_frame_ = 0;
};
```

---

## 9. Shaders

### 9.1 2D Ground Shader

**Vertex:**
```glsl
#version 450
layout(location = 0) in vec2 inPosition;  // x, z in meters
layout(binding = 0) uniform CameraUBO { mat4 proj; mat4 view; } camera;
layout(location = 0) out vec3 fragWorldPos;

void main() {
    vec3 worldPos = vec3(inPosition.x, 0.0, inPosition.y);
    fragWorldPos = worldPos;
    gl_Position = camera.proj * camera.view * vec4(worldPos, 1.0);
}
```

**Fragment:**
```glsl
#version 450
layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 baseColor = vec3(0.15, 0.15, 0.17);
    
    // Grid lines
    float gridSize = 50.0;
    vec2 grid = abs(fract(fragWorldPos.xz / gridSize - 0.5) - 0.5) 
                / fwidth(fragWorldPos.xz / gridSize);
    float line = min(grid.x, grid.y);
    float gridIntensity = 1.0 - min(line, 1.0);
    
    vec3 gridColor = vec3(0.25, 0.25, 0.28);
    vec3 finalColor = mix(baseColor, gridColor, gridIntensity * 0.5);
    
    outColor = vec4(finalColor, 1.0);
}
```

### 9.2 3D Building Shader

**Vertex:**
```glsl
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(binding = 0) uniform CameraUBO { mat4 proj; mat4 view; } camera;
layout(push_constant) uniform PushConstants { vec4 color; } pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
    fragColor = pc.color;
    fragNormal = inNormal;
}
```

**Fragment:**
```glsl
#version 450
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.3, 0.85, 0.4));
    
    float ambient = 0.35;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float light = ambient + diffuse * 0.65;
    
    float topFactor = 0.85 + 0.15 * max(normal.y, 0.0);
    
    vec3 shaded = fragColor.rgb * light * topFactor;
    outColor = vec4(shaded, fragColor.a);
}
```

---

## 10. Data Flow Examples

### 10.1 Loading and Rendering Flow

```
1. Application starts
2. Window created (1024x768)
3. Vulkan context initialized
4. Python preprocessor converts OSM PBF → osm_data.bin (protobuf)
5. OSM data loaded from protobuf
6. Geometry built from OSM data (already in local ENU meters)
7. GPU buffers created
8. Pipelines created
9. Main loop:
   a. Poll events into InputState
   b. Update camera from InputState
   c. Update uniform buffer with camera matrices
   d. Record command buffer:
      - Begin render pass (with depth clear)
      - If 2D mode:
        - Render ground
        - Render features
        - Render roads
      - If 3D mode:
        - Render ground
        - Render features
        - Render buildings
      - End render pass
   e. Submit command buffer with correct per-frame sync
   f. Present
```

---

## 11. Error Handling

### 11.1 Error Strategy
- All Vulkan operations check return value
- Errors logged with file/line info
- Fatal errors cause graceful shutdown
- Non-fatal errors log warning and continue

### 11.2 Validation
- Enable Vulkan validation layers in debug builds
- Check shader compilation errors
- Verify buffer sizes before upload
- Validate protobuf deserialization
- Validate style JSON structure

---

## 12. Testing

### 12.1 Unit Test Coverage

| Module | Test Coverage |
|--------|---------------|
| Camera | Matrix calculations, mode switching, constraints, bounds framing |
| OSM Loader | Protobuf parsing, height fallback, bounds |
| Geometry Builder | Building extrusion, polygon triangulation, road quad generation |
| Style Engine | JSON parsing, rule matching, fallback colors |

### 12.2 Integration Tests
- Full pipeline: load → build → render
- Coordinate consistency: 2D and 3D use same space
- Camera bounds: can't zoom out infinitely

---

## 13. Future Enhancements

### 13.1 Performance
- Implement frustum culling
- Add level-of-detail (LOD)
- Use instanced rendering for similar buildings
- Implement tile-based rendering

### 13.2 Features
- Building shadows
- Terrain elevation
- Animated water
- Day/night cycle
- Custom themes

### 13.3 Platform Support
- Android support (via SDL Android backend / ANativeWindow + Vulkan)
- Windows support (future)
- macOS support (MoltenVK, future)

---

## 14. Open Questions

1. **Q: Should we use a math library or write our own?**
   A: Use GLM (already decided)

2. **Q: How do we handle very large datasets (>100K buildings)?**
   A: Implement streaming/chunking in future phase

3. **Q: Should we support custom shaders?**
   A: Not in v1.0, but design for it

4. **Q: What's the minimum supported Vulkan version?**
   A: Vulkan 1.2 (for better compatibility)

5. **Q: Should we use a JSON library for styles?**
   A: Yes — nlohmann/json (header-only) is approved because it improves quality and speeds up development.
