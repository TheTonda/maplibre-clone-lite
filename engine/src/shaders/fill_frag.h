#pragma once

namespace shader_source {
inline const char* fill_fragment = R"(
uniform vec4 u_color;
out vec4 frag_color;

void main() {
    frag_color = u_color;
}
)";
} // namespace shader_source
