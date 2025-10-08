#version 460

#extension GL_EXT_buffer_reference : require

struct Vertex
{
  vec2 pos;
  vec2 uv;
  uint color;
};

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer Vertices 
{
  Vertex data[];
};

layout(push_constant) uniform PushConstant
{
  Vertices vertices;
  uvec2    window_extent;
  ivec2    window_pos;
} pc;

vec4 to_vec4(uint color)
{
  float r = float((color >> 24) & 0xFF) / 255;
  float g = float((color >> 16) & 0xFF) / 255;
  float b = float((color >> 8 ) & 0xFF) / 255;
  float a = float((color      ) & 0xFF) / 255;
  return vec4(r, g, b, a);
}

void main()
{
  Vertex vertex = pc.vertices.data[gl_VertexIndex];

  gl_Position = vec4((vertex.pos + pc.window_pos) / pc.window_extent * vec2(2) - vec2(1), 0, 1);
  uv          = vertex.uv;
  color       = to_vec4(vertex.color);
}