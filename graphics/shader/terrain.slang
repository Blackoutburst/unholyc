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
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.normal = normalize(mul((float3x3)ubo.model, input.inNormal));

    // Height-based color: sand near water level, green grass above
    float waterLevel = 5.0;
    float grassStart = 5.5;
    float t = clamp((input.inPosition.y - waterLevel) / (grassStart - waterLevel), 0.0, 1.0);
    float3 sandColor  = float3(0.95, 0.98, 0.40);
    float3 grassColor = float3(0.37, 0.85, 0.28);
    output.color = lerp(grassColor, sandColor, t);

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 lightDir = normalize(float3(0.6, 1.0, 0.4));
    float3 normal = normalize(vertIn.normal);

    float ambient = 0.25;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float light = ambient + (1.0 - ambient) * diffuse;

    return float4(vertIn.color * light, 1.0);
}
