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

float4 to_float4(uint32_t color)
{
    float r = float((color >> 24) & 0xFF) / 255;
    float g = float((color >> 16) & 0xFF) / 255;
    float b = float((color >> 8 ) & 0xFF) / 255;
    float a = float((color      ) & 0xFF) / 255;
    return float4(r, g, b, a);
}

PSInput vs(float2 pos: POSITION, float2 uv: TEXCOORD, uint32_t color : COLOR)
{
  PSInput result;
  result.pos   = float4(pos / constants.window_extent * float2(2, -2) + float2(-1, 1), 0, 1);
  result.uv    = uv;
  result.color = to_float4(color);
  return result;
}

float4 ps(PSInput input) : SV_TARGET
{
  return input.color;
}