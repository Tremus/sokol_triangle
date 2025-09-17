@vs vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = texcoord0;
}
@end

@fs fs
layout(binding=0)uniform texture2D tex;
layout(binding=0)uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

layout(binding=0)uniform fs_uniforms {
    vec2 texSize;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange() {
    vec2 unitRange = vec2(2) / texSize;
    vec2 screenTexSize = vec2(1) / fwidth(uv);
    return max(0.5 * dot(unitRange, screenTexSize), 1);
} 

void main() {
    vec3 msd = texture(sampler2D(tex, smp), uv).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0, 1);
    frag_color = vec4(opacity);

    // Add colour
    frag_color.rg *= uv;
}
@end

@program msdf vs fs