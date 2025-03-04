#ifndef SHADER_NORMAL_ENCODING_GLSL
#define SHADER_NORMAL_ENCODING_GLSL

// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233/2

// for each component of v, return -1 if the component is < 0, else 1
vec2 signNotZero(vec2 v) {
    return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
}

vec2 octahedralEncode(vec3 n) {
    n.xy /= dot(abs(n), vec3(1));

    return mix(n.xy, (1.0 - abs(n.yx)) * signNotZero(n.xy), step(n.z, 0.0));
}

vec3 octahedralDecode(vec2 n) {
    vec3 v = vec3(n.xy, 1.0 - abs(n.x) - abs(n.y));
    v.xy = mix(v.xy, (1.0 - abs(v.yx)) * signNotZero(v.xy), step(v.z, 0));

    return normalize(v);
}

#endif