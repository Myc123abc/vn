////////////////////////////////////////////////////////////////////////////////
///                                 Structure
////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
  float2   pos           : POSITION;
  float2   uv            : TEXCOORD;
  uint32_t buffer_offset : BUFFER_OFFSET;
};

struct PSParameter
{
  float4   pos           : SV_POSITION;
  float2   uv            : TEXCOORD;
  float4   color         : COLOR;
  uint32_t buffer_offset : BUFFER_OFFSET;
};

struct Constants
{
  uint2 window_extent;
  int2  window_pos;
  int   cursor_index;
};

struct ShapeProperty
{
  uint32_t type;
  uint32_t color;
  uint32_t flags;
};

////////////////////////////////////////////////////////////////////////////////
///                              Binding
////////////////////////////////////////////////////////////////////////////////

ConstantBuffer<Constants> constants         : register(b0);
SamplerState              g_sampler         : register(s0);
Texture2D                 cursor_textures[] : register(t0);
ByteAddressBuffer         buffer            : register(t0, space1);

////////////////////////////////////////////////////////////////////////////////
///                              Function
////////////////////////////////////////////////////////////////////////////////

float4 to_float4(uint32_t color)
{
  float r = float((color >> 24) & 0xFF) / 255;
  float g = float((color >> 16) & 0xFF) / 255;
  float b = float((color >> 8 ) & 0xFF) / 255;
  float a = float((color      ) & 0xFF) / 255;
  return float4(r, g, b, a);
}

bool cursor_render(uint32_t flags)
{
  return bool(flags & 0x00000001);
}

ShapeProperty get_shape_property(uint32_t offset)
{
  return buffer.Load<ShapeProperty>(offset);
}

////////////////////////////////////////////////////////////////////////////////
///                              Vertex Shader
////////////////////////////////////////////////////////////////////////////////

PSParameter vs(Vertex vertex)
{
  ShapeProperty shape_property = get_shape_property(vertex.buffer_offset);
  
  PSParameter result;
  result.pos           = float4((vertex.pos + constants.window_pos) / constants.window_extent * float2(2, -2) + float2(-1, 1), 0, 1);
  result.uv            = vertex.uv;
  result.color         = to_float4(shape_property.color);
  result.buffer_offset = vertex.buffer_offset;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
///                              Pixel Shader
////////////////////////////////////////////////////////////////////////////////

float4 ps(PSParameter args) : SV_TARGET
{
  ShapeProperty shape_property = get_shape_property(args.buffer_offset);
  if (cursor_render(shape_property.flags))
    return cursor_textures[constants.cursor_index].Sample(g_sampler, args.uv);
  else
    return args.color;
}
