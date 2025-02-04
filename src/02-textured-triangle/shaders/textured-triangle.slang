cbuffer Uniforms
{
    float4x4 mvp;
}

Texture2D<float4> texture;
SamplerState sampler;

struct VertexIn
{
    float3	position : POSITION;
    float2	uv       : TEXCOORD0;
};

struct VertexOut
{
    float4 sv_position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

struct Fragment
{
    float4 color;
};

[shader("vertex")]
VertexOut main(
    VertexIn vertexIn)
{
    VertexOut output;

    float3 position = vertexIn.position;
    float2 uv    = vertexIn.uv;

    output.sv_position = mul(mvp, float4(position, 1.0));
    output.uv = uv;

    return output;
}

[shader("fragment")]
Fragment main(
    float2 uv : TEXCOORD0) : SV_Target
{
    Fragment output;
    output.color = texture.Sample(sampler, uv);

    return output;
}