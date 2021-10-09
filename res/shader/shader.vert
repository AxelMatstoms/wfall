#version 330 core
layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;

out vec2 uv;

uniform mat3 transform;

void main()
{
    uv = inUv;
    vec3 pos = transform * vec3(inPos, 1.0);
    gl_Position = vec4(pos.xy, 0.0, 1.0);
}
