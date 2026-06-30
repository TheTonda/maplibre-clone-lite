#version 450
// Building vertex shader — 3D extruded buildings
// Reads per-vertex position (x, y, z), applies camera matrix, outputs depth.

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec4 buildColor;  // rgba
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float fragDepth;

void main() {
    vec4 worldPos = camera.proj * camera.view * vec4(inPosition, 1.0);
    gl_Position = worldPos;
    fragDepth = worldPos.z / worldPos.w;
    fragColor = pc.buildColor;
}
