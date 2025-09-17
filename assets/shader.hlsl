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

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0); // TODO: what about sampler2D

PSInput VSMain(float4 pos : POSITION, float2 uv : TEXCOORD, float4 color : COLOR)
{
  PSInput result;
  result.pos = pos;
  result.uv = uv;
  color.a = cb.alpha;
  color.rgb *= color.a;
  result.color = color;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
  float4 color = g_texture.Sample(g_sampler, input.uv);
  color.rgb *= input.color.rgb;
  color.rgb *= input.color.a;
  color.a = input.color.a;
  return color;
}