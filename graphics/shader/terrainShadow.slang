struct VSInput {
    float3 inPosition;
    float3 inNormal;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 lightMVP;
};
[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;
[[vk::binding(0, 1)]] Sampler2D shadowMap;

struct VSOutput {
    float4 pos : SV_Position;
    float3 normal;
    float4 lightSpacePos;
    float worldY;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.pos = mul(ubo.projection, mul(ubo.view, worldPos));
    output.normal = normalize(mul((float3x3)ubo.model, input.inNormal));
    output.lightSpacePos = mul(ubo.lightMVP, float4(input.inPosition, 1.0));
    output.worldY = worldPos.y;
    return output;
}

static const float2 poissonDisk[16] = {
    float2(-0.94201624, -0.39906216),
    float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845),
    float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554),
    float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507),
    float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367),
    float2( 0.14383161, -0.14100790),
};

float rand(float2 co) {
    // Keep co small to preserve sin() precision on GPU
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

float shadowCalc(float4 lightSpacePos, float3 normal, float3 lightDir, float2 screenPos) {
    float3 proj = lightSpacePos.xyz / lightSpacePos.w;
    float2 uv = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 0.0;

    float currentDepth = proj.z;

    float cosTheta = clamp(dot(normal, normalize(lightDir)), 0.0, 1.0);
    float bias = 0.01 * tan(acos(cosTheta));
    bias = clamp(bias, 0.0, 0.01);

    // Interleaved Gradient Noise (Jimenez 2014) — no visible period in screen space
    float angle = frac(52.9829189 * frac(0.06711056 * screenPos.x + 0.00583715 * screenPos.y)) * 6.28318;
    float s = sin(angle), c = cos(angle);

    float shadow = 0.0;
    float spread = 2.5 / 1024.0;
    for (int i = 0; i < 16; i++) {
        float2 tap = float2(
            poissonDisk[i].x * c - poissonDisk[i].y * s,
            poissonDisk[i].x * s + poissonDisk[i].y * c
        );
        float pcfDepth = shadowMap.Sample(uv + tap * spread).r;
        shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
    }
    return shadow / 16.0;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 lightDir = normalize(float3(1.0, 0.8, 1.0));
    float3 normal   = normalize(vertIn.normal);

    float shadow = shadowCalc(vertIn.lightSpacePos, normal, lightDir, vertIn.pos.xy);

    float ambient = 0.35;
    float diffuse = max(dot(normal, lightDir), 0.0);
    //float light   = ambient + (1.0) * (1.0 - ambient) * diffuse;
    float light   = ambient + (1.0 - shadow) * (1.0 - ambient) * diffuse;

    // Height-based color
    float waterLevel = 4.0;
    float grassStart = 5.5;
    float t = clamp((vertIn.worldY - waterLevel) / (grassStart - waterLevel), 0.0, 1.0);
    float3 sandColor  = float3(0.85, 0.78, 0.40);
    float3 grassColor = float3(0.27, 0.65, 0.18);
    float3 color = lerp(grassColor, sandColor, t);

    // Underwater darkening
    if (vertIn.worldY < waterLevel) {
        float depth = clamp((waterLevel - vertIn.worldY) / 8.0, 0.0, 1.0);
        float3 waterTint = float3(0.05, 0.10, 0.18);
        color = lerp(color, waterTint, depth * 0.75);
    }

    return float4(color * light, 1.0);
}
