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
  };

  struct Header
  {
    Type     type{};
    uint32_t color{};
    float    thickness{};
  };

  ShapeProperty(Type type, uint32_t color = {}, float thickness = {}, std::vector<glm::vec2> const& points = {}) noexcept
  {
    _data.reserve(sizeof(Header) / sizeof(uint32_t) + points.size() * sizeof(glm::vec2));
    _data.emplace_back(static_cast<uint32_t>(type));
    _data.emplace_back(color);
    _data.emplace_back(std::bit_cast<uint32_t>(thickness));
    for (auto const& p : points)
    {
      _data.emplace_back(std::bit_cast<uint32_t>(p.x));
      _data.emplace_back(std::bit_cast<uint32_t>(p.y));
    }
  }

  auto data()      const noexcept { return _data.data();                    }
  auto byte_size() const noexcept { return _data.size() * sizeof(uint32_t); }

private:
  std::vector<uint32_t> _data{};
};

}}
