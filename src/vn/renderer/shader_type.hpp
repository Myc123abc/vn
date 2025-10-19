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
  };

  enum class Operator : uint32_t
  {
    none,
    u,
  };

  struct Header
  {
    Type     type{};
    uint32_t color{};
    float    thickness{};
    Operator op{};
  };

  ShapeProperty(Type type, uint32_t color = {}, float thickness = {}, Operator op = {}, std::vector<glm::vec2> const& points = {}, std::vector<float> const& values = {}) noexcept
  {
    _data.reserve(sizeof(Header) / sizeof(uint32_t) + points.size() * sizeof(glm::vec2) + values.size() * sizeof(float));
    _data.emplace_back(static_cast<uint32_t>(type));
    _data.emplace_back(color);
    _data.emplace_back(std::bit_cast<uint32_t>(thickness));
    _data.emplace_back(static_cast<uint32_t>(op));
    for (auto const& p : points)
    {
      _data.emplace_back(std::bit_cast<uint32_t>(p.x));
      _data.emplace_back(std::bit_cast<uint32_t>(p.y));
    }
    for (auto const& v : values)
    {
      _data.emplace_back(std::bit_cast<uint32_t>(v));
    }
  }

  auto data()      const noexcept { return _data.data();                    }
  auto byte_size() const noexcept { return _data.size() * sizeof(uint32_t); }

  void set_color(uint32_t color)      noexcept { _data[1] = color;                              }
  void set_thickness(float thickness) noexcept { _data[2] = std::bit_cast<uint32_t>(thickness); }
  void set_operator(Operator op)      noexcept { _data[3] = static_cast<uint32_t>(op);          }

private:
  std::vector<uint32_t> _data{};
};

}}
