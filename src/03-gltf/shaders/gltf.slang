[[vk::binding(10,0)]]
Sampler2D<float4> globalTextures[];

[[vk::binding(0,1)]]
StructuredBuffer<float4> positions;
[[vk::binding(1,1)]]
StructuredBuffer<float2> uvs;
[[vk::binding(2,1)]]
StructuredBuffer<float4x4> transforms;
[[vk::binding(3,1)]]
cbuffer uniforms {
    float4x4 mvp;
};

struct VertexIn {
    uint vertexIndex : SV_VertexID;
}

struct VertexOut {
    float4 sv_position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct Fragment
{
    float4 color;
}

[shader("vertex")]
VertexOut main(VertexIn input) {
    VertexOut output;
    output.sv_position = mul(transpose(mvp), mul(transpose(transforms[0]), positions[input.vertexIndex]));
    output.uv = uvs[input.vertexIndex];

    return output;
}

[shader("fragment")]
Fragment main(float2 uv : TEXCOORD0) : SV_Target {
    Fragment output;
    output.color = globalTextures[1].Sample(uv);

    return output;
}
