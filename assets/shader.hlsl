////////////////////////////////////////////////////////////////////////////////
///                                 Structure
////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
  float3   pos           : POSITION;
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
  float2   window_pos;
  uint32_t cursor_index;
};

enum : uint32_t
{
  type_cursor = 1,
  type_triangle,
  type_rectangle,
  type_circle,
  type_line,
  type_bezier,

  type_path,
  type_path_line,
  type_path_bezier
};

enum : uint32_t
{
  op_none,
  op_union,
  op_discard
};

enum : uint32_t
{
  // flag_window_shadow = 0b1
};

struct ShapeProperty
{
  uint32_t type;
  float4   color;
  float    thickness;
  uint32_t op;
  uint32_t flags;

  // bool use_window_shadow()
  // {
  //   return bool(flags & flag_window_shadow);
  // }
};

////////////////////////////////////////////////////////////////////////////////
///                              Binding
////////////////////////////////////////////////////////////////////////////////

ConstantBuffer<Constants> constants         : register(b0);
SamplerState              g_sampler         : register(s0);
Texture2D                 cursor_textures[] : register(t0);
ByteAddressBuffer         buffer            : register(t0, space1);

////////////////////////////////////////////////////////////////////////////////
///                              Functions
////////////////////////////////////////////////////////////////////////////////

ShapeProperty get_shape_property(inout uint32_t offset)
{
  ShapeProperty res = buffer.Load<ShapeProperty>(offset);
  offset += sizeof(ShapeProperty);
  return res;
}

float2 get_point(inout uint32_t offset)
{
  float2 res = buffer.Load<float2>(offset);
  offset += sizeof(float2);
  return res + constants.window_pos;
}

float get_float(inout uint32_t offset)
{
  float res = buffer.Load<float>(offset);
  offset += sizeof(float);
  return res;
}

uint32_t get_uint(inout uint32_t offset)
{
  uint32_t res = buffer.Load<uint32_t>(offset);
  offset += sizeof(uint32_t);
  return res;
}

#include "sdf.h"

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

float get_distance_parition(float2 pos, inout uint offset)
{
  float d;
  switch (get_uint(offset))
  {
  case type_path_line:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    d = sdf_line_partition(pos, p0, p1);
    break;
  }
  case type_path_bezier:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    float2 p2 = get_point(offset);
    d = sdf_bezier_partition(pos, p0, p1, p2);
    break;
  }
  }
  return d;
}

float get_sd(float2 pos, uint32_t type, inout uint32_t offset)
{
  float d = -3.4028235e+38;
  switch (type)
  {
  default:
    return 0;

  case type_triangle:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    float2 p2 = get_point(offset);
    d = sdTriangle(pos, p0, p1, p2);
    break;
  }

  case type_rectangle:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    float2 extent_div2 = (p1 - p0) * 0.5;
    float2 center = p0 + extent_div2;
    d = sdBox(pos - center,extent_div2);
    break;
  }

  case type_circle:
  {
    float2 center = get_point(offset);
    float  radius = get_float(offset);
    d = sdCircle(pos - center, radius);
    break;
  }

  case type_line:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    d = sdSegment(pos, p0, p1);
    break;
  }

  case type_bezier:
  {
    float2 p0 = get_point(offset);
    float2 p1 = get_point(offset);
    float2 p2 = get_point(offset);
    d = sdBezier(pos, p0, p1, p2);
    break;
  }

  case type_path:
  {
    uint32_t count = get_uint(offset);
    for (uint32_t i = 0; i < count; ++i)
    {
      float partition_distance = get_distance_parition(pos, offset);
      float distance = max(d, partition_distance);
      
      // aliasing problem:
      // when two line segment in same line, such as (0,0)(50,50) to (50,50)(100,100)
      // max(d0,d1) will lead aliasing problem
      // so use min(abs(d0),abs(d1)) to resolve
      // well min's way can only use for 1-pixel case,
      // so for filled and thickness wireform we use max still,
      // and use min on bround, perfect! (I spent half day to resolve... my holiday...)
      if (distance > 0.0)
        d = min(abs(d), abs(partition_distance));
      else
        d = distance;
    }
    break;
  }
  }
  return d;
}

////////////////////////////////////////////////////////////////////////////////
///                              Vertex Shader
////////////////////////////////////////////////////////////////////////////////

PSParameter vs(Vertex vertex)
{
  ShapeProperty shape_property = buffer.Load<ShapeProperty>(vertex.buffer_offset);

  PSParameter result;
  result.pos           = float4((vertex.pos.xy + constants.window_pos) / constants.window_extent * float2(2, -2) + float2(-1, 1), vertex.pos.z, 1);
  result.uv            = vertex.uv;
  result.color         = shape_property.color;
  result.buffer_offset = vertex.buffer_offset;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
///                              Pixel Shader
////////////////////////////////////////////////////////////////////////////////

float4 ps(PSParameter args) : SV_TARGET
{
  uint32_t offset = args.buffer_offset;

  ShapeProperty shape_property = get_shape_property(offset);

  float4 color = args.color;

  if (shape_property.type == type_cursor)
    return cursor_textures[constants.cursor_index].Sample(g_sampler, args.uv);

  float w = length(float2(ddx_fine(args.pos.x), ddy_fine(args.pos.y)));
  float d = get_sd(args.pos.xy, shape_property.type, offset);

  while (shape_property.op != op_none)
  {
    if (shape_property.op == op_union)
    {
      shape_property = get_shape_property(offset);
      color = shape_property.color;
      d     = min(d, get_sd(args.pos.xy, shape_property.type, offset));
    }
    else if (shape_property.op == op_discard)
    {
      ShapeProperty discard_shape_property = get_shape_property(offset);
      float discard_d = get_sd(args.pos.xy, discard_shape_property.type, offset);
      if (discard_d < 0) discard;
      break;
    }
  }

  return get_color(color, w, d, shape_property.thickness);
}
