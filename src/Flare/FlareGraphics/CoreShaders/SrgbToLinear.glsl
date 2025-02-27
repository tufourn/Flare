#ifndef SHADER_SRGB_TO_LINEAR_GLSL
#define SHADER_SRGB_TO_LINEAR_GLSL
vec4 srgbToLinear(vec4 srgbIn) {
    vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
    vec3 linOut = mix(srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055), vec3(2.4)), bLess);

    return vec4(linOut, srgbIn.w);
}
#endif
