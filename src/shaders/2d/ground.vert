#version 450

// Ground plane vertex shader (2D mode).

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
} camera;

layout(location = 0) in  vec2 inPosition;
layout(location = 0) out vec3 fragWorldPos;

void main() {
    fragWorldPos = vec3(inPosition.x, 0.0, inPosition.y);
    gl_Position  = camera.proj * camera.view * vec4(inPosition.x, 0.0, inPosition.y, 1.0);
}
