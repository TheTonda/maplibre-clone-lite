#version 450

// 2D fill shader (orthographic).
// Takes a 2D position in local ENU coordinates and transforms it
// through the camera's projection * view matrix.

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

// Push constant for per-draw color
layout(push_constant) uniform PerDraw {
    vec4 color;
} pc;

layout(location = 0) in  vec2 inPosition;
layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition.x, 0.0, inPosition.y, 1.0);
    outColor    = pc.color;
}
