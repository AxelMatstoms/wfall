#version 330 core

in vec2 uv;

out vec4 fragColor;

uniform sampler1DArray wfall;
uniform float wrapPos;
uniform float width;

const float lo = -100.0f;
const float hi = -20.0;

float getY(float x) {
    float dB = texture(wfall, vec2(x, wrapPos)).r;
    float scaled = (dB - lo) / (hi - lo);

    return scaled;
}

vec3 line(vec2 pos)
{
    float dx = 1.0 / width;
    float y0 = getY(pos.x);
    float y1 = getY(pos.x + dx);

    float vline = step(min(y0, y1), pos.y) * step(pos.y, max(y0, y1));
    float hline = step(abs(y0 - pos.y), 0.005);

    return vec3(0.7f) * max(vline, hline);
}

vec3 gray(vec2 pos)
{
    float y = getY(pos.x);
    
    if (pos.y > y) {
        return vec3(0.05f);
    } else {
        return vec3(0.15f);
    }
}

void main()
{
    vec3 color = max(gray(uv), line(uv));
    fragColor = vec4(color, 1.0f);
}
