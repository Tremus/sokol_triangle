@vs vs

// Tightly packed rectangle
struct myvertex
{
    vec2 topleft;
    vec2 bottomright;
    uint type;
    uint colour1;
    float stroke_width;
    // TODO
    // uint  texid;
};

layout(binding=0) readonly buffer ssbo {
    myvertex vtx[];
};

layout(binding=0) uniform vs_params {
    vec2 size;
};

out vec2 uv;
out flat vec4 colour1;
out flat uint type;
out flat float feather;
out flat float stroke_width;

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,
    myvertex vert = vtx[v_idx];

    vec2 pos = vert.topleft;
    uv       = vec2(-1);

    uint is_odd = gl_VertexIndex & 1;
    if (is_odd == 1)
    {
        pos.x = vert.bottomright.x;
        uv.x = -uv.x;
    }
    if (i_idx >= 2 && i_idx <= 4) // idx 2/3/4
    {
        pos.y = vert.bottomright.y;
        uv.y = -uv.y;
    }

    pos = (pos + pos) / size - vec2(1);
    pos.y = -pos.y;

    float max_dimension = max(vert.bottomright.x - vert.topleft.x, vert.bottomright.y - vert.topleft.y);

    gl_Position = vec4(pos, 1, 1);
    colour1 = unpackUnorm4x8(vert.colour1).abgr; // swizzle
    type = vtx[v_idx].type;
    feather = 4.0 / max_dimension;
    stroke_width = 2 * vert.stroke_width / max_dimension;
}
@end

@fs fs
in vec2 uv;
in flat vec4 colour1;
in flat uint type;
in flat float feather;
in flat float stroke_width;

out vec4 frag_color;

#define SDF_TYPE_CIRCLE_FILL   0
#define SDF_TYPE_CIRCLE_STROKE 1

void main()
{
    vec4 col = vec4(colour1);

    if (type == SDF_TYPE_CIRCLE_FILL)
    {
        float len = 1 - length(uv);
        float circle_fill = smoothstep(0, feather, len);
        col.rgb *= vec3(circle_fill);
    }
    else if (type == SDF_TYPE_CIRCLE_STROKE)
    {
        float len = 1 - length(uv);

        float circle_fill   = smoothstep(0, feather, len);
        float circle_stroke = smoothstep(stroke_width + feather, stroke_width, len);
        col.rgb *= vec3(circle_fill * circle_stroke);
    }
    // TODO more shapes

    frag_color = col;
}
@end

@program vertexpull vs fs