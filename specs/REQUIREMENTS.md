# Requirements Specification
## Interactive 3D Map Renderer

**Version:** 1.0  
**Date:** July 2, 2026  
**Status:** Specification - Ready for Review

---

## 1. Functional Requirements

### 1.1 Data Loading

**FR-1.1: OSM Data Loading**
- **Priority:** High
- **Description:** System shall load OpenStreetMap data from JSON files
- **Input:** JSON file path (data/osm_data.json)
- **Output:** In-memory OSM data structure
- **Acceptance Criteria:**
  - Successfully loads buildings, roads, parks, water, landuse
  - Parses JSON without errors
  - Computes bounds and center
  - Handles missing fields gracefully

**FR-1.2: Coordinate Conversion**
- **Priority:** High
- **Description:** System shall convert Mercator coordinates to normalized meters
- **Input:** Mercator coordinates (0-65536 range)
- **Output:** Meters relative to data center
- **Acceptance Criteria:**
  - All coordinates normalized to (0, 0) center
  - Preserves relative positions
  - Tested with known reference points

**FR-1.3: Style Definition**
- **Priority:** High
- **Description:** System shall load style definitions from JSON
- **Input:** Style JSON file (data/style.json)
- **Output:** Style rules in memory
- **Acceptance Criteria:**
  - Loads colors for buildings, roads, parks, water
  - Supports fill, line, fill-extrusion layer types
  - Provides default colors if style missing

### 1.2 Geometry Generation

**FR-2.1: Building Extrusion**
- **Priority:** High
- **Description:** System shall convert 2D building footprints to 3D boxes
- **Input:** Building footprint (polygon) and height
- **Output:** 3D mesh with proper normals
- **Acceptance Criteria:**
  - Generates top, bottom, and side faces
  - Each face has correct normal vector
  - Winding order correct for backface culling
  - Triangulated for GPU rendering

**FR-2.2: Polygon Triangulation**
- **Priority:** High
- **Description:** System shall triangulate polygons for rendering
- **Input:** Polygon (list of points)
- **Output:** Triangle indices
- **Acceptance Criteria:**
  - Fan triangulation for convex polygons
  - Correct winding order
  - Handles 3+ point polygons

**FR-2.3: Line Geometry**
- **Priority:** Medium
- **Description:** System shall convert road lines to GPU geometry
- **Input:** Road line (list of points)
- **Output:** Line strip vertices
- **Acceptance Criteria:**
  - Preserves order of points
  - Handles disconnected segments

### 1.3 Camera System

**FR-3.1: 2D Camera**
- **Priority:** High
- **Description:** System shall provide orthographic 2D camera
- **Input:** Position (x, y), zoom level
- **Output:** Projection and view matrices
- **Acceptance Criteria:**
  - Zoom range: 0.1x to 20x
  - Pan in normalized meters
  - Smooth transitions

**FR-3.2: 3D Camera**
- **Priority:** High
- **Description:** System shall provide perspective 3D camera
- **Input:** Position, distance, tilt, rotation
- **Output:** Projection and view matrices
- **Acceptance Criteria:**
  - Tilt range: 0° to 85°
  - Distance range: 50m to 5000m
  - Full 360° rotation
  - Camera always looks at data center

**FR-3.3: Camera Mode Switching**
- **Priority:** High
- **Description:** System shall allow switching between 2D and 3D modes
- **Input:** F1 (2D) or F2 (3D) key
- **Output:** Mode change
- **Acceptance Criteria:**
  - Instant switch
  - Position preserved
  - Zoom/distance adjusted appropriately

**FR-3.4: Camera Input Controls**
- **Priority:** High
- **Description:** System shall respond to keyboard and mouse input
- **Controls:**
  - Arrow keys: Pan
  - Mouse drag: Pan
  - +/- or scroll: Zoom
  - Q/E: Tilt
  - A/D: Rotate
- **Acceptance Criteria:**
  - Smooth, responsive controls
  - Pan speed proportional to zoom
  - Zoom clamped to valid range

### 1.4 Rendering

**FR-4.1: 2D Rendering**
- **Priority:** High
- **Description:** System shall render scene in 2D top-down view
- **Output:** Rendered framebuffer
- **Acceptance Criteria:**
  - Renders ground plane
  - Renders parks, water, landuse as filled polygons
  - Renders roads as lines
  - No depth testing
  - Correct z-ordering

**FR-4.2: 3D Rendering**
- **Priority:** High
- **Description:** System shall render scene in 3D perspective view
- **Output:** Rendered framebuffer
- **Acceptance Criteria:**
  - Renders ground plane
  - Renders extruded buildings
  - Renders features on ground
  - Depth testing enabled
  - Per-vertex lighting
  - Correct render order (ground → features → buildings)

**FR-4.3: Building Lighting**
- **Priority:** Medium
- **Description:** System shall apply directional lighting to buildings
- **Input:** Vertex normals
- **Output:** Lit fragment color
- **Acceptance Criteria:**
  - Ambient + diffuse lighting
  - Top faces brighter than sides
  - Consistent across all buildings

**FR-4.4: Ground Grid**
- **Priority:** Low
- **Description:** System shall render grid pattern on ground
- **Output:** Ground with grid lines
- **Acceptance Criteria:**
  - Grid every 50 meters
  - Subtle, non-distracting
  - Anti-aliased lines

### 1.5 Input Handling

**FR-5.1: Keyboard Input**
- **Priority:** High
- **Description:** System shall respond to keyboard events
- **Keys:**
  - ESC: Quit
  - F1: 2D mode
  - F2: 3D mode
  - Arrow keys: Pan
  - Q/E: Tilt
  - A/D: Rotate
  - +/-: Zoom
- **Acceptance Criteria:**
  - All keys work as specified
  - No key repeat issues

**FR-5.2: Mouse Input**
- **Priority:** High
- **Description:** System shall respond to mouse events
- **Actions:**
  - Left drag: Pan
  - Scroll wheel: Zoom
- **Acceptance Criteria:**
  - Smooth panning
  - Zoom proportional to scroll

**FR-5.3: Window Events**
- **Priority:** High
- **Description:** System shall handle window events
- **Events:**
  - Close button: Quit
  - Resize: Update viewport
- **Acceptance Criteria:**
  - Clean shutdown
  - Viewport updates on resize

### 1.6 Performance

**FR-6.1: Frame Rate**
- **Priority:** High
- **Description:** System shall maintain minimum 30 FPS
- **Conditions:** 40,000 buildings, modern GPU
- **Acceptance Criteria:**
  - 30+ FPS on GTX 1060 / RX 580
  - 60+ FPS on RTX 3060 / RX 6700 XT

**FR-6.2: Memory Usage**
- **Priority:** Medium
- **Description:** System shall use less than 500MB GPU memory
- **Acceptance Criteria:**
  - GPU memory < 500MB
  - System memory < 200MB
  - No memory leaks

**FR-6.3: Load Time**
- **Priority:** Medium
- **Description:** System shall load data in under 5 seconds
- **Acceptance Criteria:**
  - Initial load < 5 seconds
  - Subsequent loads < 2 seconds

---

## 2. Non-Functional Requirements

### 2.1 Code Quality

**NFR-1.1: Coding Standards**
- **Priority:** High
- **Description:** Code shall follow C++ Core Guidelines
- **Acceptance Criteria:**
  - No compiler warnings with -Wall -Wextra
  - Consistent formatting
  - Meaningful variable names
  - Functions < 50 lines

**NFR-1.2: Documentation**
- **Priority:** High
- **Description:** All public APIs shall be documented
- **Acceptance Criteria:**
  - Doxygen comments for all public functions
  - README with build instructions
  - Architecture documentation

**NFR-1.3: Testing**
- **Priority:** High
- **Description:** System shall have comprehensive tests
- **Acceptance Criteria:**
  - 80%+ code coverage
  - All public APIs tested
  - Edge cases covered
  - All tests pass

### 2.2 Portability

**NFR-2.1: Cross-Platform**
- **Priority:** Medium
- **Description:** Code shall compile on Linux, Windows, macOS
- **Acceptance Criteria:**
  - Builds on Linux (primary)
  - Builds on Windows (secondary)
  - Builds on macOS (best effort)

**NFR-2.2: Compiler Support**
- **Priority:** High
- **Description:** Code shall compile with modern compilers
- **Acceptance Criteria:**
  - GCC 11+
  - Clang 14+
  - MSVC 2022+

### 2.3 Maintainability

**NFR-3.1: Modular Design**
- **Priority:** High
- **Description:** Code shall be organized in clear modules
- **Acceptance Criteria:**
  - Clear separation of concerns
  - Minimal coupling between modules
  - Easy to understand and modify

**NFR-3.2: Build System**
- **Priority:** High
- **Description:** Build system shall be simple and reproducible
- **Acceptance Criteria:**
  - CMake-based
  - Out-of-source builds
  - Clear error messages
  - Fast incremental builds

### 2.4 Reliability

**NFR-4.1: Error Handling**
- **Priority:** High
- **Description:** System shall handle errors gracefully
- **Acceptance Criteria:**
  - No crashes on invalid input
  - Clear error messages
  - Graceful degradation

**NFR-4.2: Resource Management**
- **Priority:** High
- **Description:** System shall properly manage resources
- **Acceptance Criteria:**
  - No memory leaks (Valgrind clean)
  - Proper Vulkan cleanup
  - RAII for all resources

### 2.5 Usability

**NFR-5.1: Visual Feedback**
- **Priority:** Medium
- **Description:** System shall provide visual feedback
- **Acceptance Criteria:**
  - Smooth animations
  - Clear visual hierarchy
  - Appropriate colors

**NFR-5.2: Performance Feedback**
- **Priority:** Low
- **Description:** System shall show performance metrics
- **Acceptance Criteria:**
  - FPS counter (optional)
  - Camera position display (optional)

---

## 3. Constraints

### 3.1 Technical Constraints

**TC-1:** Must use Vulkan 1.2+ (for modern features)
**TC-2:** Must use C++23 (for modern language features)
**TC-3:** Must use SDL2 (for window management)
**TC-4:** Must use GLM (for math)
**TC-5:** Must use CMake 3.20+ (for build system)
**TC-6:** Must support Linux as primary platform

### 3.2 Resource Constraints

**RC-1:** GPU memory limit: 500MB
**RC-2:** System memory limit: 200MB
**RC-3:** No external runtime dependencies beyond standard libraries
**RC-4:** Build time: < 2 minutes on modern hardware

### 3.3 Schedule Constraints

**SC-1:** Core functionality: 4 weeks
**SC-2:** Polish and optimization: 2 weeks
**SC-3:** Total: 6 weeks for v1.0

---

## 4. Acceptance Criteria

### 4.1 Definition of Done

A feature is "done" when:
- [ ] Code implemented
- [ ] Unit tests written and passing
- [ ] Integration tests passing
- [ ] Manual testing completed
- [ ] Code reviewed
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Committed to repository

### 4.2 Release Criteria

v1.0 is ready for release when:
- [ ] All functional requirements implemented
- [ ] All non-functional requirements met
- [ ] All tests passing (100% pass rate)
- [ ] Performance targets achieved
- [ ] No known critical bugs
- [ ] Documentation complete
- [ ] Build reproducible on clean system

---

## 5. Priority Matrix

| Priority | Requirements |
|----------|-------------|
| **Critical** | FR-1.1, FR-1.2, FR-2.1, FR-3.1, FR-3.2, FR-4.1, FR-4.2, NFR-1.1, NFR-1.3 |
| **High** | FR-1.3, FR-2.2, FR-3.3, FR-3.4, FR-5.1, FR-5.2, FR-5.3, FR-6.1, NFR-2.2, NFR-3.1, NFR-3.2, NFR-4.1, NFR-4.2 |
| **Medium** | FR-2.3, FR-4.3, FR-6.2, FR-6.3, NFR-2.1, NFR-5.1 |
| **Low** | FR-4.4, NFR-5.2 |

---

## 6. Out of Scope (v1.0)

The following are explicitly out of scope for v1.0:
- Real-time data updates
- Server-side rendering
- Advanced lighting (shadows, AO)
- Building interiors
- Routing/navigation
- Offline tile caching
- MVT (Mapbox Vector Tiles) format
- Terrain elevation
- Custom shader support
- Plugin system

These may be considered for future versions.

---

## 7. Success Metrics

The project is successful if:
1. **Correctness:** Renders OSM data accurately in both 2D and 3D
2. **Performance:** Achieves 30+ FPS with full dataset
3. **Quality:** 80%+ test coverage, no critical bugs
4. **Usability:** Intuitive controls, smooth experience
5. **Maintainability:** Clean code, good documentation

---

## 8. Open Questions

1. **Q: Should we support custom styles?**
   A: Not in v1.0, but design for it

2. **Q: What's the minimum supported Vulkan version?**
   A: Vulkan 1.2

3. **Q: Should we implement building shadows?**
   A: Not in v1.0 (defer to v2.0)

4. **Q: How do we handle very large datasets?**
   A: Implement streaming in future version

5. **Q: Should we support MVT format?**
   A: Not in v1.0 (OSM JSON only)
