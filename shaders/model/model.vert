#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Push constant
layout(push_constant) uniform PushConstants {
    float angle;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    float c = cos(pc.angle);
    float s = sin(pc.angle);

    // Rotation around Z axis
    mat4 rotation = mat4(
        vec4( c,  s, 0.0, 0.0),
        vec4(-s,  c, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );

    vec4 rotatedPos = rotation * vec4(inPosition, 1.0);

    gl_Position = ubo.proj * ubo.view * ubo.model * rotatedPos;
    fragColor = inColor;
}
