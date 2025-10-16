#include "sdf.h"

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
  uint2    window_extent;
  int2     window_pos;
  uint32_t cursor_index;
};

enum : uint32_t
{
  type_cursor = 1,
  type_triangle,
};

struct ShapeProperty
{
  uint32_t type;
  uint32_t color;

  float4 get_color()
  {
    float r = float((color >> 24) & 0xFF) / 255;
    float g = float((color >> 16) & 0xFF) / 255;
    float b = float((color >> 8 ) & 0xFF) / 255;
    float a = float((color      ) & 0xFF) / 255;
    return float4(r, g, b, a);
  }
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
  result.color         = shape_property.get_color();
  result.buffer_offset = vertex.buffer_offset;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
///                              Pixel Shader
////////////////////////////////////////////////////////////////////////////////

float4 ps(PSParameter args) : SV_TARGET
{
  ShapeProperty shape_property = get_shape_property(args.buffer_offset);

  switch (shape_property.type)
  {
  default:
    return args.color;

  case type_cursor:
    return cursor_textures[constants.cursor_index].Sample(g_sampler, args.uv);

  case type_triangle:
  {
    float w = length(float2(ddx_fine(args.pos.x), ddy_fine(args.pos.y)));
    // TODO: distance
    return float4(1, 1, 0, 1);
  }
  }
}
