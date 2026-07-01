#version 450
// Line vertex shader — renders 2D lines (roads) on the ground plane

layout(location = 0) in vec2 inPosition;  // x, z in world space

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec4 lineColor;  // rgba
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    // Position at Y=0.1 to avoid z-fighting with ground plane
    vec3 worldPos = vec3(inPosition.x, 0.1, inPosition.y);
    vec4 viewPos = camera.view * vec4(worldPos, 1.0);
    vec4 clipPos = camera.proj * viewPos;
    gl_Position = clipPos;
    fragColor = pc.lineColor;
}
