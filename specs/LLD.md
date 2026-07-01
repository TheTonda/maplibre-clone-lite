# Low-Level Design (LLD) Specification
## Interactive 3D Map Renderer

**Version:** 1.0  
**Date:** July 2, 2026  
**Status:** Specification - Ready for Review

---

## 1. Module Breakdown

### 1.1 Module Hierarchy

```
src/
├── main.cpp                    # Application entry point
├── core/
│   ├── window.h/.cpp          # SDL window management
│   ├── vulkan_context.h/.cpp  # Vulkan initialization
│   ├── renderer.h/.cpp        # Main renderer class
│   └── camera.h/.cpp          # Camera system
├── data/
│   ├── osm_loader.h/.cpp      # OSM JSON parser
│   ├── geometry_builder.h/.cpp # 3D geometry generation
│   └── style_engine.h/.cpp    # Style parser
├── graphics/
│   ├── pipeline.h/.cpp        # Graphics pipeline
│   ├── shader.h/.cpp          # Shader management
│   ├── buffer.h/.cpp          # GPU buffer management
│   └── descriptor.h/.cpp      # Descriptor sets
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
    void poll_events();
    bool should_close() const;
    void close();
    
    SDL_Window* get_sdl_window() const;
    VkSurfaceKHR get_surface() const;
    VkExtent2D get_extent() const;
    
    bool is_key_pressed(Key key) const;
    bool is_mouse_button_pressed(MouseButton button) const;
    void get_mouse_position(int& x, int& y) const;
    
private:
    SDL_Window* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
```

**Responsibilities:**
- Create SDL window with Vulkan support
- Create Vulkan surface
- Handle window events (resize, close)
- Provide input state

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
    
    uint32_t find_memory_type(uint32_t type_filter, 
                              VkMemoryPropertyFlags properties) const;
    
private:
    void create_instance();
    void create_device();
    void create_swapchain(Window& window);
    void create_image_views();
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
    VkRenderPass render_pass_;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool command_pool_;
};
```

**Responsibilities:**
- Create Vulkan instance with required extensions
- Select physical device (prefer discrete GPU)
- Create logical device with graphics queue
- Create swapchain with optimal settings
- Create render pass with color + depth attachments
- Create framebuffers for each swapchain image
- Create command pool

---

## 3. Camera System

### 3.1 Camera Module (`core/camera.h`)

**Purpose:** Manage 2D and 3D camera state and matrices

**Interface:**
```cpp
enum class CameraMode { MODE_2D, MODE_3D };

class Camera {
public:
    Camera();
    
    // Position (in meters, relative to data center)
    void set_position(float x, float y);
    void pan(float dx, float dy);
    
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
    float get_y() const;
    float get_zoom() const;
    float get_distance() const;
    float get_tilt() const;
    float get_rotation() const;
    
    // Update from input
    void update_from_input(const InputState& input, float dt);
    
private:
    CameraMode mode_ = CameraMode::MODE_2D;
    
    // Position (in normalized meters)
    float x_ = 0.0f;
    float y_ = 0.0f;
    
    // 2D mode
    float zoom_ = 1.0f;  // 1.0 = show 100m area
    
    // 3D mode
    float distance_ = 500.0f;  // meters from look-at point
    float tilt_ = 45.0f;        // degrees
    float rotation_ = 0.0f;     // degrees
    
    // Constraints
    static constexpr float MIN_ZOOM_2D = 0.1f;
    static constexpr float MAX_ZOOM_2D = 20.0f;
    static constexpr float MIN_DISTANCE_3D = 50.0f;
    static constexpr float MAX_DISTANCE_3D = 5000.0f;
    static constexpr float MIN_TILT = 0.0f;
    static constexpr float MAX_TILT = 85.0f;
};
```

**Coordinate System:**
- All positions in meters relative to data center (0, 0)
- X: East-West (positive = East)
- Y: Height (positive = up) - only used in 3D
- Z: North-South (positive = North) - stored as y_ in 2D for compatibility

**Matrix Calculations:**

*2D Orthographic:*
```cpp
float visible_half_w = 100.0f / zoom_;  // 100m at zoom 1.0
float visible_half_h = visible_half_w / aspect;
proj = glm::ortho(x_ - visible_half_w, x_ + visible_half_w,
                  y_ - visible_half_h, y_ + visible_half_h,
                  -1.0f, 1.0f);
view = glm::mat4(1.0f);  // Identity
```

*3D Perspective:*
```cpp
proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 5000.0f);

float tilt_rad = glm::radians(tilt_);
float rot_rad = glm::radians(rotation_);

float cam_height = distance_ * std::cos(tilt_rad);
float horizontal_dist = distance_ * std::sin(tilt_rad);
float cam_x_offset = horizontal_dist * std::sin(rot_rad);
float cam_z_offset = horizontal_dist * std::cos(rot_rad);

glm::vec3 cam_pos(x_ + cam_x_offset, cam_height, y_ + cam_z_offset);
glm::vec3 look_at(x_, 0.0f, y_);
view = glm::lookAt(cam_pos, look_at, glm::vec3(0.0f, 1.0f, 0.0f));
```

---

## 4. Data Pipeline

### 4.1 OSM Loader (`data/osm_loader.h`)

**Purpose:** Load and parse OSM JSON data

**Data Structures:**
```cpp
namespace osm {
    struct MercatorPoint {
        float x, y;  // Mercator coordinates (0-65536)
    };
    
    struct Building {
        int64_t id;
        std::vector<MercatorPoint> footprint;
        float height;
        std::string type;
    };
    
    struct Road {
        int64_t id;
        std::vector<MercatorPoint> line;
        std::string type;
    };
    
    struct PolygonFeature {
        std::vector<MercatorPoint> polygon;
    };
    
    struct OSMData {
        std::vector<Building> buildings;
        std::vector<Road> roads;
        std::vector<PolygonFeature> parks;
        std::vector<PolygonFeature> water;
        std::vector<PolygonFeature> landuse;
        
        // Bounds (in Mercator coordinates)
        float min_x, min_y, max_x, max_y;
        
        // Center (in Mercator coordinates)
        float center_x, center_y;
    };
}
```

**Interface:**
```cpp
class OSMLoader {
public:
    static OSMData load_from_file(const std::string& path);
    static OSMData load_from_json(const std::string& json_content);
    
private:
    static void parse_buildings(JSONValue& json, OSMData& data);
    static void parse_roads(JSONValue& json, OSMData& data);
    static void parse_features(JSONValue& json, OSMData& data, 
                               const std::string& key,
                               std::vector<PolygonFeature>& output);
    static void compute_bounds(OSMData& data);
};
```

### 4.2 Geometry Builder (`data/geometry_builder.h`)

**Purpose:** Convert 2D OSM data to 3D GPU-ready geometry

**Building Vertex:**
```cpp
struct BuildingVertex {
    float x, y, z;      // Position (meters, normalized)
    float nx, ny, nz;   // Normal (unit vector)
};
```

**Interface:**
```cpp
class GeometryBuilder {
public:
    // Build 3D building geometry from 2D footprints
    static BuildingMesh build_buildings(
        const std::vector<osm::Building>& buildings,
        const glm::vec2& center  // Mercator center for normalization
    );
    
    // Build 2D polygon fills (parks, water, landuse)
    static PolygonMesh build_polygons(
        const std::vector<osm::PolygonFeature>& features,
        const glm::vec2& center
    );
    
    // Build 2D line geometry (roads)
    static LineMesh build_lines(
        const std::vector<osm::Road>& roads,
        const glm::vec2& center
    );
    
    // Build ground plane
    static GroundMesh build_ground(float size);
};
```

**Building Extrusion Algorithm:**
```
For each building footprint:
    1. Normalize coordinates (subtract center)
    2. Create top face vertices (normal = 0, 1, 0)
    3. Create bottom face vertices (normal = 0, -1, 0)
    4. For each edge:
        a. Calculate outward normal
        b. Create 4 vertices for side face
        c. Generate 2 triangles
    5. Triangulate top and bottom with fan
```

**Coordinate Normalization:**
```cpp
// Input: Mercator coordinates (e.g., 46800-46900)
// Output: Normalized meters (e.g., -50 to +50)

glm::vec2 normalize(mercator_x, mercator_y, center) {
    return glm::vec2(mercator_x - center.x, mercator_y - center.y);
}
```

---

## 5. Graphics Pipeline

### 5.1 Pipeline Manager (`graphics/pipeline.h`)

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
    VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
    
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

### 5.2 Shader Management (`graphics/shader.h`)

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

## 6. Rendering

### 6.1 Main Renderer (`core/renderer.h`)

**Purpose:** Orchestrate the rendering pipeline

**Interface:**
```cpp
class Renderer {
public:
    bool initialize(VulkanContext& vk_ctx, Window& window);
    void cleanup();
    
    void begin_frame();
    void end_frame();
    
    void set_camera(const Camera& camera);
    void set_data(const OSMData& data, const GeometryData& geometry);
    
    // Render scene
    void render_2d();
    void render_3d();
    
private:
    void render_ground_2d();
    void render_features_2d();
    void render_lines_2d();
    
    void render_ground_3d();
    void render_features_3d();
    void render_buildings_3d();
    
    void record_command_buffer(uint32_t image_index);
    
    VulkanContext* vk_ctx_;
    Window* window_;
    Camera camera_;
    
    // GPU resources
    Buffer uniform_buffer_;
    DescriptorSet camera_descriptor_;
    
    // Pipelines
    VkPipeline ground_pipeline_2d_;
    VkPipeline fill_pipeline_2d_;
    VkPipeline line_pipeline_2d_;
    
    VkPipeline ground_pipeline_3d_;
    VkPipeline building_pipeline_3d_;
    VkPipeline feature_pipeline_3d_;
    
    // Geometry buffers
    Buffer building_vb_, building_ib_;
    Buffer ground_vb_;
    Buffer feature_vb_, feature_ib_;
    Buffer line_vb_, line_ib_;
    
    // Command buffers
    std::vector<VkCommandBuffer> command_buffers_;
    
    // Sync objects
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    VkFence in_flight_;
    uint32_t current_frame_ = 0;
};
```

---

## 7. Shaders

### 7.1 2D Ground Shader

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

### 7.2 3D Building Shader

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

## 8. Data Flow Examples

### 8.1 Loading and Rendering Flow

```
1. Application starts
2. Window created (1024x768)
3. Vulkan context initialized
4. OSM data loaded from JSON
5. Geometry built from OSM data (normalized to center)
6. GPU buffers created
7. Pipelines created
8. Main loop:
   a. Process events
   b. Update camera from input
   c. Update uniform buffer with camera matrices
   d. Record command buffer:
      - Begin render pass
      - If 2D mode:
        - Render ground
        - Render features
        - Render lines
      - If 3D mode:
        - Render ground
        - Render features
        - Render buildings
      - End render pass
   e. Submit command buffer
   f. Present
```

---

## 9. Error Handling

### 9.1 Error Strategy
- All Vulkan operations check return value
- Errors logged with file/line info
- Fatal errors cause graceful shutdown
- Non-fatal errors log warning and continue

### 9.2 Validation
- Enable Vulkan validation layers in debug builds
- Check shader compilation errors
- Verify buffer sizes before upload
- Validate JSON structure

---

## 10. Testing

### 10.1 Unit Test Coverage

| Module | Test Coverage |
|--------|---------------|
| Camera | Matrix calculations, mode switching, constraints |
| OSM Loader | JSON parsing, coordinate conversion, bounds |
| Geometry Builder | Building extrusion, polygon triangulation, line generation |
| Style Engine | JSON parsing, rule matching |

### 10.2 Integration Tests
- Full pipeline: load → build → render
- Coordinate consistency: 2D and 3D use same space
- Camera bounds: can't zoom out infinitely

---

## 11. Future Enhancements

### 11.1 Performance
- Implement frustum culling
- Add level-of-detail (LOD)
- Use instanced rendering for similar buildings
- Implement tile-based rendering

### 11.2 Features
- Building shadows
- Terrain elevation
- Animated water
- Day/night cycle
- Custom themes

### 11.3 Platform Support
- Windows support
- macOS support (MoltenVK)
- Mobile platforms (future)

---

## 12. Open Questions

1. **Q: Should we use a math library or write our own?**
   A: Use GLM (already decided)

2. **Q: How do we handle very large datasets (>100K buildings)?**
   A: Implement streaming/chunking in future phase

3. **Q: Should we support custom shaders?**
   A: Not in v1.0, but design for it

4. **Q: What's the minimum supported Vulkan version?**
   A: Vulkan 1.2 (for better compatibility)

5. **Q: Should we implement our own JSON parser or use a library?**
   A: Custom parser (no external JSON dependency)
