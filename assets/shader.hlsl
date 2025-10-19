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
  type_rectangle,
  type_circle,
};

struct ShapeProperty
{
  uint32_t type;
  uint32_t color;
  float    thickness;

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

float2 get_point(uint32_t offset, uint32_t index)
{
  return buffer.Load<float2>(offset + sizeof(ShapeProperty) + sizeof(float2) * index) + constants.window_pos;
}

float get_float(uint32_t offset, uint32_t index)
{
  return buffer.Load<float>(offset + sizeof(ShapeProperty) + sizeof(float) * index);
}

float4 get_color(float4 color, float w, float d, float t)
{
  float value;
  if (t == 0)
    value = d;
  else if (t == 1)
    value = abs(d);
  else
  {
    if (d > 0.0)
      value = d;
    else
      value = -d - t + 1.0;
  }
  if (value >= w) discard;
  float alpha = 1.0 - smoothstep(0.0, w, value);
  return float4(color.rgb, color.a * alpha);
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

  float w = length(float2(ddx_fine(args.pos.x), ddy_fine(args.pos.y)));
  float d;

  switch (shape_property.type)
  {
  default:
    return args.color;

  case type_cursor:
    return cursor_textures[constants.cursor_index].Sample(g_sampler, args.uv);

  case type_triangle:
  {
    d = sdTriangle(
      args.pos.xy,
      get_point(args.buffer_offset, 0),
      get_point(args.buffer_offset, 1),
      get_point(args.buffer_offset, 2));
    break;
  }

  case type_rectangle:
  {
    float2 p0 = get_point(args.buffer_offset, 0);
    float2 p1 = get_point(args.buffer_offset, 1);
    float2 extent_div2 = (p1 - p0) * 0.5;
    float2 center = p0 + extent_div2;
    d = sdBox(
      args.pos.xy - center,
      extent_div2
    );
    break;
  }

  case type_circle:
  {
    float2 center = get_point(args.buffer_offset, 0);
    float radius = get_float(args.buffer_offset, 2);
    d = sdCircle(args.pos.xy - center, radius);
    break;
  }
  }
  return get_color(args.color, w, d, shape_property.thickness);
}
