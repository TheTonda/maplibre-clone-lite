#version 450

// Road vertex shader — 3D positions, rendered in 2D with push-constant color.

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(push_constant) uniform PushConstants {
    vec4 color;
} pc;

layout(location = 0) in  vec3 inPosition;
layout(location = 0) out vec4 vColor;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
    vColor = pc.color;
}
