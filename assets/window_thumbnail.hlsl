struct Constants
{
  float2 vertices[4];
  uint2  window_extent;
  float2 window_pos;
  float2 thumbnail_extent;
};

ConstantBuffer<Constants> constants    : register(b0);
SamplerState              g_sampler    : register(s0);
Texture2D                 window_image : register(t0);

struct PSParameter
{
  float4 pos : SV_Position;
  float2 uv  : TexCoord;
};

PSParameter vs(uint id: SV_VertexID, float2 pos : Position)
{
  PSParameter param;
  param.pos = float4(pos / constants.thumbnail_extent * float2(2, -2) + float2(-1, 1), 0, 1);
  param.uv  = (constants.vertices[id] + constants.window_pos) / constants.window_extent;
  return param;
}

float4 ps(PSParameter arg) : SV_Target
{
  return window_image.Sample(g_sampler, arg.uv);
}
