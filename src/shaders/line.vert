#version 450
// Line vertex shader — reads per-vertex position from buffer,
// applies camera projection matrix, passes push-constant color.

layout(location = 0) in vec2 inPosition;

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
} camera;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec3 color;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = camera.proj * vec4(inPosition, 0.0, 1.0);
    fragColor = pc.color;
}
