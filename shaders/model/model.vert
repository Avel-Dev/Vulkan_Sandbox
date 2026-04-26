#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

// Push constant
layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = pc.proj * pc.view * ubo.model * vec4(inPosition, 1.0f);
    fragColor = inColor;
}
