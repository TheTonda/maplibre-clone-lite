#version 450
// Fill fragment shader — outputs premultiplied color with alpha from push constant.

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
