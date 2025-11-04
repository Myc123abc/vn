RWTexture2D<float> window_mask_image : register(u0);

struct Constants
{
  float left;
  float top;
  float right;
  float bottom;
};

ConstantBuffer<Constants> constants : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 pos: SV_DispatchThreadID)
{
  if (pos.x >= constants.left && pos.x <= constants.right && pos.y >= constants.top && pos.y <= constants.bottom)
    window_mask_image[pos.xy] = 1;
}
