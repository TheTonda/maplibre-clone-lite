#include "map_renderer/renderer.h"

#include <cstddef>
#include <string>

#include <glad/glad.h>

// glad/glad.h declares the GL function-pointer macros (e.g.
// `#define glGenVertexArrays glad_glGenVertexArrays`) so that calling code
// using the global glad function table can write `glGenVertexArrays(...)`.
// The Renderer routes GL calls through the GLFunctions struct whose members
// are named `glGenVertexArrays`, etc. — the glad macros would rewrite those
// member names into `glad_glGenVertexArrays` and break compilation. We only
// need glad for the integer GL_* constants, so undefine every function-name
// macro that collides with a GLFunctions member.
#undef glGenVertexArrays
#undef glDeleteVertexArrays
#undef glBindVertexArray
#undef glGenBuffers
#undef glDeleteBuffers
#undef glBindBuffer
#undef glBufferData
#undef glEnableVertexAttribArray
#undef glVertexAttribPointer
#undef glDrawArrays
#undef glUseProgram
#undef glUniformMatrix4fv
#undef glUniform4f
#undef glUniform2f
#undef glGetUniformLocation
#undef glClearColor
#undef glClear
#undef glViewport
#undef glCreateShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glDeleteShader
#undef glCreateProgram
#undef glAttachShader
#undef glLinkProgram
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glDeleteProgram
#undef glEnable
#undef glDisable
#undef glGetError

#include "map_renderer/color_table.h"
#include "map_renderer/camera.h"
#include "map_renderer/debug_log.h"
#include "map_renderer/geometry_builder.h"
#include "map_renderer/gl_check.h"
#include "shaders/fill_frag.h"
#include "shaders/fill_vert.h"

namespace map_renderer {

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

bool Renderer::initialize(PlatformInterface& platform) {
    platform_ = &platform;
    gl_ = &platform.get_gl_functions();

    if (!compile_shaders()) {
        return false;
    }

    uniform_proj_ = gl_->glGetUniformLocation(shader_program_, "u_proj");
    uniform_view_ = gl_->glGetUniformLocation(shader_program_, "u_view");
    uniform_color_ = gl_->glGetUniformLocation(shader_program_, "u_color");
    uniform_tile_offset_ =
        gl_->glGetUniformLocation(shader_program_, "u_tile_offset");
    GL_CHECK(*gl_);

    return true;
}

bool Renderer::compile_shaders() {
    // Desktop GLSL prefix. The embedded shader sources omit the #version
    // directive so the app can prepend the right one per platform
    // (LLD §6.2). Desktop targets GL 3.3 core.
    static const char* version_prefix = "#version 330 core\n";

    const char* vert_sources[2] = {version_prefix, shader_source::fill_vertex};
    const int32_t vert_lengths[2] = {
        static_cast<int32_t>(std::char_traits<char>::length(version_prefix)),
        static_cast<int32_t>(std::char_traits<char>::length(
            shader_source::fill_vertex))};

    uint32_t vert = gl_->glCreateShader(GL_VERTEX_SHADER);
    GL_CHECK(*gl_);
    gl_->glShaderSource(vert, 2, vert_sources, vert_lengths);
    gl_->glCompileShader(vert);
    GL_CHECK(*gl_);

    int32_t vert_status = GL_FALSE;
    gl_->glGetShaderiv(vert, GL_COMPILE_STATUS, &vert_status);
    if (vert_status == GL_FALSE) {
        char log[1024];
        int32_t log_len = 0;
        gl_->glGetShaderInfoLog(vert, static_cast<int32_t>(sizeof(log)),
                                &log_len, log);
        DEBUG_LOG("Vertex shader compile failed: %.*s", log_len, log);
        gl_->glDeleteShader(vert);
        return false;
    }

    const char* frag_sources[2] = {version_prefix, shader_source::fill_fragment};
    const int32_t frag_lengths[2] = {
        static_cast<int32_t>(std::char_traits<char>::length(version_prefix)),
        static_cast<int32_t>(std::char_traits<char>::length(
            shader_source::fill_fragment))};

    uint32_t frag = gl_->glCreateShader(GL_FRAGMENT_SHADER);
    GL_CHECK(*gl_);
    gl_->glShaderSource(frag, 2, frag_sources, frag_lengths);
    gl_->glCompileShader(frag);
    GL_CHECK(*gl_);

    int32_t frag_status = GL_FALSE;
    gl_->glGetShaderiv(frag, GL_COMPILE_STATUS, &frag_status);
    if (frag_status == GL_FALSE) {
        char log[1024];
        int32_t log_len = 0;
        gl_->glGetShaderInfoLog(frag, static_cast<int32_t>(sizeof(log)),
                                &log_len, log);
        DEBUG_LOG("Fragment shader compile failed: %.*s", log_len, log);
        gl_->glDeleteShader(vert);
        gl_->glDeleteShader(frag);
        return false;
    }

    uint32_t program = gl_->glCreateProgram();
    GL_CHECK(*gl_);
    gl_->glAttachShader(program, vert);
    gl_->glAttachShader(program, frag);
    gl_->glLinkProgram(program);
    GL_CHECK(*gl_);

    int32_t link_status = GL_FALSE;
    gl_->glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        char log[1024];
        int32_t log_len = 0;
        gl_->glGetProgramInfoLog(program, static_cast<int32_t>(sizeof(log)),
                                 &log_len, log);
        DEBUG_LOG("Program link failed: %.*s", log_len, log);
        gl_->glDeleteShader(vert);
        gl_->glDeleteShader(frag);
        gl_->glDeleteProgram(program);
        return false;
    }

    // Shader objects are no longer needed after linking.
    gl_->glDeleteShader(vert);
    gl_->glDeleteShader(frag);
    GL_CHECK(*gl_);

    shader_program_ = program;
    return true;
}

void Renderer::upload_tile_geometry(TileData& tile) {
    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    gl_->glGenVertexArrays(1, &tile.vao);
    gl_->glBindVertexArray(tile.vao);
    GL_CHECK(*gl_);

    gl_->glGenBuffers(1, &tile.vbo);
    gl_->glBindBuffer(GL_ARRAY_BUFFER, tile.vbo);
    GL_CHECK(*gl_);

    gl_->glBufferData(GL_ARRAY_BUFFER,
                      static_cast<intptr_t>(geom.vertices.size() * sizeof(float)),
                      geom.vertices.data(), GL_STATIC_DRAW);
    GL_CHECK(*gl_);

    gl_->glEnableVertexAttribArray(0);
    gl_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                               static_cast<int32_t>(2 * sizeof(float)),
                               nullptr);
    GL_CHECK(*gl_);

    tile.water_range = geom.water;
    tile.park_range = geom.park;
    tile.landuse_range = geom.landuse;
    tile.road_range = geom.road;
    tile.building_range = geom.building;
}

void Renderer::on_tile_loaded(const TileId& /*id*/, TileData& tile) {
    upload_tile_geometry(tile);
    tile.uploaded = true;
}

void Renderer::on_tile_evicted(const TileId& /*id*/, TileData& tile) {
    if (tile.vbo != 0) {
        gl_->glDeleteBuffers(1, &tile.vbo);
        tile.vbo = 0;
    }
    if (tile.vao != 0) {
        gl_->glDeleteVertexArrays(1, &tile.vao);
        tile.vao = 0;
    }
}

void Renderer::cleanup() {
    if (shader_program_ != 0) {
        gl_->glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
}

void Renderer::render(const Camera& camera, TileCache& cache,
                      const std::vector<TileId>& visible_tiles) {
    int vp_w = platform_->get_viewport_width();
    int vp_h = platform_->get_viewport_height();
    float aspect = float(vp_w) / float(vp_h);

    gl_->glViewport(0, 0, vp_w, vp_h);

    // Clear with ground color
    glm::vec4 ground = get_color("ground");
    gl_->glClearColor(ground.r, ground.g, ground.b, ground.a);
    gl_->glClear(GL_COLOR_BUFFER_BIT);

    // Get camera matrices (only recomputed if dirty)
    glm::mat4 proj = camera.get_projection_matrix(aspect);
    glm::mat4 view = camera.get_view_matrix();

    // Bind shader
    gl_->glUseProgram(shader_program_);
    gl_->glUniformMatrix4fv(uniform_proj_, 1, GL_FALSE, &proj[0][0]);
    gl_->glUniformMatrix4fv(uniform_view_, 1, GL_FALSE, &view[0][0]);

    // Draw each ready tile in draw order.
    // visible_tiles is provided by Engine (computed only when camera dirty)
    // — no per-frame allocation here.
    for (const TileId& tid : visible_tiles) {
        auto tile = cache.get(tid);
        if (!tile) continue;  // not loaded yet — skip (stale LOD or blank)

        // Set tile offset uniform
        gl_->glUniform2f(uniform_tile_offset_,
                         tile->world_offset_x, tile->world_offset_z);

        // Bind tile VAO (tile->vao is already uint32_t, matching GLFunctions)
        gl_->glBindVertexArray(tile->vao);

        // Draw in order: water -> parks -> landuse -> roads -> buildings.
        auto draw_range = [&](const TileData::DrawRange& r,
                              const glm::vec4& color) {
            if (r.count == 0) return;
            gl_->glUniform4f(uniform_color_, color.r, color.g, color.b, color.a);
            gl_->glDrawArrays(GL_TRIANGLES,
                              static_cast<int32_t>(r.offset),
                              static_cast<int32_t>(r.count));
        };

        draw_range(tile->water_range, get_color("water"));
        draw_range(tile->park_range, get_color("park"));
        draw_range(tile->landuse_range, get_color("landuse"));
        // v2.0: all roads use the generic "road" color. road_primary and
        // road_secondary colors are defined in the table for future per-type
        // rendering (requires splitting road_range by type in GeometryBuilder).
        draw_range(tile->road_range, get_color("road"));
        draw_range(tile->building_range, get_color("building"));
    }

    GL_CHECK(*gl_);
}

void Renderer::draw_tile(const TileData& /*tile*/, const glm::mat4& /*proj*/,
                         const glm::mat4& /*view*/) {
    // STUB — implemented in Task 11.
}

glm::vec4 Renderer::get_color(const std::string& feature_type) const {
    Color c = map_renderer::get_color(feature_type);
    return glm::vec4(c.r, c.g, c.b, c.a);
}

} // namespace map_renderer
