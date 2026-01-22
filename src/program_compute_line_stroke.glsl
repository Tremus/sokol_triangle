@cs cs

// NOTE: this compute shader is (fixed) chatgpt slop

// The output image – a 2‑D RGBA8 format (change as needed)
layout(binding = 0, rgba8) uniform writeonly image2D cs_output;

struct buffer_item {
    float y;
};

layout(binding=1) readonly buffer cs_storage_buffer {
    buffer_item sine_buffer[];
};

layout(binding=2) uniform cs_uniforms {
    int   u_buffer_length;
    float u_stroke_width;
};


bool insideTriangle(vec2 p, vec2 a, vec2 b, vec2 c)
{
    // Compute vectors        
    vec2 v0 = c - a;
    vec2 v1 = b - a;
    vec2 v2 = p - a;

    // Compute dot products
    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);

    // Compute barycentric coordinates
    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    // Check if point is in triangle
    return (u >= 0.0) && (v >= 0.0) && (u + v <= 1.0);
}

float sdSegment(in vec2 p, in vec2 a, in vec2 b)
{
    vec2  ba = b - a;
    vec2  pa = p - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

layout(local_size_x = 64, local_size_y = 1) in;   // tile size – tweak as needed

void main()
{
    ivec2 imgSize = imageSize(cs_output);
    ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 view_size = vec2(imgSize);

    int idx      = min(pixCoord.x, u_buffer_length - 1);
    int idx_prev = max(pixCoord.x - 1, 0);
    int idx_next = min(pixCoord.x + 1, u_buffer_length - 1);

    float y_scale     = -0.8;
    float sine_y      = sine_buffer[idx].y      * y_scale;
    float sine_y_prev = sine_buffer[idx_prev].y * y_scale;
    float sine_y_next = sine_buffer[idx_next].y * y_scale;

    float y_top = max(max(sine_y, sine_y_prev), sine_y_next) ;
    float y_bot = min(min(sine_y, sine_y_prev), sine_y_next);
    // Normalise
    y_top = y_top * 0.5 + 0.5;
    y_bot = y_bot * 0.5 + 0.5;
    y_top = ceil(y_top * view_size.y + u_stroke_width * 0.5);
    y_bot = floor(y_bot * view_size.y - u_stroke_width * 0.5);

    int y_start = int(y_bot);
    int y_end   = int(y_top);

    float px_inc       = 2.0 / view_size.x;
    float stroke_width = u_stroke_width / view_size.y * 2;
    for (int i = y_start; i < y_end + 1; i++)
    {
        vec2 p = vec2(pixCoord.x, i);

        p = (p + p) / view_size - vec2(1);

        vec2 a = vec2(p.x - px_inc, sine_y_prev);
        vec2 b = vec2(p.x         , sine_y);
        vec2 c = vec2(p.x + px_inc, sine_y_next);

        float d1 = sdSegment(p, a, b);
        float d2 = sdSegment(p, b, c);
        float d = min(d1, d2);

        float shape_vertical   = smoothstep(stroke_width, 0, abs(d));
        float shape_horizontal = smoothstep(stroke_width, 0, abs(sine_y - p.y));
        float shape            = max(shape_vertical, shape_horizontal);

        vec3 col_bg   = vec3(0);
        vec3 col_line = vec3(1.0);
        vec3 col = mix(col_bg, col_line, shape);

        imageStore(cs_output, ivec2(pixCoord.x, i), vec4(col, 1.0));
    }
}
@end

// ---------------------------------------------------------------------------

// a regular vertex/fragment shader pair to display the result
@vs vs
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end

@fs fs
layout(binding=0) uniform texture2D disp_tex;
layout(binding=0) uniform sampler disp_smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(disp_tex, disp_smp), uv).xyz, 1);
}
@end

@program compute cs
@program display vs fs