struct PSInput
{
  float4 pos   : SV_POSITION;
  float2 uv    : TEXCOORD;
  float4 color : COLOR;
};

struct Constants
{
  uint2 window_extent;
};
ConstantBuffer<Constants> constants : register(b0);

PSInput VSMain(float2 pos : POSITION, float2 uv : TEXCOORD, float4 color : COLOR)
{
  PSInput result;
  result.pos   = float4(pos / constants.window_extent * float2(2, -2) + float2(-1, 1), 0, 1);
  result.uv    = uv;
  result.color = color;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  return input.color;
}