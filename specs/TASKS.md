# Detailed Task Breakdown
## Interactive 3D Map Renderer

**Version:** 1.1  
**Date:** July 2, 2026  
**Status:** Specification - Optimized after review

---

## Overview

This document breaks down the project into detailed tasks and subtasks. Each task is small enough to be completed in 1-4 hours and has clear acceptance criteria.

**Total Tasks:** 50  
**Total Subtasks:** 200+  
**Estimated Effort:** 6–7 weeks

---

## Phase 1: Project Foundation (Week 1)

### Task 1.1: Set up build system
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 1.1.1: Create CMakeLists.txt with C++23 support
- [ ] 1.1.2: Find and link Vulkan, SDL2, GLM dependencies
- [ ] 1.1.3: Add protobuf C++ via CMake (system or FetchContent)
- [ ] 1.1.4: Add nlohmann/json header via FetchContent
- [ ] 1.1.5: Add Google Test via FetchContent
- [ ] 1.1.6: Set up compiler flags (-Wall -Wextra -Wpedantic)
- [ ] 1.1.7: Create build directory structure
- [ ] 1.1.8: Add .gitignore for build artifacts
- [ ] 1.1.9: Create README.md with build instructions
- [ ] 1.1.10: Verify empty project builds successfully

**Acceptance Criteria:**
- `cmake ..` succeeds
- `make` compiles without errors
- All dependencies are found or fetched
- Protobuf compiler (`protoc`) is available

---

### Task 1.2: Create project structure
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 1.2.1: Create src/ directory
- [ ] 1.2.2: Create src/core/ for window, input, renderer, camera
- [ ] 1.2.3: Create src/data/ for OSM loader, geometry builder, style engine
- [ ] 1.2.4: Create src/graphics/ for Vulkan wrappers
- [ ] 1.2.5: Create src/proto/ for .proto schema
- [ ] 1.2.6: Create src/shaders/ for GLSL files
- [ ] 1.2.7: Create tests/ directory
- [ ] 1.2.8: Create tools/ for Python scripts
- [ ] 1.2.9: Create data/ for sample data

**Acceptance Criteria:**
- Directory structure matches LLD
- All directories have .gitkeep files

---

### Task 1.3: Set up Google Test framework
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 1.3.1: Fetch Google Test via CMake FetchContent
- [ ] 1.3.2: Create test executable target
- [ ] 1.3.3: Create test_main.cpp with GTest initialization
- [ ] 1.3.4: Add first dummy test
- [ ] 1.3.5: Verify tests run

**Acceptance Criteria:**
- `./map-renderer-tests` runs
- Dummy test passes

---

### Task 1.4: Add debug logging infrastructure
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 1.4.1: Create `src/core/debug_log.h`
- [ ] 1.4.2: Implement `DEBUG_LOG` macro gated by `MAP_RENDERER_DEBUG`
- [ ] 1.4.3: Support formatted output (e.g., `std::format` or `fmt`-like)
- [ ] 1.4.4: Add compile-time no-op in release builds
- [ ] 1.4.5: Write unit test verifying macro compiles in both modes

**Acceptance Criteria:**
- `DEBUG_LOG` prints in debug builds
- `DEBUG_LOG` compiles away in release builds
- No runtime overhead when disabled

---

## Phase 2: Minimal Vulkan Window (Week 1)

### Task 2.1: Create Window class
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 2.1.1: Create src/core/window.h with class declaration
- [ ] 2.1.2: Implement SDL_Init in constructor
- [ ] 2.1.3: Implement SDL_CreateWindow
- [ ] 2.1.4: Implement event polling
- [ ] 2.1.5: Implement should_close()
- [ ] 2.1.6: Implement cleanup
- [ ] 2.1.7: Add error handling
- [ ] 2.1.8: Test window creation and closing

**Acceptance Criteria:**
- Window opens with title "Map Renderer"
- Window closes on ESC or close button
- No memory leaks

---

### Task 2.2: Initialize Vulkan instance
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 2.2.1: Create src/core/vulkan_context.h
- [ ] 2.2.2: Create VkApplicationInfo structure
- [ ] 2.2.3: Get required extensions from SDL
- [ ] 2.2.4: Create VkInstance
- [ ] 2.2.5: Check for validation layer support
- [ ] 2.2.6: Enable validation layers in debug builds
- [ ] 2.2.7: Set up debug messenger
- [ ] 2.2.8: Test instance creation

**Acceptance Criteria:**
- Vulkan instance created successfully
- Validation layers work in debug mode
- No errors reported

---

### Task 2.3: Create Vulkan surface
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 2.3.1: Call SDL_Vulkan_CreateSurface
- [ ] 2.3.2: Check return value
- [ ] 2.3.3: Store surface handle
- [ ] 2.3.4: Test surface creation

**Acceptance Criteria:**
- Surface created successfully
- Surface associated with window

---

### Task 2.4: Select physical device
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.4.1: Enumerate physical devices
- [ ] 2.4.2: Check for discrete GPU preference
- [ ] 2.4.3: Check queue family support
- [ ] 2.4.4: Check surface support
- [ ] 2.4.5: Select best device
- [ ] 2.4.6: Store device handle

**Acceptance Criteria:**
- Physical device selected
- Device supports graphics and present queues

---

### Task 2.5: Create logical device
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.5.1: Create queue create info
- [ ] 2.5.2: Enable required device features
- [ ] 2.5.3: Enable swapchain extension
- [ ] 2.5.4: Create VkDevice
- [ ] 2.5.5: Get queue handles
- [ ] 2.5.6: Test device creation

**Acceptance Criteria:**
- Logical device created
- Graphics queue obtained

---

### Task 2.6: Create swapchain
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 2.6.1: Query swapchain support
- [ ] 2.6.2: Choose surface format (prefer SRGB)
- [ ] 2.6.3: Choose present mode (prefer MAILBOX)
- [ ] 2.6.4: Choose extent
- [ ] 2.6.5: Create VkSwapchainKHR
- [ ] 2.6.6: Get swapchain images
- [ ] 2.6.7: Create image views
- [ ] 2.6.8: Test swapchain

**Acceptance Criteria:**
- Swapchain created
- Image views created
- Optimal settings chosen

---

### Task 2.7: Create depth buffer
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.7.1: Choose depth format (D32_SFLOAT or D24_UNORM_S8_UINT)
- [ ] 2.7.2: Create depth image and memory
- [ ] 2.7.3: Create depth image view
- [ ] 2.7.4: Test depth image creation

**Acceptance Criteria:**
- Depth image/view created
- Format supported by device

---

### Task 2.8: Create render pass
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.8.1: Define color attachment
- [ ] 2.8.2: Define depth attachment
- [ ] 2.8.3: Define subpass
- [ ] 2.8.4: Define dependencies
- [ ] 2.8.5: Create VkRenderPass
- [ ] 2.8.6: Test render pass

**Acceptance Criteria:**
- Render pass created with color + depth

---

### Task 2.9: Create framebuffers
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 2.9.1: Create framebuffer for each swapchain image
- [ ] 2.9.2: Attach color and depth views
- [ ] 2.9.3: Test framebuffers

**Acceptance Criteria:**
- One framebuffer per swapchain image

---

### Task 2.10: Create command pool and buffers
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.10.1: Create command pool
- [ ] 2.10.2: Allocate one command buffer per swapchain image
- [ ] 2.10.3: Test command buffer allocation

**Acceptance Criteria:**
- Command pool created
- Command buffers allocated

---

### Task 2.11: Create sync objects
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.11.1: Define `kMaxFramesInFlight` (2)
- [ ] 2.11.2: Create per-frame image-available semaphores
- [ ] 2.11.3: Create per-frame render-finished semaphores
- [ ] 2.11.4: Create per-frame in-flight fences
- [ ] 2.11.5: Create per-swapchain-image fences
- [ ] 2.11.6: Test sync object creation

**Acceptance Criteria:**
- All sync objects created
- Synchronization model matches LLD

---

### Task 2.12: Main render loop
**Priority:** Critical  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 2.12.1: Acquire next image with timeout
- [ ] 2.12.2: Wait for and reset per-frame fence
- [ ] 2.12.3: Check per-image fence before reuse
- [ ] 2.12.4: Record command buffer (clear screen)
- [ ] 2.12.5: Submit command buffer with correct wait/signal stages
- [ ] 2.12.6: Present image
- [ ] 2.12.7: Handle swapchain recreate (resize)
- [ ] 2.12.8: Test clear color visible

**Acceptance Criteria:**
- Window shows clear color
- No validation errors
- Window resize handled

---

## Phase 3: Hello Triangle (Week 2) — Optional Vertical Slice

**Note:** The Hello Triangle phase is a learning/validation milestone. It may be skipped once the renderer can draw the ground plane; however, it is strongly recommended for first-time Vulkan setup to isolate pipeline/shader issues before map data is involved.

### Task 3.1: Create first shader
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 3.1.1: Create src/shaders/triangle.vert (GLSL)
- [ ] 3.1.2: Create src/shaders/triangle.frag (GLSL)
- [ ] 3.1.3: Add shader compilation to CMake
- [ ] 3.1.4: Test compilation produces .spv files
- [ ] 3.1.5: Verify shaders compile without errors

**Acceptance Criteria:**
- Both shaders compile to SPIR-V
- No compilation errors

---

### Task 3.2: Create shader module loader
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 3.2.1: Read .spv file into buffer
- [ ] 3.2.2: Create VkShaderModule
- [ ] 3.2.3: Add error checking
- [ ] 3.2.4: Test module creation

**Acceptance Criteria:**
- Can load SPIR-V files
- Shader modules created successfully

---

### Task 3.3: Create graphics pipeline
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 3.3.1: Define vertex input (none for now)
- [ ] 3.3.2: Define input assembly (triangle list)
- [ ] 3.3.3: Define viewport state
- [ ] 3.3.4: Define rasterization (no culling)
- [ ] 3.3.5: Define multisampling (1 sample)
- [ ] 3.3.6: Define depth stencil
- [ ] 3.3.7: Define color blending
- [ ] 3.3.8: Create pipeline layout
- [ ] 3.3.9: Create graphics pipeline
- [ ] 3.3.10: Test pipeline creation

**Acceptance Criteria:**
- Pipeline created successfully
- Pipeline can be bound

---

### Task 3.4: Draw triangle
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 3.4.1: Begin render pass
- [ ] 3.4.2: Bind pipeline
- [ ] 3.4.3: Draw 3 vertices (no vertex buffer)
- [ ] 3.4.4: End render pass
- [ ] 3.4.5: Test triangle appears on screen

**Acceptance Criteria:**
- Colored triangle visible on screen
- Triangle covers reasonable area

---

## Phase 4: Data Pipeline (Week 2)

**Goal:** Replace the previous JSON-based pipeline with a protobuf-based pipeline that carries local ENU meters and pre-resolved building heights.

### Task 4.1: Define OSM data structures
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 4.1.1: Create src/data/osm_types.h
- [ ] 4.1.2: Define `osm::Point` (local ENU meters)
- [ ] 4.1.3: Define `osm::Building` with height source enum
- [ ] 4.1.4: Define `osm::Road`
- [ ] 4.1.5: Define `osm::PolygonFeature`
- [ ] 4.1.6: Define `osm::OSMData`
- [ ] 4.1.7: Add bounds and center fields

**Acceptance Criteria:**
- All data structures defined
- Proper namespacing
- Height source enum present

---

### Task 4.2: Create protobuf schema
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 4.2.1: Create src/proto/osm_data.proto
- [ ] 4.2.2: Define Point2D, Building, Road, PolygonFeature, OSMDataProto
- [ ] 4.2.3: Add schema version field
- [ ] 4.2.4: Add CMake rule to generate C++ and Python code from .proto
- [ ] 4.2.5: Verify generated code compiles

**Acceptance Criteria:**
- .proto file defined
- C++ generated sources compile
- Python generated module imports successfully

---

### Task 4.3: Implement protobuf OSM loader
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 4.3.1: Implement `OSMLoader::load_from_file`
- [ ] 4.3.2: Implement `OSMLoader::load_from_proto`
- [ ] 4.3.3: Convert protobuf messages to internal `osm::OSMData`
- [ ] 4.3.4: Compute bounds and center
- [ ] 4.3.5: Validate schema version
- [ ] 4.3.6: Write unit tests

**Acceptance Criteria:**
- Can load a protobuf file into OSMData
- Bounds correctly computed
- Schema mismatch reported

---

### Task 4.4: Create Python preprocessing tool
**Priority:** Critical  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 4.4.1: Install Python protobuf and osmium packages
- [ ] 4.4.2: Read OSM PBF file
- [ ] 4.4.3: Convert WGS84 lat/lon to local ENU meters centered on dataset
- [ ] 4.4.4: Extract buildings with height fallback (tag → levels → default)
- [ ] 4.4.5: Extract roads and road widths
- [ ] 4.4.6: Extract parks, water, landuse polygons
- [ ] 4.4.7: Serialize to protobuf binary file
- [ ] 4.4.8: Write unit tests
- [ ] 4.4.9: Test with NewDelhi.osm.pbf

**Acceptance Criteria:**
- Can extract OSM data
- Output is valid protobuf
- Coordinates are in local ENU meters
- All buildings have valid heights

---

## Phase 5: 2D Rendering (Week 3)

### Task 5.1: Implement style engine
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 5.1.1: Create src/data/style_engine.h/.cpp
- [ ] 5.1.2: Define StyleRule struct
- [ ] 5.1.3: Load style JSON with nlohmann/json
- [ ] 5.1.4: Implement built-in default style
- [ ] 5.1.5: Write unit tests

**Acceptance Criteria:**
- Style engine loads JSON
- Missing style falls back to defaults
- Rules accessible by feature type

---

### Task 5.2: Camera class - 2D mode
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 5.2.1: Create src/core/camera.h
- [ ] 5.2.2: Define CameraMode enum and InputState
- [ ] 5.2.3: Implement 2D orthographic projection based on data extent
- [ ] 5.2.4: Implement zoom (0.1x to 20x, 1.0 = full extent)
- [ ] 5.2.5: Implement pan in ENU meters
- [ ] 5.2.6: Implement `frame_bounds`
- [ ] 5.2.7: Write unit tests for matrix calculations

**Acceptance Criteria:**
- 2D camera produces correct matrices
- Zoom 1.0 frames the full dataset
- `frame_bounds` sets correct initial view

---

### Task 5.3: Ground plane shader
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 5.3.1: Create 2D ground vertex shader
- [ ] 5.3.2: Create 2D ground fragment shader
- [ ] 5.3.3: Add grid pattern
- [ ] 5.3.4: Compile shaders
- [ ] 5.3.5: Test compilation

**Acceptance Criteria:**
- Shaders compile
- Grid pattern visible

---

### Task 5.4: Render ground in 2D
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 5.4.1: Create vertex buffer for ground quad
- [ ] 5.4.2: Create ground pipeline
- [ ] 5.4.3: Bind pipeline and draw
- [ ] 5.4.4: Update camera UBO
- [ ] 5.4.5: Test ground visible

**Acceptance Criteria:**
- Ground plane visible
- Grid pattern visible

---

### Task 5.5: Polygon fill rendering
**Priority:** High  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 5.5.1: Implement ear-clipping triangulation
- [ ] 5.5.2: Validate winding order
- [ ] 5.5.3: Create vertex/index buffers
- [ ] 5.5.4: Create fill shader
- [ ] 5.5.5: Create fill pipeline
- [ ] 5.5.6: Render parks, water, landuse
- [ ] 5.5.7: Test features visible

**Acceptance Criteria:**
- Parks, water, landuse visible
- Concave polygons rendered correctly
- Correct colors

---

### Task 5.6: Road quad rendering
**Priority:** High  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 5.6.1: Implement road quad generation in geometry builder
- [ ] 5.6.2: Create road vertex/index buffers
- [ ] 5.6.3: Create road shader
- [ ] 5.6.4: Create road pipeline
- [ ] 5.6.5: Render roads with correct widths
- [ ] 5.6.6: Test roads visible

**Acceptance Criteria:**
- Roads visible as solid quads
- Width proportional to configured meters
- No reliance on Vulkan line width

---

## Phase 6: 3D Rendering (Week 4)

### Task 6.0: Ensure depth testing is active
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 6.0.1: Enable depth test in 3D pipelines
- [ ] 6.0.2: Clear depth buffer each frame
- [ ] 6.0.3: Verify 3D draw order with depth

**Acceptance Criteria:**
- Depth testing enabled for all 3D passes
- Buildings correctly occlude each other

---

### Task 6.1: Building extrusion
**Priority:** Critical  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 6.1.1: Define BuildingVertex with normal
- [ ] 6.1.2: Implement extrude_building()
- [ ] 6.1.3: Generate top cap with normal (0,1,0)
- [ ] 6.1.4: Generate side faces with outward normals
- [ ] 6.1.5: Ensure correct counter-clockwise winding order when viewed from outside
- [ ] 6.1.6: Skip hidden bottom cap to save geometry
- [ ] 6.1.7: Write unit tests

**Acceptance Criteria:**
- Buildings extruded correctly
- Normals correct
- Top and side faces present
- Backface culling works correctly

---

### Task 6.2: Camera class - 3D mode
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 6.2.1: Implement 3D perspective projection
- [ ] 6.2.2: Implement spherical camera positioning
- [ ] 6.2.3: Implement distance control
- [ ] 6.2.4: Implement tilt control
- [ ] 6.2.5: Implement rotation control
- [ ] 6.2.6: Write unit tests

**Acceptance Criteria:**
- 3D camera produces correct matrices
- All controls work

---

### Task 6.3: 3D ground shader
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 6.3.1: Create 3D ground vertex shader
- [ ] 6.3.2: Create 3D ground fragment shader
- [ ] 6.3.3: Add depth testing
- [ ] 6.3.4: Test compilation

**Acceptance Criteria:**
- 3D ground renders correctly
- Depth testing works

---

### Task 6.4: Building shader
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 6.4.1: Create building vertex shader
- [ ] 6.4.2: Create building fragment shader
- [ ] 6.4.3: Add per-vertex normal attribute
- [ ] 6.4.4: Add lighting calculation
- [ ] 6.4.5: Test compilation

**Acceptance Criteria:**
- Building shader compiles
- Lighting works

---

### Task 6.5: Render buildings in 3D
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 6.5.1: Create building vertex/index buffers
- [ ] 6.5.2: Create building pipeline
- [ ] 6.5.3: Update vertex input for normals
- [ ] 6.5.4: Draw buildings
- [ ] 6.5.5: Test buildings visible

**Acceptance Criteria:**
- Buildings visible in 3D
- Proper lighting
- Correct colors

---



---

## Phase 7: Interactivity (Week 5)

### Task 7.1: Input state system
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 7.1.1: Create `InputState` struct
- [ ] 7.1.2: Populate input state from SDL events each frame
- [ ] 7.1.3: Track key and mouse button edges
- [ ] 7.1.4: Compute frame delta time
- [ ] 7.1.5: Write unit tests for input state mapping

**Acceptance Criteria:**
- InputState reflects current keyboard and mouse state
- Edge flags work correctly

---

### Task 7.2: Keyboard input
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 7.2.1: Map keys to actions via InputState
- [ ] 7.2.2: Update camera from keys
- [ ] 7.2.3: Test all keys work

**Acceptance Criteria:**
- All keys work as specified

---

### Task 7.3: Mouse input
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 7.3.1: Handle mouse button events
- [ ] 7.3.2: Handle mouse motion events
- [ ] 7.3.3: Handle mouse wheel events
- [ ] 7.3.4: Implement drag panning
- [ ] 7.3.5: Test mouse controls

**Acceptance Criteria:**
- Mouse drag pans camera
- Scroll wheel zooms

---

### Task 7.4: Mode switching
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 7.4.1: Handle F1/F2 keys
- [ ] 7.4.2: Switch camera mode
- [ ] 7.4.3: Preserve look-at point across modes
- [ ] 7.4.4: Derive 3D distance from 2D zoom or vice versa
- [ ] 7.4.5: Test mode switching

**Acceptance Criteria:**
- F1 switches to 2D
- F2 switches to 3D
- Look-at point (x, z) preserved across modes
- 3D distance roughly matches 2D zoom level

---

### Task 7.5: Camera constraints
**Priority:** Medium  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 7.5.1: Clamp zoom to valid range
- [ ] 7.5.2: Clamp distance to valid range
- [ ] 7.5.3: Clamp tilt to valid range
- [ ] 7.5.4: Test constraints

**Acceptance Criteria:**
- All values stay in valid range

---

## Phase 8: Testing (Week 5)

### Task 8.1: Unit tests for camera
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 8.1.1: Test 2D projection matrix
- [ ] 8.1.2: Test 3D projection matrix
- [ ] 8.1.3: Test view matrix calculation
- [ ] 8.1.4: Test zoom constraints
- [ ] 8.1.5: Test mode switching

**Acceptance Criteria:**
- All camera tests pass

---

### Task 8.2: Unit tests for OSM loader
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 8.2.1: Test protobuf loading
- [ ] 8.2.2: Test building height fallback
- [ ] 8.2.3: Test road parsing
- [ ] 8.2.4: Test feature parsing
- [ ] 8.2.5: Test bounds calculation
- [ ] 8.2.6: Test schema version mismatch handling

**Acceptance Criteria:**
- All loader tests pass

---

### Task 8.3: Unit tests for geometry
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 8.3.1: Test building extrusion
- [ ] 8.3.2: Test normal generation
- [ ] 8.3.3: Test ear-clipping triangulation (convex and concave)
- [ ] 8.3.4: Test road quad generation

**Acceptance Criteria:**
- All geometry tests pass

---

### Task 8.4: Integration tests
**Priority:** Medium  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 8.4.1: Test full pipeline (load → build → render)
- [ ] 8.4.2: Test coordinate consistency between 2D and 3D
- [ ] 8.4.3: Test camera bounds and constraints
- [ ] 8.4.4: Test style fallback behavior
- [ ] 8.4.5: Run tests under AddressSanitizer

**Acceptance Criteria:**
- All integration tests pass
- No memory errors under ASan

---

## Phase 9: Polish (Week 6)

### Task 9.1: Performance optimization
**Priority:** Medium  
**Estimated Time:** 5 hours

**Subtasks:**
- [ ] 9.1.1: Profile current performance
- [ ] 9.1.2: Add frustum culling
- [ ] 9.1.3: Add distance-based culling
- [ ] 9.1.4: Batch draw calls by pipeline/material
- [ ] 9.1.5: Optimize buffer usage
- [ ] 9.1.6: Measure improvements

**Acceptance Criteria:**
- 30+ FPS achieved on target hardware

---

### Task 9.2: Visual polish
**Priority:** Low  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 9.2.1: Improve colors
- [ ] 9.2.2: Add smooth transitions
- [ ] 9.2.3: Polish grid pattern
- [ ] 9.2.4: Test visual quality

**Acceptance Criteria:**
- Visually appealing output

---

### Task 9.3: Documentation
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 9.3.1: Update README.md
- [ ] 9.3.2: Add build instructions (dependencies, protobuf generation)
- [ ] 9.3.3: Add usage guide
- [ ] 9.3.4: Document keyboard and mouse controls
- [ ] 9.3.5: Document protobuf preprocessing workflow
- [ ] 9.3.6: Add code comments

**Acceptance Criteria:**
- Complete documentation

---

### Task 9.4: Final testing
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 9.4.1: Run all unit tests
- [ ] 9.4.2: Run integration tests
- [ ] 9.4.3: Manual testing (2D/3D modes, controls, mode switching)
- [ ] 9.4.4: Check for memory leaks with ASan/LSan
- [ ] 9.4.5: Verify build on clean system
- [ ] 9.4.6: Run with Vulkan validation layers enabled

**Acceptance Criteria:**
- All tests pass
- No memory errors under ASan/LSan
- Clean build works
- No Vulkan validation errors

---

## Task Dependencies

```
Task 1.1 (Build system) → All other tasks
Task 1.4 (Debug logging) → All core modules
Task 2.1 (Window) → Task 2.2 (Vulkan instance) → Task 2.3 (Surface)
Task 2.4 (Physical device) → Task 2.5 (Logical device)
Task 2.6 (Swapchain) → Task 2.7 (Depth buffer) → Task 2.8 (Render pass) → Task 2.9 (Framebuffers)
Task 2.10 (Command pool) → Task 2.12 (Main loop)
Task 3.1 (Shaders) → Task 3.3 (Pipeline) → Task 3.4 (Draw)
Task 4.1 (Data structures) → Task 4.2 (Protobuf schema) → Task 4.3 (OSM loader)
Task 4.2 (Protobuf schema) → Task 4.4 (Python preprocessor)
Task 5.1 (Style engine) → Task 5.5 (Polygon fills), Task 5.6 (Roads)
Task 5.2 (2D camera) → Task 5.4 (Render ground), Task 7.4 (Mode switching)
Task 5.3 (Ground shader) → Task 5.4 (Render ground)
Task 6.0 (Depth testing) → Task 6.5 (Render buildings)
Task 6.1 (Extrusion) → Task 6.5 (Render buildings)
Task 6.2 (3D camera) → Task 6.5 (Render buildings)
Task 7.1 (Input state) → Task 7.2 (Keyboard), Task 7.3 (Mouse)
Task 7.2 (Keyboard) → Task 7.4 (Mode switching)
Task 7.3 (Mouse) → Task 7.4 (Mode switching)
```

---

## Definition of Done

A task is complete when:
- [ ] Code implemented
- [ ] Unit tests written and passing
- [ ] Manual testing completed
- [ ] No compiler warnings
- [ ] Code follows style guidelines
- [ ] Committed to repository

---

## Notes

- Each task should be completable in 1-4 hours
- If a task takes longer, break it down further
- Test after each task
- Commit after each task
- Don't move to next task until current is working
- Update this TASKS.md file if estimates or dependencies change
