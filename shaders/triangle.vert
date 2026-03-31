#version 450

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

// Define a full-screen triangle using gl_VertexIndex
// gl_VertexIndex: 0, 1, 2 for the three vertices
const vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),   // Bottom-left
    vec2( 3.0, -1.0),   // Far right (covers right side)
    vec2(-1.0,  3.0)    // Far top (covers top)
);

const vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // Red
    vec3(0.0, 1.0, 0.0),  // Green
    vec3(0.0, 0.0, 1.0)   // Blue
);

void main() {
    // Generate triangle that covers the full screen
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
