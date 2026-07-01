#version 450
// Building fragment shader — per-vertex normal lighting with ambient + directional.

layout(location = 0) in vec4 fragColor;
layout(location = 1) in float fragDepth;
layout(location = 2) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

void main() {
    // Normalize the interpolated normal
    vec3 normal = normalize(fragNormal);

    // Directional light from above and slightly to the side
    vec3 lightDir = normalize(vec3(0.3, 0.85, 0.4));

    // Ambient + diffuse lighting
    float ambient = 0.35;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float light = ambient + diffuse * 0.65;

    // Top faces get a slight brightness boost
    float top_factor = 0.85 + 0.15 * max(normal.y, 0.0);

    vec3 shaded = fragColor.rgb * light * top_factor;
    outColor = vec4(shaded, fragColor.a);
}
