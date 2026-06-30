#version 450

vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
} camera;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec3 color;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = camera.proj * vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = pc.color;
}
