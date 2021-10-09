#version 330 core

in vec2 uv;

out vec4 fragColor;

vec3 waterfall(vec2 pos)
{
    return vec3(0.0, pos);
    //return vec3(0.0f, 1.0f, 0.0f);
}

void main()
{
    fragColor = vec4(waterfall(uv), 1.0f);
}
