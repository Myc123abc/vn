Texture2D<float4>   input_image  : register(t0);
RWTexture2D<float4> output_image : register(u0);

[numthreads(16, 16, 1)]
void compute_main(uint3 id : SV_DispatchThreadID)
{
  uint2 uv = id.xy;
 
  float4 color = float4(0, 0, 0, 0);

  //int offsets[4] = { -1, 0, 1, 2 };
  //for (int y = 0; y < 4; ++y)
  //{
  //  for (int x = 0; x < 4; ++x)
  //  {
  //    uint2 sample_coord = uv + uint2(offsets[x], offsets[y]);
  //    color += input_image.Load(int3(sample_coord, 0));
  //  }
  //}
  //color /= 16;

  //output_image[uv] = color;

  output_image[uv] = input_image.Load(int3(uv, 0));
}