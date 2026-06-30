#version 450
// Fill vertex shader — reads per-vertex position from buffer,
// applies camera projection matrix, passes push-constant fill color.

layout(location = 0) in vec2 inPosition;

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
} camera;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec4 fillColor;  // rgb + opacity
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = camera.proj * vec4(inPosition, 0.0, 1.0);
    fragColor = pc.fillColor;
}
