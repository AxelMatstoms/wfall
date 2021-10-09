#version 330 core

in vec2 uv;

out vec4 fragColor;

uniform vec2 color;

vec3 spectrum(vec2 pos)
{
    return vec3(color, pos.x);
}

void main()
{
    fragColor = vec4(spectrum(uv), 1.0f);
}
