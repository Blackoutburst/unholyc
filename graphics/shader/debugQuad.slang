struct VSInput {
    float3 inPosition;
    float3 inNormal;  // unused
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;
[[vk::binding(0, 1)]] Sampler2D depthTex;

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.uv = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float depth = depthTex.Sample(vertIn.uv).r;
    // Remap depth for visibility: shadow map depth is near 1.0 for far, near 0.0 for close
    // Linearize a bit to make details visible
    float d = 1.0 - depth;
    d = clamp(d * 10.0, 0.0, 1.0);
    return float4(d, d, d, 1.0);
}
