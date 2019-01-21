#version 450

//??what benefit does this have??
#extension GL_ARB_separate_shader_objects : enable

// Note: If there are descriptors that vary per object (ex: model->world transforms) and 
// descriptors that don't (ex: world->camera and camera->window), you can put them into separate 
// descriptor sets so that we don't have to rebind the constant ones on every frame. But we aren't 
// doing that here (and I don't know how yet).
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0f);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
