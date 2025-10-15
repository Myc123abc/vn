#pragma once

#include <glm/glm.hpp>

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
  };

  Type     type{};
  uint32_t color{};
};

}}