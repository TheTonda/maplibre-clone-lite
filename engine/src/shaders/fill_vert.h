// Shared between desktop (#version 330 core) and Android (#version 300 es)
// Desktop app prepends "#version 330 core\n"
// Android app prepends "#version 300 es\nprecision highp float;\n"
#pragma once

namespace shader_source {
inline const char* fill_vertex = R"(
layout(location = 0) in vec2 a_position;    // local ENU (x, z) relative to tile center

uniform mat4 u_proj;
uniform mat4 u_view;
uniform vec2 u_tile_offset;  // world ENU offset of this tile's center

void main() {
    vec2 world_pos = a_position + u_tile_offset;
    gl_Position = u_proj * u_view * vec4(world_pos, 0.0, 1.0);
}
)";
} // namespace shader_source
