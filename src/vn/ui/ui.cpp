#include "ui.hpp"
#include "../renderer/window_manager.hpp"
#include "ui_context.hpp"

using namespace vn::renderer;
using namespace vn::ui;

namespace {

auto get_bounding_rectangle(std::vector<glm::vec2> const& data) -> std::pair<glm::vec2, glm::vec2>
{
  assert(data.size() > 1);

  auto min = data[0];
  auto max = data[0];

  for (auto i = 1; i < data.size(); ++i)
  {
    auto& p = data[i];
    if (p.x < min.x) min.x = p.x;
    if (p.y < min.y) min.y = p.y;
    if (p.x > max.x) max.x = p.x;
    if (p.y > max.y) max.y = p.y;
  }

  return { min, max };
}

void add_shape(
  ShapeProperty::Type             type,
  uint32_t                        color,
  float                           thickness,
  std::pair<glm::vec2, glm::vec2> bounding_rectangle,
  std::vector<glm::vec2> const&   points,
  std::vector<float> const&       values = {}) noexcept
{
  auto ctx        = UIContext::instance();
  auto [min, max] = bounding_rectangle;

  min -= glm::vec2(1);
  max += glm::vec2(1);

  ctx->render_data.vertices.append_range(std::vector<Vertex>
  {
    { min,              {}, ctx->shape_properties_offset },
    { { max.x, min.y }, {}, ctx->shape_properties_offset },
    { max,              {}, ctx->shape_properties_offset },
    { { min.x, max.y }, {}, ctx->shape_properties_offset },
  });
  ctx->render_data.indices.append_range(std::vector<uint16_t>
  {
    static_cast<uint16_t>(ctx->render_data.idx_beg + 0),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 1),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 2),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 0),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 2),
    static_cast<uint16_t>(ctx->render_data.idx_beg + 3),
  });
  ctx->render_data.idx_beg += 4;

  ctx->render_data.shape_properties.emplace_back(ShapeProperty
  {
    type,
    color,
    thickness,
    ctx->op,
    points,
    values
  });

  ctx->shape_properties_offset += ctx->render_data.shape_properties.back().byte_size();
}
  
}

namespace vn { namespace ui {

void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> const& update_func) noexcept
{
  UIContext::instance()->add_window(name, x, y, width, height, update_func);
}

auto window_count() noexcept -> uint32_t
{
  return WindowManager::instance()->window_count();
}

auto window_extent() noexcept -> std::pair<uint32_t, uint32_t>
{
  return { UIContext::instance()->window.width, UIContext::instance()->window.height };
}

void begin_union(uint32_t color, float thickness) noexcept
{
  auto ctx = UIContext::instance();
  ctx->op        = ShapeProperty::Operator::u;
  ctx->color     = color;
  ctx->thickness = thickness;
}

void end_union() noexcept
{
  auto ctx = UIContext::instance();
  ctx->op        = {};
  ctx->render_data.shape_properties.back().set_color(ctx->color);
  ctx->render_data.shape_properties.back().set_thickness(ctx->thickness);
  ctx->render_data.shape_properties.back().set_operator(ctx->op);
  ctx->color     = {};
  ctx->thickness = {};
}

void triangle(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color, float thickness) noexcept
{
  add_shape(ShapeProperty::Type::triangle, color, thickness, get_bounding_rectangle({ p0, p1, p2 }), { p0, p1, p2 });
}

void rectangle(glm::vec2 const& left_top, glm::vec2 const& right_bottom, uint32_t color, uint32_t thickness) noexcept
{
  add_shape(ShapeProperty::Type::rectangle, color, thickness, { left_top, right_bottom }, { left_top, right_bottom });
}

void circle(glm::vec2 const& center, float radius, uint32_t color, uint32_t thickness) noexcept
{
  add_shape(ShapeProperty::Type::circle, color, thickness, { center - radius, center + radius }, { center }, { radius });
}

}}
