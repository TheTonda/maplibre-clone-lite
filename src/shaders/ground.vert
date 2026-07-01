#version 450
// Ground vertex shader — renders a large flat plane at Y=0
// Reads per-vertex position (x, y, z), applies camera matrix, outputs world position.

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(location = 0) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = vec4(inPosition, 1.0);
    vec4 viewPos = camera.view * worldPos;
    vec4 clipPos = camera.proj * viewPos;
    gl_Position = clipPos;
    fragWorldPos = inPosition;
}
