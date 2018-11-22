#version 450

vec2 positions[3] = vec2[] (
    vec2(+0.0f, -0.5f),
    vec2(+0.5f, +0.5f),
    vec2(-0.5f, +0.5f)
);

vec3 colors[3] = vec3[] (
    vec3(1.0f, 0.0f, 0.0f),     // r
    vec3(0.0f, 1.0f, 0.0f),     // g
    vec3(0.0f, 0.0f, 1.0f)      // b
);

layout(location = 0) out vec3 fragColor;

void_ main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
