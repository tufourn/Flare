#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "CoreShaders/BindlessCommon.glsl"

layout (location = 0) out vec2 outUV;
layout (location = 1) out uint outDrawId;
layout (location = 2) out vec3 outModelSpacePos;
layout (location = 3) out vec4 outClipSpacePos;
layout (location = 4) out vec4 outPrevClipSpacePos;
layout (location = 5) out mat3 outTBN;

struct GBufferUniforms {
    mat4 modelView;
    mat4 prevModelView;
};

layout (set = 0, binding = 0) uniform U { GBufferUniforms uniforms; } gBufferUniforms[];

void main() {
    const uint indirectDrawDataBufferIndex = pc.data0;
    const uint transformBufferIndex = pc.data1;
    const uint materialBufferIndex = pc.data2;

    GBufferUniforms uniforms = gBufferUniforms[pc.uniformOffset].uniforms;

    IndirectDrawData dd = indirectDrawDataAlias[indirectDrawDataBufferIndex].indirectDrawDatas[gl_DrawID];

    mat4 transform = transformAlias[transformBufferIndex].transforms[dd.transformOffset];
}