#version 450
#extension GL_ARB_separate_shader_objects : enable

// Note: Location "0" is a framebuffer index. That is, "location=0" means framebuffer 0. At this 
// time (11/22/2018), we only have one (??how do we know??).
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragcolor;

void main() {
    outColor = vec4(fragcolor, 1.0f);
}
