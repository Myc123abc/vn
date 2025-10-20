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

enum : uint32_t
{
  op_none,
  op_union,
};

struct ShapeProperty
{
  uint32_t type;
  uint32_t color;
  float    thickness;
  uint32_t op;

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

float sdTriangle(float2 p, float2 p0, float2 p1, float2 p2)
{
  float2 e0 = p1-p0, e1 = p2-p1, e2 = p0-p2;
  float2 v0 = p -p0, v1 = p -p1, v2 = p -p2;
  float2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
  float2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
  float2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
  float s = sign( e0.x*e2.y - e0.y*e2.x );
  float2 d = min(min(float2(dot(pq0,pq0), s*(v0.x*e0.y-v0.y*e0.x)),
                     float2(dot(pq1,pq1), s*(v1.x*e1.y-v1.y*e1.x))),
                     float2(dot(pq2,pq2), s*(v2.x*e2.y-v2.y*e2.x)));
  return -sqrt(d.x)*sign(d.y);
}

float sdBox(float2 p, float2 b)
{
  float2 d = abs(p)-b;
  return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float sdCircle(float2 p, float r)
{
  return length(p) - r;
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

float get_sd(float2 pos, uint32_t type, inout uint32_t offset)
{
  float d;
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
      color = shape_property.get_color();
      d     = min(d, get_sd(args.pos.xy, shape_property.type, offset));
    }
  }

  return get_color(color, w, d, shape_property.thickness);
}
