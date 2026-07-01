#version 450

// 3D building fragment shader with simple directional lighting.

layout(location = 0) in  vec3 fragNormal;
layout(location = 1) in  vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(fragNormal);
    // Directional light from upper-right-front
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(n, light_dir), 0.0);
    float ambient = 0.3;
    float intensity = ambient + (1.0 - ambient) * diffuse;
    outColor = vec4(fragColor.rgb * intensity, fragColor.a);
}
