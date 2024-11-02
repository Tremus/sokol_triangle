@vs offscreen_vs
in vec4 position;
in vec4 colour0;

out vec4 colour;

void main() {
    gl_Position = position;
    colour = colour0;
}
@end

@fs offscreen_fs
in vec4 colour;
out vec4 frag_colour;

void main() {
    frag_colour = colour;
}
@end

@program offscreen offscreen_vs offscreen_fs

/* texquad vertex shader */
@vs display_vs
in vec4 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = position;
    uv = vec2(texcoord0.x, -texcoord0.y);
}
@end

/* texquad fragment shader */
@fs display_fs
in vec2 uv;
out vec4 frag_colour;
uniform texture2D tex;
uniform sampler smp;
uniform fs_blur {
    vec2 u_resolution;
};

// Shader from here:
// https://www.youtube.com/watch?v=5xUT5QdkPAU
// https://github.com/SuboptimalEng/shader-tutorials/blob/main/09-gaussian-blur/shader.frag#L19

void main() {
    vec2 texelSize = 1.0 / u_resolution;

    // ------ Box blur ------

    // const float kernelSize = 0.0;
    // const float kernelSize = 1.0;
    // const float kernelSize = 2.0;
    // const float kernelSize = 3.0;
    // const float kernelSize = 4.0;
    const float kernelSize = 5.0;
    vec3 boxBlurColour = vec3(0.0);
    // note: if kernelSize == 1.0, then boxBlurDivisor == 9.0
    float boxBlurDivisor = pow(2.0 * kernelSize + 1.0, 2.0);
    for (float i = -kernelSize; i <= kernelSize; i++) {
        for (float j = -kernelSize; j <= kernelSize; j++) {
            vec4 tex_col = texture(sampler2D(tex, smp), uv + vec2(i, j) * texelSize);
            boxBlurColour = boxBlurColour + tex_col.rgb;
        }
    }
    boxBlurColour = boxBlurColour / boxBlurDivisor;

    // ------ Gaussian blur ------

    vec3 b0 = texture(sampler2D(tex, smp), uv + vec2(-1, 1) * texelSize).rgb * 1.0;
    vec3 b1 = texture(sampler2D(tex, smp), uv + vec2(0, 1) * texelSize).rgb * 2.0;
    vec3 b2 = texture(sampler2D(tex, smp), uv + vec2(1, 1) * texelSize).rgb * 1.0;
    vec3 b3 = texture(sampler2D(tex, smp), uv + vec2(-1, 0) * texelSize).rgb * 2.0;
    vec3 b4 = texture(sampler2D(tex, smp), uv + vec2(0, 0) * texelSize).rgb * 4.0;
    vec3 b5 = texture(sampler2D(tex, smp), uv + vec2(1, 0) * texelSize).rgb * 2.0;
    vec3 b6 = texture(sampler2D(tex, smp), uv + vec2(-1, -1) * texelSize).rgb * 1.0;
    vec3 b7 = texture(sampler2D(tex, smp), uv + vec2(0, -1) * texelSize).rgb * 2.0;
    vec3 b8 = texture(sampler2D(tex, smp), uv + vec2(1, -1) * texelSize).rgb * 1.0;
    vec3 gaussianBlurColour = b0 + b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8;
    float gaussianDivisor = 16.0;
    gaussianBlurColour = gaussianBlurColour / gaussianDivisor;

    frag_colour = vec4(boxBlurColour, 1.0);
    // frag_colour = vec4(gaussianBlurColour, 1.0);
    // frag_colour = texture(sampler2D(tex, smp), uv);
}
@end

/* texquad shader program */
@program display display_vs display_fs