struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord; // unused
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 normal;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.normal = normalize(mul((float3x3)ubo.model, input.inNormal));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 lightDir = normalize(float3(0.6, 1.0, 0.4));
    float3 normal = normalize(vertIn.normal);

    float ambient = 0.25;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float light = ambient + (1.0 - ambient) * diffuse;

    float3 waterColor = float3(0.16, 0.52, 0.78);
    return float4(waterColor * light, 0.6);
}
