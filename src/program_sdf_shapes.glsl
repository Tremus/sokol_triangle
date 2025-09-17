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
in vec2 uv;
out vec4 frag_color;

layout(binding=0)uniform fs_uniforms_sdf_shapes {
    float feather;
};

void main() {
    float len = 1 - length(uv);

    // Fill
    vec3 col = vec3(smoothstep(0, feather, len));

    // Stroke
    float thickness = 0.02;
    col *= vec3(smoothstep(thickness, thickness - feather, len));

    frag_color = vec4(col, 1);
}
@end

@program sdf_sahpes vs fs