#version 450
#extension GL_ARB_separate_shader_objects : enable

// Note: Only the location of the "out" from the previous shader stage and "in" in this 
// shader stage have to match. The name does not, unlike my prior experiences in Vulkan.
layout(location = 0) in vec3 fragColorGoober;

// Note: Location "0" is a framebuffer index. That is, "location=0" means framebuffer 0. At this 
// time (11/22/2018), we only have one (??how do we know??).
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColorGoober, 1.0f);
}
