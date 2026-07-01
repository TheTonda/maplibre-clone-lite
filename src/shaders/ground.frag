#version 450
// Ground fragment shader — simple colored plane with grid pattern
// Uses world position to draw a subtle grid for spatial reference.

layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

void main() {
    // Base ground color (dark gray)
    vec3 baseColor = vec3(0.15, 0.15, 0.17);

    // Grid lines every 50 meters
    float gridSize = 50.0;
    vec2 grid = abs(fract(fragWorldPos.xz / gridSize - 0.5) - 0.5) / fwidth(fragWorldPos.xz / gridSize);
    float line = min(grid.x, grid.y);
    float gridIntensity = 1.0 - min(line, 1.0);

    // Mix base color with slightly lighter grid lines
    vec3 gridColor = vec3(0.25, 0.25, 0.28);
    vec3 finalColor = mix(baseColor, gridColor, gridIntensity * 0.5);

    outColor = vec4(finalColor, 1.0);
}
