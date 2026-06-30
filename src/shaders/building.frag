#version 450
// Building fragment shader — flat-shaded with directional lighting.
// Uses world-space position to compute a simple normal for shading.

layout(location = 0) in vec4 fragColor;
layout(location = 1) in float fragDepth;
layout(location = 0) out vec4 outColor;

void main() {
    // Simple ambient + directional lighting
    vec3 lightDir = normalize(vec3(0.4, 0.8, -0.3));
    // Approximate normal from depth gradient (works for flat roofs and vertical walls)
    vec3 normal = normalize(vec3(
        dFdx(fragDepth) * 100.0,
        0.1,
        dFdy(fragDepth) * 100.0
    ));
    // If normal is too flat (top face), use up direction
    if (abs(normal.y) > 0.7) {
        normal = vec3(0.0, 1.0, 0.0);
    }
    float light = max(dot(normal, lightDir), 0.25);
    // Darken sides slightly more than tops
    float side_factor = 0.7 + 0.3 * abs(normal.y);
    vec3 shaded = fragColor.rgb * light * side_factor;
    outColor = vec4(shaded, fragColor.a);
}
