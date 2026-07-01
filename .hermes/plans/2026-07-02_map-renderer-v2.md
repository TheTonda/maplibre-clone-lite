# Interactive 3D Map Renderer - Implementation Plan

> **For Hermes:** Execute this plan task-by-task using `opencode run` for each. Verify build + tests after every task before committing and pushing.

**Goal:** Build an interactive 3D map renderer from scratch using Vulkan + C++23 that renders OpenStreetMap protobuf data in both 2D orthographic and 3D perspective modes with extruded buildings.

**Architecture:** Four-layer design — Application (SDL window, input, camera), Renderer (orchestrates passes), Data (OSM protobuf loader, geometry builder, style engine), Graphics (Vulkan wrappers, pipelines, shaders). Single coordinate space: local ENU meters centered on dataset.

**Tech Stack:** C++23, Vulkan 1.2, SDL2, GLM, CMake 3.20+, Google Test, protobuf, nlohmann/json, Python 3.8+ (preprocessing).

**Model strategy:**
- Routine/mechanical: `opencode-go/deepseek-v4-flash`
- Vulkan-heavy/complex: `opencode-go/glm-5.2`
- Graphical/debugging: `opencode-go/kimi-k2.7-code`

---

## Project Config

File: `.opencode/opencode.json` (already created)
Default model: `opencode-go/deepseek-v4-flash`
Permission: edit=allow, bash=allow, question=deny

---

## Task 1: Project Scaffold and Build System

**Model:** deepseek-v4-flash | **Files:** ~8 new files | **Estimated:** 3 min

Create the complete build system and project structure.

Files to create:
1. `CMakeLists.txt` — C++23, find Vulkan/SDL2/GLM, fetch protobuf/nlohmann/GTest via FetchContent, compiler flags -Wall -Wextra -Wpedantic, define MAP_RENDERER_DEBUG in Debug builds, set rpath for Vulkan
2. `src/core/debug_log.h` — DEBUG_LOG macro using std::format, gated by #ifdef MAP_RENDERER_DEBUG, no-op in release
3. `tests/test_main.cpp` — GTest main, include gtest/gtest.h
4. `tests/test_dummy.cpp` — one dummy test that passes to verify framework
5. `.gitignore` — ignore build/, *.o, *.spv, __pycache__, .DS_Store
6. `README.md` — project description, build instructions (dependencies, cmake, make), controls, data pipeline overview

Directory structure to create:
- src/core/, src/data/, src/graphics/, src/proto/, src/shaders/2d/, src/shaders/3d/
- tests/, tools/, data/

**Verification:** `mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./map-renderer-tests` — dummy test passes

---

## Task 2: Vulkan Window Foundation

**Model:** glm-5.2 | **Files:** ~4 new files | **Estimated:** 4 min

Implement the Window class, InputState struct, and Vulkan instance creation up through logical device. This is the hardest Vulkan setup phase.

Files to create:
1. `src/core/window.h` — Window class: initialize(title,w,h), poll_events(InputState&), should_close(), close(), get_sdl_window(), get_surface(), get_extent(). Uses SDL2 with Vulkan flag. Creates surface via SDL_Vulkan_CreateSurface.
2. `src/core/window.cpp` — Full implementation with error handling
3. `src/core/input_state.h` — InputState struct as defined in LLD: keyboard bools (up/down/left/right/zoom_in/out/tilt_up/down/rotate_left/right/switch_2d/3d), mouse fields (left_mouse_down/pressed/released, x/y/delta_x/delta_y, scroll_delta), dt float
4. `src/core/vulkan_context.h` — VulkanContext class declaration: initialize(Window&), cleanup(), getters for instance/device/queue/swapchain/framebuffers/render_pass, find_memory_type(). Private: create_instance, create_device, create_swapchain, create_image_views, create_depth_resources, create_render_pass, create_framebuffers, create_command_pool. Stores VkInstance, VkPhysicalDevice, VkDevice, VkQueue, VkSwapchainKHR, images, views, depth resources, render_pass, framebuffers, command_pool.

Implementation details for vulkan_context.cpp (this task covers instance + device only; swapchain/passes come in T3-T4):
- create_instance(): VkApplicationInfo, get SDL extensions, enable validation layers in debug, create debug messenger
- create_device(): enumerate physical devices, prefer discrete GPU, check queue family (graphics+present), create logical device with swapchain extension

**Verification:** Build succeeds with no warnings. Window opens with clear color (once main loop is added in T5, but the classes compile now).

---

## Task 3: Swapchain, Depth Buffer, Render Pass

**Model:** glm-5.2 | **Files:** 1 modified (vulkan_context.cpp) | **Estimated:** 3 min

Complete the VulkanContext with swapchain creation, depth buffer, render pass, framebuffers, and command pool.

Continue implementing vulkan_context.cpp:
- create_swapchain(): query support, choose SRGB format, MAILBOX present mode, create VkSwapchainKHR, get images, create image views
- create_depth_resources(): choose D32_SFLOAT format, create image+memory+view
- create_render_pass(): color attachment (SRGB, load=clear, store=store), depth attachment (D32, load=clear, store=dont_care), single subpass, dependency for VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
- create_framebuffers(): one per swapchain image, attach color+depth views
- create_command_pool(): VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

**Verification:** Build succeeds. VulkanContext compiles completely with all private methods stubbed and implemented.

---

## Task 4: Sync Objects and Main Render Loop

**Model:** glm-5.2 | **Files:** 1 modified + 1 new | **Estimated:** 3 min

Add per-frame-in-flight sync objects and the main render loop to VulkanContext. Create main.cpp entry point.

Update vulkan_context.h/.cpp:
- Add members: kMaxFramesInFlight=2, image_available_[], render_finished_[], in_flight_fences_[], image_in_flight_[] vectors
- Add sync object creation in initialize() after command pool
- Add method: draw_frame() — acquire next image with timeout, wait for per-frame fence, reset fence, record command buffer (simple clear), submit with wait/signal semaphores, present
- Handle swapchain recreation on resize/minimize

Create:
1. `src/main.cpp` — SDL_Init, create Window, create VulkanContext, main loop: poll_events → draw_frame, check should_close(), cleanup

**Verification:** `cd build && cmake .. && make -j$(nproc) && ./map-renderer` — window opens showing clear color (dark grey), closes on ESC

---

## Task 5: Graphics Wrappers - Buffer, DescriptorSet, Pipeline

**Model:** glm-5.2 | **Files:** 4 new files | **Estimated:** 4 min

Create reusable Vulkan graphics wrappers: Buffer, DescriptorSet, ShaderManager, and PipelineManager.

Files to create:
1. `src/graphics/buffer.h` — Buffer class: create(device, phys_dev, size, usage, properties), destroy(), upload(device, data, size), get_handle(), get_memory(), get_size(). Uses staging buffer for upload.
2. `src/graphics/buffer.cpp` — Full implementation with VkBuffer, VkDeviceMemory, staging buffer pattern
3. `src/graphics/descriptor.h` — DescriptorSet class: create(device, bindings), destroy(), get_layout(), get_set(), get_pool(), update_buffer(). Manages VkDescriptorPool, VkDescriptorSetLayout, VkDescriptorSet.
4. `src/graphics/descriptor.cpp` — Implementation
5. `src/graphics/shader.h` — ShaderManager class: initialize(device), cleanup(), load_from_file(path), load_from_source(source). Reads .spv files.
6. `src/graphics/shader.cpp` — Implementation
7. `src/graphics/pipeline.h` — PipelineConfig struct (name, vertex/fragment shader, bindings, attributes, polygon_mode, cull_mode, front_face, depth_test, depth_write, depth_compare, blend_enable, push_constant_size, descriptor_bindings). PipelineManager class: initialize(device, render_pass), cleanup(), create_pipeline(config), get_layout(name).
8. `src/graphics/pipeline.cpp` — Implementation: create VkDescriptorSetLayout from bindings, create VkPipelineLayout, create VkGraphicsPipeline with all state from config, store in maps

**Verification:** Build compiles all 4 new files. Add unit test for buffer creation in tests/test_buffer.cpp — create buffer, upload data, verify (no actual Vulkan instance needed for struct test).

---

## Task 6: Camera UBO + Shader Infrastructure

**Model:** deepseek-v4-flash | **Files:** 3 new + 2 modified | **Estimated:** 2 min

Create the Camera uniform buffer object, wire it into the render pipeline, and set up CMake shader compilation.

Files to create:
1. `src/core/camera_ubo.h` — CameraUBO struct: mat4 proj, mat4 view (GLM, std140 layout). Helper to create descriptor set layout binding.
2. `src/shaders/compile_shaders.sh` — Script to compile all .vert/.frag to .spv using glslc, output to build/shaders/
3. Update `CMakeLists.txt` — add custom command to run compile_shaders.sh during build, copy .spv to build/shaders/

Create first shader pair:
4. `src/shaders/2d/fill.vert` — takes vec2 inPosition at location 0, CameraUBO at binding 0, transforms to clip space
5. `src/shaders/2d/fill.frag` — outputs solid color (vec4 from push constant)

Update `tests/`:
- test_camera_ubo.cpp — verify sizeof/alignment matches GLSL std140

**Verification:** `cmake .. && make` — shaders compile to .spv, build succeeds. Shader files exist in build/shaders/.

---

## Task 7: Data Pipeline - Protobuf Schema and OSM Types

**Model:** deepseek-v4-flash | **Files:** 3 new files | **Estimated:** 3 min

Define the protobuf schema, internal OSM data types, and Python preprocessing script.

Files to create:
1. `src/proto/osm_data.proto` — As defined in LLD: Point2D (double x, z), Building (int64 id, repeated Point2D footprint, float height_m, string height_source, string type), Road (int64 id, repeated Point2D line, string type, float width_m), PolygonFeature (repeated Point2D polygon, string type), OSMDataProto (uint32 schema_version, repeated Building, Road, PolygonFeature for parks/water/landuse, double center/min/max x/z)
2. `src/data/osm_types.h` — osm namespace: Point (float x,z), HeightSource enum (Tag, Levels, Default), Building (int64_t id, vector<Point> footprint, float height, HeightSource height_source, string type), Road (int64_t id, vector<Point> line, string type, float width), PolygonFeature (vector<Point> polygon, string type), OSMData (vectors of buildings/roads/parks/water/landuse, float bounds min/max/center x/z)
3. `tools/extract_geometry.py` — Reads OSM PBF via osmium, converts WGS84 to local ENU meters, applies height fallback (tag → levels×3.0 → default 9.0), extracts buildings/roads/parks/water/landuse, serializes to protobuf binary. Module structure: main(), enu_convert(lat,lon,center_lat,center_lon), get_height(tags), extract_buildings(), extract_roads(), extract_polygons().

Update `CMakeLists.txt`:
- Add protobuf_generate_cpp() for osm_data.proto
- Link protobuf library

**Verification:** Build compiles with proto-generated code. Python script: `python3 tools/extract_geometry.py` — creates osm_data.bin (requires test OSM file).

---

## Task 8: OSM Loader Implementation

**Model:** deepseek-v4-flash | **Files:** 2 new + tests | **Estimated:** 3 min

Implement the protobuf OSM loader in C++ with comprehensive tests.

Files to create:
1. `src/data/osm_loader.h` — OSMLoader class: static load_from_file(path) → osm::OSMData, static load_from_proto(bytes) → osm::OSMData, private validate_bounds(OSMData&). Includes schema version check.
2. `src/data/osm_loader.cpp` — Implementation: read binary file, parse with OSMDataProto::ParseFromString, convert protobuf types to internal types, compute bounds from data if invalid, check schema_version matches expected
3. `tests/test_osm_loader.cpp` — Tests: load valid minimal proto, verify building count, verify height values, verify bounds, test schema version mismatch (reports error), test empty file (handles gracefully), test missing fields (defaults). Uses in-memory protobuf construction for tests (no file dependency).

**Verification:** `./map-renderer-tests --gtest_filter=*OSMLoader*` — all tests pass.

---

## Task 9: Style Engine

**Model:** deepseek-v4-flash | **Files:** 2 new + tests | **Estimated:** 2 min

Implement the JSON-based style engine using nlohmann/json.

Files to create:
1. `src/data/style_engine.h` — StyleRule struct (feature_type, fill_color vec4, line_color vec4, line_width_meters, fill_extrusion_height). StyleEngine class: load_from_file(path), load_from_json(json_content), get_rule(feature_type) → StyleRule, static default_style() → StyleEngine. Internal: unordered_map<string,StyleRule>.
2. `src/data/style_engine.cpp` — Implementation: parse JSON array of rules, populate map, fallback to defaults on error. Default colors per HLD: building=#D9C3A5, road_primary=#FEFEFE 8m, road_secondary=#F4F4F4 6m, water=#4D8DC9, park=#8FBA7F, landuse=#E8E0D8, ground=#1E1E20.
3. `tests/test_style_engine.cpp` — Tests: load valid JSON, verify rule lookup, test missing style file falls back to defaults, test invalid JSON falls back with warning, test custom rule overrides default

Create `data/style.json` — default style with the colors above.

**Verification:** `./map-renderer-tests --gtest_filter=*StyleEngine*` — all tests pass.

---

## Task 10: Geometry Builder - Polygon Triangulation and Building Extrusion

**Model:** glm-5.2 | **Files:** 2 new + tests | **Estimated:** 5 min

Implement geometry generation: ear-clipping triangulation, building extrusion, road quad generation. This is algorithmically complex.

Files to create:
1. `src/data/geometry_builder.h` — BuildingVertex (x,y,z, nx,ny,nz). BuildingMesh (vector<BuildingVertex>, indices). PolygonMesh, LineMesh, GroundMesh, GeometryData structs. GeometryBuilder class: static build_buildings(buildings) → BuildingMesh, static build_polygons(features) → PolygonMesh, static build_road_quads(roads, default_width) → LineMesh, static build_ground(half_size) → GroundMesh. Private helpers: triangulate_ear_clip(polygon), extrude_building(footprint, height).
2. `src/data/geometry_builder.cpp` — Implementation:
   - Ear-clipping: find ear (convex vertex with no vertices inside triangle), clip, repeat. CCW winding, no holes in v1.0.
   - Building extrusion: bottom at y=0, top at y=height, side walls (2 tris per edge, outward normal = cross(edge_dir, up)), top cap via ear-clipping, skip bottom. Use VK_FRONT_FACE_COUNTER_CLOCKWISE.
   - Road quads: for each segment, compute perpendicular in XZ plane (cross(edge_dir, up) normalized), extrude ±half_width, emit 2 tris. Shared vertices at joins.
   - Ground plane: large quad at y=0, centered at origin, size = 2*half_size
3. `tests/test_geometry_builder.cpp` — Tests: ear-clip triangle (3 vertices), ear-clip square (2 tris), ear-clip concave L-shape, building extrusion square footprint (verify vertex count, normals, winding), road quad from 2-segment line, ground plane bounds

**Verification:** `./map-renderer-tests --gtest_filter=*Geometry*` — all tests pass.

---

## Task 11: Camera System

**Model:** deepseek-v4-flash | **Files:** 2 new + tests | **Estimated:** 3 min

Implement complete camera system supporting both 2D orthographic and 3D perspective modes.

Files to create:
1. `src/core/camera.h` — CameraMode enum (MODE_2D, MODE_3D). Camera class: position (x,z), zoom(0.1-20), distance(50-5000), tilt(0-85°), rotation(0-360°). Methods: set_position, pan, set_zoom, zoom_by, set_distance, set_tilt, set_rotation, set_mode, get_mode, get_projection_matrix(aspect), get_view_matrix(), get_x/y/z, update_from_input(InputState&), frame_bounds(min/max_x/z), clamp_values(). Private: mode_, x_, z_, data_extent_x/z_, zoom_, distance_, tilt_, rotation_. Constants for MIN/MAX ranges.
2. `src/core/camera.cpp` — Implementation:
   - 2D ortho: scale = min(extent/(2*zoom), extent*aspect/(2*zoom)), ortho centered on (x,z)
   - 3D perspective: 60° FOV, 0.1 near, 10000 far. Spherical coords from distance/tilt/rotation to compute eye position. lookAt(cam_pos, look_at(x,0,z), up(0,1,0))
   - Mode switch: 2D zoom = 500/distance clamped, 3D distance = 500/zoom clamped
   - update_from_input: pan speed = base/zoom (2D) or base*distance/500 (3D), apply keyboard/mouse deltas, clamp
3. `tests/test_camera.cpp` — Tests: 2D projection values, 3D projection values, zoom constraints clamp at 0.1 and 20, tilt clamp at 0 and 85, mode switch preserves position, frame_bounds sets correct initial view, pan moves look-at point

**Verification:** `./map-renderer-tests --gtest_filter=*Camera*` — all tests pass.

---

## Task 12: 2D Shaders and Ground Rendering

**Model:** deepseek-v4-flash | **Files:** 3 new shader files + renderer integration | **Estimated:** 3 min

Create 2D shaders and implement the main Renderer class for ground rendering.

Files to create:
1. `src/shaders/2d/ground.vert` — in vec2 inPosition, uniform CameraUBO, out vec3 fragWorldPos. Transforms world pos (x,0,z) to clip space.
2. `src/shaders/2d/ground.frag` — in vec3 fragWorldPos, out vec4 outColor. Dark base (#1A1A1C), grid lines every 50m using fwidth(), subtle intensity.
3. `src/shaders/2d/road.vert` — in vec3 inPosition (x,y,z triplets from road quads), uniform CameraUBO. No lighting.
4. `src/shaders/2d/road.frag` — uniform vec4 color (or push constant), out vec4 outColor.

Create:
5. `src/core/renderer.h` — Renderer class: initialize(vk_ctx, window), cleanup(), render_frame(camera), set_data(osmData, geometry). Private: uniform_buffer_, camera_descriptor_, pipeline pointers (ground_2d, fill_2d, road_2d, ground_3d, building_3d, feature_3d), vertex/index buffers (building, ground, feature, road), command_buffers_, sync objects (per-frame and per-image). Record command buffer per frame.
6. `src/core/renderer.cpp` — initialize: create uniform buffer for CameraUBO, create camera descriptor set (binding 0), create ShaderManager and PipelineManager, load 2D ground shaders, create ground pipeline (vertex input: vec2, cull_mode=NONE, depth_test=false, blend=false), create ground vertex buffer (large quad at y=0). render_frame: acquire image, update UBO with camera matrices, begin render pass, bind ground pipeline, bind camera descriptor set, draw ground (6 vertices for quad), end render pass, submit+present. Cleanup properly.
7. Update `src/main.cpp` — integrate Renderer: after VulkanContext init, create Camera, create Renderer, call renderer.render_frame(camera) in loop instead of draw_frame

**Verification:** `make && ./map-renderer` — window shows dark ground with grid lines, zoomable with +/-. 

---

## Task 13: Polygon Fill and Road Quad Rendering

**Model:** deepseek-v4-flash | **Files:** modified renderer + new shaders | **Estimated:** 4 min

Implement polygon fill rendering (parks, water, landuse) and road quad rendering in 2D mode.

Update renderer.cpp:
- Create fill shader: uses 2D fill.vert/.frag (create these: vertex takes vec2, frag outputs push constant color)
- Create fill pipeline: polygon mode=FILL, cull=NONE, depth=false, uses push constant for color (vec4, 16 bytes)
- Create road pipeline: uses road shaders, same as fill but with different vertex input (vec3 positions)
- In initialize(): load fill shaders, create fill pipeline, create road pipeline
- In set_data(): upload geometry to GPU — create vertex/index buffers for polygons and roads, upload to GPU using staging buffers
- In record_command_buffer(): after ground draw, bind fill pipeline → for each feature type (parks/water/landuse), push style color, bind feature buffers, draw indexed. Then bind road pipeline → push road color, bind road buffers, draw indexed.
- Load style engine in renderer initialization, use style colors

**Verification:** After running Python preprocessor and C++ loader, `./map-renderer` shows ground with colored polygons (parks green, water blue) and white/grey roads. All features visible in correct z-order.

---

## Task 14: 3D Building Extrusion Rendering

**Model:** glm-5.2 | **Files:** 2 shader files + renderer updates | **Estimated:** 4 min

Implement 3D building shaders and integrate building rendering into the Renderer.

Files to create:
1. `src/shaders/3d/building.vert` — in vec3 inPosition, in vec3 inNormal, uniform CameraUBO, push_constant vec4 color, out vec4 fragColor, out vec3 fragNormal. Transforms position, passes color and normal.
2. `src/shaders/3d/building.frag` — in vec4 fragColor, in vec3 fragNormal, out vec4 outColor. Normalize normal, directional light (0.3, 0.85, 0.4), ambient 0.35 + diffuse 0.65, top face boost (0.85 + 0.15*normal.y).

Update renderer.cpp:
- Initialize 3D building pipeline: vertex input has position vec3 + normal vec3, cull_mode=BACK, front_face=COUNTER_CLOCKWISE, depth_test=true, depth_write=true, push_constant_size=16
- Load 3D shaders, create pipeline
- In record_command_buffer: if camera mode is 3D, after ground, bind building pipeline, push building color, bind building vertex/index buffers, draw indexed

Also create:
3. `src/shaders/3d/ground3d.vert` — same as 2D ground but with depth writing
4. `src/shaders/3d/ground3d.frag` — same grid pattern as 2D

**Verification:** `make && ./map-renderer` — press F2 for 3D mode, buildings visible with extrusion, lighting, proper depth occlusion. Backface culling works (can't see through buildings).

---

## Task 15: Camera Controls and Mode Switching

**Model:** deepseek-v4-flash | **Files:** modified main.cpp + window.cpp | **Estimated:** 2 min

Wire up InputState population from SDL events and connect camera.update_from_input().

Update:
1. `src/core/window.cpp` — In poll_events(InputState&): handle SDL_KEYDOWN/KEYUP for arrow keys, +/-/=, Q/E, A/D, F1/F2. Handle SDL_MOUSEBUTTONDOWN/UP for left button. Handle SDL_MOUSEMOTION for mouse delta. Handle SDL_MOUSEWHEEL for scroll. Set edge flags (pressed/released) correctly. Compute dt from frame time.
2. `src/main.cpp` — In main loop: window.poll_events(inputState), camera.update_from_input(inputState), check for mode switch (F1/F2), then renderer.render_frame(camera). Add FPS counter (optional, print to stdout or window title).
3. `tests/test_input_state.cpp` — Verify key mappings, edge detection, mouse button state transitions

**Verification:** `./map-renderer` — arrow keys pan, +/- zoom, mouse drag pans, scroll zooms, Q/E tilt (3D), A/D rotate (3D), F1/F2 switch modes, ESC quits. Mode switch preserves look-at position.

---

## Task 16: Integration Testing and Sanitization

**Model:** deepseek-v4-flash | **Files:** test files + documentation | **Estimated:** 3 min

Write integration tests, verify full pipeline end-to-end, add sanitizer support.

Create:
1. `tests/test_integration.cpp` — Integration tests: load small protobuf → verify OSM data → build geometry → verify mesh vertex counts → verify all indices valid. Test coordinate consistency: loaded data uses same units as camera (ENU meters). Test style fallback: missing style file → default colors applied.
2. `tests/test_renderer.cpp` — Renderer tests: initialization succeeds, uniform buffer updates correctly, all pipelines created, cleanup doesn't leak

Update `CMakeLists.txt`:
- Add AddressSanitizer option: `-fsanitize=address` in debug builds
- Add LeakSanitizer option

Update `README.md`:
- Complete documentation: build instructions, dependency list, controls, data pipeline overview, architecture diagram

**Verification:** `cmake -DCMAKE_BUILD_TYPE=Debug .. && make && ./map-renderer-tests` — all tests pass. `ASAN_OPTIONS=detect_leaks=1 ./map-renderer-tests` — no leaks detected.

---

## Task 17: Performance Optimization and Visual Polish

**Model:** deepseek-v4-flash (or kimi-k2.7-code for visual debug) | **Files:** modifications | **Estimated:** 3 min

Optimize rendering performance and polish visuals.

Optimizations:
- Add frustum culling: compute camera frustum planes from proj*view matrix, cull building AABBs outside frustum before draw call
- Batch draws by material: group all buildings into one draw call (already done with single vertex/index buffer)
- Minimize UBO updates: only update when camera changed

Visual polish:
- Improve color palette: test different ground/base colors
- Smooth camera transitions: interpolate zoom changes
- Add distance-based fade: far buildings slightly darker (fog effect in shader)

Profile and verify performance:
- FPS counter in window title
- Measure with 40K buildings dataset

**Verification:** `./map-renderer` — 30+ FPS on GTX 1060/RX 580, smooth controls, visually appealing colors.

---

## Execution Notes

1. Each task = one `opencode run` call. Prompts must be fully self-contained.
2. After each task: verify `make -j$(nproc)` succeeds with no warnings, then `./map-renderer-tests` passes, then `git add -A && git commit -m "task N: description" && git push`
3. Create a `.md` doc after each task in `docs/task-N.md` with summary of what was done
4. If a task fails: debug with `opencode run` re-run, do NOT proceed to next task
5. Model selection: glm-5.2 for T2-T5 (Vulkan setup) and T10 (geometry) and T14 (building shader). deepseek-v4-flash for everything else. Kimi-k2.7-code only if visual debugging is needed.
6. All code must have doxygen comments for public APIs
7. All modules must use DEBUG_LOG macro where appropriate
