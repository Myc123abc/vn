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
  int                   cursor_index{};
};

struct ShapeProperty
{
  uint32_t type{};
  uint32_t color{};
  uint32_t flags{};
};

}}