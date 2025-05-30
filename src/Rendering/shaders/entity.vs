#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aSize;
layout (location = 2) in float aRotation;
layout (location = 3) in vec4 aTexCoord;
layout (location = 4) in vec4 aColor;

out VS_OUT {
    vec3  Pos;
    vec4  TexCoords;
    vec2  Size;
    float Rotation;
    vec4  Color;
} vs_out;

uniform vec2 texAtlasSize;

void main() {
    vs_out.Pos = aPos;
    vs_out.TexCoords = vec4(aTexCoord.xy / texAtlasSize, aTexCoord.zw / texAtlasSize);
    vs_out.Size = aSize;
    vs_out.Rotation = aRotation;
    vs_out.Color = aColor;
}