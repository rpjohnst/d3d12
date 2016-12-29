struct VS_INPUT {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float4 color: COLOR;
};

cbuffer ConstantsPerObject : register(b0) {
    float4x4 worldViewProj;
};

VS_OUTPUT main(VS_INPUT vertex) {
    VS_OUTPUT output;
    output.pos = mul(worldViewProj, float4(vertex.pos, 1.0));
    output.color = float4(vertex.normal, 1.0);
    return output;
}
