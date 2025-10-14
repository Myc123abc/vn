////////////////////////////////////////////////////////////////////////////////
///                                 Structure
////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
  float2   pos   : POSITION;
  float2   uv    : TEXCOORD;
  uint32_t color : COLOR;
  uint32_t flags : FLAGS;
};

struct PSParameter
{
  float4 pos     : SV_POSITION;
  float2 uv      : TEXCOORD;
  float4 color   : COLOR;
  uint32_t flags : FLAGS;
};

struct Constants
{
  uint2 window_extent;
  int2  window_pos;
  int   cursor_index;
  uint  buffer_offset;
};

struct ShapeProperty
{
  uint32_t type;
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

ShapeProperty get_shape_property(inout uint32_t offset)
{
  uint32_t old_offset = offset;
  offset += sizeof(ShapeProperty);
  return buffer.Load<ShapeProperty>(old_offset);
}

////////////////////////////////////////////////////////////////////////////////
///                              Vertex Shader
////////////////////////////////////////////////////////////////////////////////

PSParameter vs(Vertex vertex)
{
  PSParameter result;
  result.pos           = float4((vertex.pos + constants.window_pos) / constants.window_extent * float2(2, -2) + float2(-1, 1), 0, 1);
  result.uv            = vertex.uv;
  result.color         = to_float4(vertex.color);
  result.flags = vertex.flags;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
///                              Pixel Shader
////////////////////////////////////////////////////////////////////////////////

float4 ps(PSParameter args) : SV_TARGET
{
  //uint32_t offset = 0;
  //ShapeProperty shape = get_shape_property(offset);
  if (buffer.Load(constants.buffer_offset) == 42)
    return float4(0,1,0,1);
  if (cursor_render(args.flags))
    return cursor_textures[constants.cursor_index].Sample(g_sampler, args.uv);
  else
    return args.color;
}
