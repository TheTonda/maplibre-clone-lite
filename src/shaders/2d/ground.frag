#version 450

// Ground plane fragment shader (2D mode).
// Dark base colour with subtle grid lines every 50 metres.

layout(location = 0) in  vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    const float GRID = 50.0;
    vec3 dark = vec3(0.10, 0.10, 0.12);  // near-black
    vec3 grid = vec3(0.18, 0.18, 0.20);  // slightly lighter

    // Compute grid lines using fwidth for anti-aliasing
    vec2 pos = fragWorldPos.xz;
    vec2 fw  = fwidth(pos);
    vec2 g   = abs(fract(pos / GRID - 0.5) - 0.5) / fw;
    float line = min(g.x, g.y);
    float grid_alpha = 1.0 - smoothstep(0.0, 0.5, line);

    vec3 color = mix(dark, grid, grid_alpha);
    outColor = vec4(color, 1.0);
}
