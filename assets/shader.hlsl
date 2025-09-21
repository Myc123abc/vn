struct PSInput
{
  float4 pos   : SV_POSITION;
  float2 uv    : TEXCOORD;
  float4 color : COLOR;
};

struct Constants
{
  float alpha;
};
ConstantBuffer<Constants> cb : register(b0);

PSInput VSMain(float4 pos : POSITION, float2 uv : TEXCOORD, float4 color : COLOR)
{
  PSInput result;
  result.pos   = pos;
  result.uv    = uv;
  result.color = color;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  return input.color;
}