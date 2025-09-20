Texture2D<float4>   input  : register(t0);
RWTexture2D<float4> output : register(u0);

[numthreads(16, 16, 1)]
void compute_main(uint3 id : SV_DispatchThreadID)
{
  uint2 coord = id.xy;
  float4 color = input.Load(int3(coord, 0));
  output[coord] = color;
}