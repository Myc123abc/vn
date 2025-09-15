struct PSInput
{
  float4 position : SV_POSITION;
  float4 color    : COLOR;
};

struct Constants
{
  float alpha;
};
ConstantBuffer<Constants> cb : register(b0);

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
  PSInput result;
  result.position = position;
  color.a = cb.alpha;
  color.rgb *= color.a;
  result.color = color;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  return input.color;
}