#version 330 core

in vec2 uv;

out vec4 fragColor;

uniform sampler1DArray wfall;
uniform sampler1D cmap;
uniform float wrapPos;
uniform float histLen;
uniform float wfallHeight;

const float lo = -100.0f;
const float hi = -20.0f;

vec3 waterfall(vec2 pos)
{
    float dt = (1.0 - pos.y) * wfallHeight;
    float t = mod(wrapPos - dt + histLen, histLen);
    float a = texture(wfall, vec2(pos.x, t + 0.5)).r;

    float c = clamp((a - lo) / (hi - lo), 0.0f, 1.0f);

    return texture(cmap, c).rgb;
}

void main()
{
    fragColor = vec4(waterfall(uv), 1.0f);
}
