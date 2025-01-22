static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

struct VertexOutput {
    float4 sv_position : SV_Position;
};

struct FragmentOutput {
    float4 color;
};

[shader("vertex")]
VertexOutput main(int id: SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[id], 0.f, 1.f);

    return output;
}

[shader("fragment")]
FragmentOutput main(VertexOutput vertexOut) : SV_Target {
    FragmentOutput output;
    output.color = {1.f, 0.f, 0.f, 1.f};

    return output;
}
