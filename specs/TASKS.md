# Detailed Task Breakdown
## Interactive 3D Map Renderer

**Version:** 1.0  
**Date:** July 2, 2026  
**Status:** Specification - Ready for Review

---

## Overview

This document breaks down the project into detailed tasks and subtasks. Each task is small enough to be completed in 1-4 hours and has clear acceptance criteria.

**Total Tasks:** 45  
**Total Subtasks:** 180+  
**Estimated Effort:** 6 weeks

---

## Phase 1: Project Foundation (Week 1)

### Task 1.1: Set up build system
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 1.1.1: Create CMakeLists.txt with C++23 support
- [ ] 1.1.2: Find and link Vulkan, SDL2, GLM dependencies
- [ ] 1.1.3: Set up compiler flags (-Wall -Wextra -Wpedantic)
- [ ] 1.1.4: Create build directory structure
- [ ] 1.1.5: Add .gitignore for build artifacts
- [ ] 1.1.6: Create README.md with build instructions
- [ ] 1.1.7: Verify empty project builds successfully

**Acceptance Criteria:**
- `cmake ..` succeeds
- `make` compiles without errors
- All dependencies are found

---

### Task 1.2: Create project structure
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 1.2.1: Create src/ directory
- [ ] 1.2.2: Create src/core/ for window and renderer
- [ ] 1.2.3: Create src/data/ for OSM loader
- [ ] 1.2.4: Create src/graphics/ for Vulkan wrappers
- [ ] 1.2.5: Create src/shaders/ for GLSL files
- [ ] 1.2.6: Create tests/ directory
- [ ] 1.2.7: Create tools/ for Python scripts
- [ ] 1.2.8: Create data/ for sample data

**Acceptance Criteria:**
- Directory structure matches LLD
- All directories have .gitkeep files

---

### Task 1.3: Set up Google Test framework
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 1.3.1: Find Google Test in CMake
- [ ] 1.3.2: Create test executable target
- [ ] 1.3.3: Create test_main.cpp with GTest initialization
- [ ] 1.3.4: Add first dummy test
- [ ] 1.3.5: Verify tests run

**Acceptance Criteria:**
- `./map-renderer-tests` runs
- Dummy test passes

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

### Task 2.7: Create render pass
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.7.1: Define color attachment
- [ ] 2.7.2: Define depth attachment
- [ ] 2.7.3: Define subpass
- [ ] 2.7.4: Define dependencies
- [ ] 2.7.5: Create VkRenderPass
- [ ] 2.7.6: Test render pass

**Acceptance Criteria:**
- Render pass created with color + depth

---

### Task 2.8: Create framebuffers
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 2.8.1: Create framebuffer for each swapchain image
- [ ] 2.8.2: Attach color and depth views
- [ ] 2.8.3: Test framebuffers

**Acceptance Criteria:**
- One framebuffer per swapchain image

---

### Task 2.9: Create command pool and buffers
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 2.9.1: Create command pool
- [ ] 2.9.2: Allocate command buffers
- [ ] 2.9.3: Test command buffer allocation

**Acceptance Criteria:**
- Command pool created
- Command buffers allocated

---

### Task 2.10: Create sync objects
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 2.10.1: Create image available semaphore
- [ ] 2.10.2: Create render finished semaphore
- [ ] 2.10.3: Create in-flight fence
- [ ] 2.10.4: Test sync objects

**Acceptance Criteria:**
- All sync objects created

---

### Task 2.11: Main render loop
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 2.11.1: Acquire next image
- [ ] 2.11.2: Reset fence
- [ ] 2.11.3: Record command buffer (clear screen)
- [ ] 2.11.4: Submit command buffer
- [ ] 2.11.5: Present image
- [ ] 2.11.6: Wait for fence
- [ ] 2.11.7: Test clear color visible

**Acceptance Criteria:**
- Window shows clear color
- No validation errors

---

## Phase 3: Hello Triangle (Week 2)

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

### Task 4.1: Create OSM data structures
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 4.1.1: Create src/data/osm_types.h
- [ ] 4.1.2: Define MercatorPoint
- [ ] 4.1.3: Define Building
- [ ] 4.1.4: Define Road
- [ ] 4.1.5: Define PolygonFeature
- [ ] 4.1.6: Define OSMData
- [ ] 4.1.7: Add bounds fields

**Acceptance Criteria:**
- All data structures defined
- Proper namespacing

---

### Task 4.2: Implement simple JSON parser
**Priority:** High  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 4.2.1: Create JSON tokenizer
- [ ] 4.2.2: Parse null, bool, number, string
- [ ] 4.2.3: Parse arrays
- [ ] 4.2.4: Parse objects
- [ ] 4.2.5: Handle nested structures
- [ ] 4.2.6: Add error reporting
- [ ] 4.2.7: Write unit tests
- [ ] 4.2.8: Test with sample JSON

**Acceptance Criteria:**
- Can parse simple JSON
- Can parse nested JSON
- Error messages are clear

---

### Task 4.3: Parse OSM JSON
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 4.3.1: Parse buildings array
- [ ] 4.3.2: Parse roads array
- [ ] 4.3.3: Parse parks array
- [ ] 4.3.4: Parse water array
- [ ] 4.3.5: Parse landuse array
- [ ] 4.3.6: Compute bounds
- [ ] 4.3.7: Compute center
- [ ] 4.3.8: Write unit tests

**Acceptance Criteria:**
- Can load sample OSM data
- Bounds correctly computed
- Center correctly computed

---

### Task 4.4: Create Python preprocessing tool
**Priority:** Medium  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 4.4.1: Install osmium library
- [ ] 4.4.2: Read OSM PBF file
- [ ] 4.4.3: Extract buildings with height
- [ ] 4.4.4: Extract roads
- [ ] 4.4.5: Extract features
- [ ] 4.4.6: Convert to JSON
- [ ] 4.4.7: Write unit tests
- [ ] 4.4.8: Test with NewDelhi.osm.pbf

**Acceptance Criteria:**
- Can extract OSM data
- Output is valid JSON
- Handles large files

---

## Phase 5: 2D Rendering (Week 3)

### Task 5.1: Coordinate normalization
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 5.1.1: Create normalize_coordinates() function
- [ ] 5.1.2: Apply to all loaded data
- [ ] 5.1.3: Store offset for reference
- [ ] 5.1.4: Write unit tests

**Acceptance Criteria:**
- All data normalized to center origin
- Offset stored for future use

---

### Task 5.2: Camera class - 2D mode
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 5.2.1: Create src/core/camera.h
- [ ] 5.2.2: Define CameraMode enum
- [ ] 5.2.3: Implement 2D orthographic projection
- [ ] 5.2.4: Implement zoom (0.1x to 20x)
- [ ] 5.2.5: Implement pan
- [ ] 5.2.6: Write unit tests for matrix calculations

**Acceptance Criteria:**
- 2D camera produces correct matrices
- Zoom works as expected

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
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 5.5.1: Triangulate polygons
- [ ] 5.5.2: Create vertex buffer
- [ ] 5.5.3: Create fill shader
- [ ] 5.5.4: Create fill pipeline
- [ ] 5.5.5: Render parks, water, landuse
- [ ] 5.5.6: Test features visible

**Acceptance Criteria:**
- Parks, water, landuse visible
- Correct colors

---

### Task 5.6: Line rendering
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] 5.6.1: Create line vertex buffer
- [ ] 5.6.2: Create line shader
- [ ] 5.6.3: Create line pipeline
- [ ] 5.6.4: Render roads
- [ ] 5.6.5: Test roads visible

**Acceptance Criteria:**
- Roads visible as lines
- Correct colors

---

## Phase 6: 3D Rendering (Week 4)

### Task 6.1: Building extrusion
**Priority:** Critical  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 6.1.1: Define BuildingVertex with normal
- [ ] 6.1.2: Implement extrude_building()
- [ ] 6.1.3: Generate top face with normal (0,1,0)
- [ ] 6.1.4: Generate bottom face with normal (0,-1,0)
- [ ] 6.1.5: Generate side faces with outward normals
- [ ] 6.1.6: Ensure correct winding order
- [ ] 6.1.7: Write unit tests

**Acceptance Criteria:**
- Buildings extruded correctly
- Normals correct
- All faces present

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

### Task 6.6: Depth buffer
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 6.6.1: Create depth image
- [ ] 6.6.2: Allocate depth memory
- [ ] 6.6.3: Create depth image view
- [ ] 6.6.4: Update render pass with depth
- [ ] 6.6.5: Update framebuffers with depth
- [ ] 6.6.6: Test depth testing

**Acceptance Criteria:**
- Depth buffer works
- Correct z-ordering

---

## Phase 7: Interactivity (Week 5)

### Task 7.1: Keyboard input
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 7.1.1: Handle SDL_KEYDOWN events
- [ ] 7.1.2: Map keys to actions
- [ ] 7.1.3: Update camera from keys
- [ ] 7.1.4: Test all keys work

**Acceptance Criteria:**
- All keys work as specified

---

### Task 7.2: Mouse input
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 7.2.1: Handle mouse button events
- [ ] 7.2.2: Handle mouse motion events
- [ ] 7.2.3: Handle mouse wheel events
- [ ] 7.2.4: Implement drag panning
- [ ] 7.2.5: Test mouse controls

**Acceptance Criteria:**
- Mouse drag pans camera
- Scroll wheel zooms

---

### Task 7.3: Mode switching
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 7.3.1: Handle F1/F2 keys
- [ ] 7.3.2: Switch camera mode
- [ ] 7.3.3: Adjust parameters for new mode
- [ ] 7.3.4: Test mode switching

**Acceptance Criteria:**
- F1 switches to 2D
- F2 switches to 3D

---

### Task 7.4: Camera constraints
**Priority:** Medium  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] 7.4.1: Clamp zoom to valid range
- [ ] 7.4.2: Clamp distance to valid range
- [ ] 7.4.3: Clamp tilt to valid range
- [ ] 7.4.4: Test constraints

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
- [ ] 8.2.1: Test JSON parsing
- [ ] 8.2.2: Test building parsing
- [ ] 8.2.3: Test road parsing
- [ ] 8.2.4: Test feature parsing
- [ ] 8.2.5: Test bounds calculation

**Acceptance Criteria:**
- All loader tests pass

---

### Task 8.3: Unit tests for geometry
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 8.3.1: Test building extrusion
- [ ] 8.3.2: Test normal generation
- [ ] 8.3.3: Test polygon triangulation
- [ ] 8.3.4: Test line generation

**Acceptance Criteria:**
- All geometry tests pass

---

### Task 8.4: Integration tests
**Priority:** Medium  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 8.4.1: Test full pipeline (load → build → render)
- [ ] 8.4.2: Test coordinate consistency
- [ ] 8.4.3: Test camera bounds

**Acceptance Criteria:**
- All integration tests pass

---

## Phase 9: Polish (Week 6)

### Task 9.1: Performance optimization
**Priority:** Medium  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] 9.1.1: Profile current performance
- [ ] 9.1.2: Add distance-based culling
- [ ] 9.1.3: Optimize buffer usage
- [ ] 9.1.4: Measure improvements

**Acceptance Criteria:**
- 30+ FPS achieved

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
- [ ] 9.3.2: Add build instructions
- [ ] 9.3.3: Add usage guide
- [ ] 9.3.4: Document keyboard controls
- [ ] 9.3.5: Add code comments

**Acceptance Criteria:**
- Complete documentation

---

### Task 9.4: Final testing
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] 9.4.1: Run all tests
- [ ] 9.4.2: Manual testing
- [ ] 9.4.3: Check for memory leaks
- [ ] 9.4.4: Verify build on clean system

**Acceptance Criteria:**
- All tests pass
- No memory leaks
- Clean build works

---

## Task Dependencies

```
Task 1.1 (Build system) → All other tasks
Task 2.1 (Window) → Task 2.2 (Vulkan instance) → Task 2.3 (Surface)
Task 2.4 (Physical device) → Task 2.5 (Logical device)
Task 2.6 (Swapchain) → Task 2.7 (Render pass) → Task 2.8 (Framebuffers)
Task 2.9 (Command pool) → Task 2.11 (Main loop)
Task 3.1 (Shaders) → Task 3.3 (Pipeline) → Task 3.4 (Draw)
Task 4.1 (Data structures) → Task 4.2 (JSON parser) → Task 4.3 (OSM parser)
Task 5.1 (Normalization) → Task 5.2 (2D camera)
Task 5.3 (Ground shader) → Task 5.4 (Render ground)
Task 6.1 (Extrusion) → Task 6.5 (Render buildings)
Task 6.2 (3D camera) → Task 6.5 (Render buildings)
Task 7.1 (Keyboard) → Task 7.3 (Mode switching)
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
