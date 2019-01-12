//#version 450
//#extension GL_ARB_separate_shader_objects : enable
//
//vec2 positions[3] = vec2[] (
//    vec2(-0.7f, -0.7f),
//    vec2(-0.2f, +0.2f),
//    vec2(+0.7f, +0.7f)
//);
//
//vec3 colors[3] = vec3[] (
//    vec3(1.0f, 0.0f, 0.0f),
//    vec3(0.0f, 1.0f, 0.0f),
//    vec3(0.0f, 0.0f, 1.0f)
//);
//
//layout(location = 0) out vec3 fragColor;
//
//void main() {
//    gl_Position = vec4(positions[gl_VertexIndex], 0.0f, 1.0f);
//    fragColor = colors[gl_VertexIndex];
//}
//

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

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0f, 1.0f);
    //gl_Position = ubo.proj * ubo.view * vec4(inPosition, 0.0f, 1.0f);   // no rotation
    //gl_Position = ubo.proj * vec4(inPosition, 0.0f, 1.0f);  // blank ??why??
    //gl_Position = vec4(inPosition, 0.0f, 1.0f);     // blank ??why??

    fragColor = inColor;
    fragTexCoord = inTexCoord;

    //gl_Position = vec4(0.5f, 0.5f, 0.0f, 1.0f);
    //fragColor = vec3(1.0f, 1.0f, 1.0f);
}
