struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
float4 vertMain(VSInput input) : SV_Position {
    return mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
}

[shader("fragment")]
void fragMain() {}
