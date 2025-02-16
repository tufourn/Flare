struct BindingsOffset {
    uint bufferIndex;
    uint offset;
}

[[vk::push_constant]] ConstantBuffer<BindingsOffset> pc;

[[vk::binding(0,0)]] ByteAddressBuffer g_byteAddressBuffer[];

[[vk::binding(0,1)]] Texture2D g_texture2D[];

[[vk::binding(0,2)]] RWTexture2D g_rwTexture2D[];

[[vk::binding(0,3)]] SamplerState g_samplerState[];

struct GpuMeshDraw {
    uint globalBufferIndex;
    uint positionBufferIndex;
    uint uvBufferIndex;
    uint transformBufferIndex;

    uint transformOffset;
    float pad[3];
}

struct Globals {
    float4x4 mvp;
}

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

    GpuMeshDraw md = g_byteAddressBuffer[pc.bufferIndex].Load<GpuMeshDraw>(pc.offset * 32);

    Globals globals = g_byteAddressBuffer[md.globalBufferIndex].Load<Globals>(0);
    float4 position = g_byteAddressBuffer[md.positionBufferIndex].Load<float4>(input.vertexIndex * 16);
    float2 uv = g_byteAddressBuffer[md.uvBufferIndex].Load<float2>(input.vertexIndex * 8);
    float4x4 transform = g_byteAddressBuffer[md.transformBufferIndex].Load<float4x4>(md.transformOffset * 64);

    output.sv_position = mul(transpose(globals.mvp), mul(transpose(transform), position));
//     output.sv_position = mul(transpose(globals.mvp), position);
    output.uv = uv;

    return output;
}

[shader("fragment")]
Fragment main(float2 uv : TEXCOORD0) : SV_Target {
    Fragment output;
    // hardcoded index 4 because first 3 are depth images
    output.color = g_texture2D[4].Sample(g_samplerState[0], uv);

    return output;
}
