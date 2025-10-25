#pragma once

#include <glm/glm.hpp>

#include <bit>

namespace vn { namespace renderer {

struct alignas(8) Vertex
{
  glm::vec2 pos{};
  glm::vec2 uv{};
  uint32_t  buffer_offset{};
};

struct Constants
{
  glm::vec<2, uint32_t> window_extent{};
  glm::vec<2, int32_t>  window_pos{};
  uint32_t              cursor_index{};
};

struct ShapeProperty
{
  enum class Type : uint32_t
  {
    cursor = 1,
    triangle,
    rectangle,
    circle,
    line,
    bezier,

    path,
    path_line,
    path_bezier
  };

  enum class Operator : uint32_t
  {
    none,
    u,
    discard
  };

  struct Header
  {
    Type      type{};
    glm::vec4 color{};
    float     thickness{};
    Operator  op{};
    uint32_t  padding{};
  };

  ShapeProperty(Type type, glm::vec4 color = {}, float thickness = {}, Operator op = {}, std::vector<float> const& values = {}) noexcept
  {
    _data.reserve(sizeof(Header) / sizeof(uint32_t) + values.size() * sizeof(float));
    _data.emplace_back(std::bit_cast<uint32_t>(type));
    _data.emplace_back(std::bit_cast<uint32_t>(color.r));
    _data.emplace_back(std::bit_cast<uint32_t>(color.g));
    _data.emplace_back(std::bit_cast<uint32_t>(color.b));
    _data.emplace_back(std::bit_cast<uint32_t>(color.a));
    _data.emplace_back(std::bit_cast<uint32_t>(thickness));
    _data.emplace_back(std::bit_cast<uint32_t>(op));
    _data.emplace_back(std::bit_cast<uint32_t>(0)); // padding
    for (auto const& v : values)
      _data.emplace_back(std::bit_cast<uint32_t>(v));
  }

  auto data()      const noexcept { return _data.data();                    }
  auto byte_size() const noexcept { return _data.size() * sizeof(uint32_t); }

  void set_color(glm::vec4 const& color) noexcept
  { 
    _data[1] = std::bit_cast<uint32_t>(color.r);
    _data[2] = std::bit_cast<uint32_t>(color.g);
    _data[3] = std::bit_cast<uint32_t>(color.b);
    _data[4] = std::bit_cast<uint32_t>(color.a);
  }
  void set_thickness(float thickness) noexcept { _data[5] = std::bit_cast<uint32_t>(thickness); }
  void set_operator(Operator op)      noexcept { _data[6] = std::bit_cast<uint32_t>(op);        }

private:
  std::vector<uint32_t> _data{};
};

}}
