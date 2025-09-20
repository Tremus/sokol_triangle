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
out flat vec2 uv_xy_scale;
out flat vec4 colour1;
out flat uint type;
out flat float feather;
out flat float stroke_width;

void main() {
    uint v_idx = gl_VertexIndex / 6u;
    uint i_idx = gl_VertexIndex - v_idx * 6;

    myvertex vert = vtx[v_idx];

    //  0.5f,  0.5f,
    // -0.5f, -0.5f,
    //  0.5f, -0.5f,
    // -0.5f,  0.5f,
    // 0, 1, 2,
    // 1, 2, 3,

    // Is odd
    bool is_right = (gl_VertexIndex & 1) == 1;
    bool is_bottom = i_idx >= 2 && i_idx <= 4;

    vec2 pos = vec2(
        is_right  ? vert.bottomright.x : vert.topleft.x,
        is_bottom ? vert.bottomright.y : vert.topleft.y
    );

    pos = (pos + pos) / size - vec2(1);
    pos.y = -pos.y;

    float vw = vert.bottomright.x - vert.topleft.x;
    float vh = vert.bottomright.y - vert.topleft.y;

    gl_Position = vec4(pos, 1, 1);

    uv = vec2(is_right  ? 1 : -1, is_bottom ? -1 : 1);
    uv_xy_scale = vec2(vw > vh ? vw / vh : 1, vh > vw ? vh / vw : 1);
    colour1 = unpackUnorm4x8(vert.colour1).abgr; // swizzle
    type = vtx[v_idx].type;
    feather = 16.0 / max(size.x, size.y);
    stroke_width = 2 * vert.stroke_width / max(vw, vh);
}
@end

@fs fs
in vec2 uv;
in flat vec2 uv_xy_scale;
in flat vec4 colour1;
in flat uint type;
in flat float feather;
in flat float stroke_width;

out vec4 frag_color;

#define SDF_TYPE_RECTANGLE_FILL   0
#define SDF_TYPE_RECTANGLE_STROKE 1
#define SDF_TYPE_CIRCLE_FILL      2
#define SDF_TYPE_CIRCLE_STROKE    3

float sdRoundBox( in vec2 p, in vec2 b, in vec4 r ) 
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

void main()
{
    vec4 col = colour1;

    float shape = 1;
    if (type == SDF_TYPE_RECTANGLE_FILL)
    {
        vec2 b = uv_xy_scale;
        vec4 r = vec4(0.5);
        float d = sdRoundBox(uv * uv_xy_scale, b, r);

        shape = smoothstep(0, feather, abs(d));
        shape = d > 0 ? 0 : shape;
    }
    else if (type == SDF_TYPE_RECTANGLE_STROKE)
    {
        vec2 b = uv_xy_scale;
        vec4 r = vec4(1);
        float d = sdRoundBox(uv * uv_xy_scale, b - stroke_width, r);
        float rect_stroke = smoothstep(stroke_width + feather, stroke_width, abs(d));
        shape = rect_stroke;
    }
    else if (type == SDF_TYPE_CIRCLE_FILL)
    {
        float len = 1 - length(uv);
        float circle_fill = smoothstep(0, feather, len);
        shape = circle_fill;
    }
    else if (type == SDF_TYPE_CIRCLE_STROKE)
    {
        float len = 1 - length(uv);

        float circle_fill   = smoothstep(0, feather, len);
        float circle_stroke = smoothstep(stroke_width + feather, stroke_width, len);
        shape = circle_fill * circle_stroke;
    }
    col.rgb *= vec3(shape);

    frag_color = col;
}
@end

@program vertexpull vs fs