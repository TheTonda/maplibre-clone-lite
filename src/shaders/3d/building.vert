#version 450

// 3D building vertex shader.
// Takes position + normal, applies MVP, passes world-space normal to fragment.

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(push_constant) uniform PushConstants {
    vec4 color;
} pc;

layout(location = 0) in  vec3 inPosition;
layout(location = 1) in  vec3 inNormal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 fragColor;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
    // Transform normal to view space (no non-uniform scale expected)
    fragNormal = mat3(camera.view) * inNormal;
    fragColor  = pc.color;
}
