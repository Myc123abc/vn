RWTexture2D<float4> image : register(u0);

struct Constants
{
  float2 window_extent;
};

ConstantBuffer<Constants> constants : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
  float2 uv = id.xy / constants.window_extent;
  image[id.xy] = float4(uv.xy, 0, 1);
}