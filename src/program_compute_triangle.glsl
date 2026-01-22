@cs cs

// NOTE: this compute shader is (fixed) chatgpt slop

// The output image – a 2‑D RGBA8 format (change as needed)
layout(binding = 0, rgba8) uniform writeonly image2D cs_output;

// Vertex positions in Normalised Device Coordinates (NDC)
//   x,y in [-1,1]  ->  screen coordinates will be derived from image size
//   z is unused (fixed at 0.0)
const vec2 vPos[3] = vec2[](
    vec2(-0.5,  0.5),   // Vertex 0
    vec2( 0.5,  0.5),   // Vertex 1
    vec2( 0.0, -0.5)    // Vertex 2
);

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

layout(local_size_x = 16, local_size_y = 16) in;   // tile size – tweak as needed

void main()
{
    // Global pixel coordinates for this invocation
    ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);

    // Image dimensions (must be queried from the image itself)
    ivec2 imgSize = imageSize(cs_output);

    // If we are outside the image bounds, just return
    if(any(lessThan(pixCoord, ivec2(0))) || any(greaterThanEqual(pixCoord, imgSize)))
        return;

    // Convert pixel coordinate to NDC space [-1,1]
    vec2 ndc = (vec2(pixCoord) + 0.5) / vec2(imgSize);   // centre of texel
    ndc = ndc * 2.0 - 1.0;                               // map to [-1,1]

    // Test if the current pixel lies inside the triangle
    if(insideTriangle(ndc, vPos[0], vPos[1], vPos[2]))
    {
        // Triangle colour (RGBA)
        const vec4 u_TriColour = vec4(1.0, 0.4, 0.6, 1.0);   // pinkish
        imageStore(cs_output, pixCoord, u_TriColour);
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