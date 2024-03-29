#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec2 aVelocity;

out vec2 TexCoord;
out vec2 Velocity;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    Velocity = aVelocity;
}