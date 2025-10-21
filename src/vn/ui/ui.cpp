#include "ui.hpp"
#include "../renderer/window_manager.hpp"
#include "ui_context.hpp"
#include "util.hpp"

#include <ranges>

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

void add_vertices_indices(std::pair<glm::vec2, glm::vec2> const& bounding_rectangle)
{
  auto ctx        = UIContext::instance();
  auto [min, max] = bounding_rectangle;

  if (min.x > 0)                  --min.x;
  if (min.y > 0)                  --min.y;
  if (max.x < ctx->window.width)  ++max.x;
  if (max.y < ctx->window.height) ++max.y;

  auto offset = ctx->op_data.op == ShapeProperty::Operator::none ? ctx->shape_properties_offset : ctx->op_data.offset;

  ctx->render_data.vertices.append_range(std::vector<Vertex>
  {
    { min,              {}, offset },
    { { max.x, min.y }, {}, offset },
    { max,              {}, offset },
    { { min.x, max.y }, {}, offset },
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
}

void add_shape(
  ShapeProperty::Type                    type,
  uint32_t                               color,
  float                                  thickness,
  std::vector<float> const&              values,
  std::pair<glm::vec2, glm::vec2> const& bounding_rectangle) noexcept
{
  auto ctx        = UIContext::instance();
  auto [min, max] = bounding_rectangle;

  if (ctx->op_data.op != ShapeProperty::Operator::none)
  {
    ctx->op_data.points.emplace_back(min);
    ctx->op_data.points.emplace_back(max);
    goto add_shape_property;
  }

  add_vertices_indices(bounding_rectangle);

add_shape_property:
  ctx->render_data.shape_properties.emplace_back(ShapeProperty
  {
    type,
    color,
    thickness,
    ctx->op_data.op,
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

void begin_union() noexcept
{
  auto ctx = UIContext::instance();
  ctx->op_data.op     = ShapeProperty::Operator::u;
  ctx->op_data.offset = ctx->shape_properties_offset;
}

void end_union(uint32_t color, float thickness) noexcept
{
  auto ctx = UIContext::instance();
  ctx->render_data.shape_properties.back().set_color(color);
  ctx->render_data.shape_properties.back().set_thickness(thickness);
  ctx->render_data.shape_properties.back().set_operator({});

  add_vertices_indices(get_bounding_rectangle(ctx->op_data.points));

  ctx->op_data.op     = {};
  ctx->op_data.offset = {};
  ctx->op_data.points.clear();
}

void begin_path() noexcept
{
  auto ctx = UIContext::instance();
  ctx->path_draw = true;
  ctx->path_draw_data.push_back(std::bit_cast<float>(0u)); // record count
}

void end_path(uint32_t color, float thickness) noexcept
{
  auto ctx = UIContext::instance();

  err_if(ctx->path_draw_points.empty(), "path drawing not have any data");

  add_shape(ShapeProperty::Type::path, color, thickness, ctx->path_draw_data, get_bounding_rectangle(ctx->path_draw_points));

  ctx->path_draw = {};
  ctx->path_draw_data.clear();
  ctx->path_draw_points.clear();
}

void triangle(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color, float thickness) noexcept
{
  err_if(UIContext::instance()->path_draw, "path draw cannot use on triangle");
  add_shape(ShapeProperty::Type::triangle, color, thickness, { p0.x, p0.y, p1.x, p1.y, p2.x, p2.y }, get_bounding_rectangle({ p0, p1, p2 }));
}

void rectangle(glm::vec2 const& left_top, glm::vec2 const& right_bottom, uint32_t color, float thickness) noexcept
{
  err_if(UIContext::instance()->path_draw, "path draw cannot use on rectangle");
  add_shape(ShapeProperty::Type::rectangle, color, thickness, { left_top.x, left_top.y, right_bottom.x, right_bottom.y }, { left_top, right_bottom });
}

void circle(glm::vec2 const& center, float radius, uint32_t color, float thickness) noexcept
{
  err_if(UIContext::instance()->path_draw, "path draw cannot use on circle");
  add_shape(ShapeProperty::Type::circle, color, thickness, { center.x, center.y, radius }, { center - radius, center + radius });
}

void line(glm::vec2 const& p0, glm::vec2 const& p1, uint32_t color) noexcept
{
  auto ctx = UIContext::instance();
  if (ctx->path_draw)
  {
    ctx->path_draw_data[0] = std::bit_cast<float>(std::bit_cast<uint32_t>(ctx->path_draw_data[0]) + 1);
    auto points = { p0, p1 };
    ctx->path_draw_points.append_range(points);
    ctx->path_draw_data.emplace_back(std::bit_cast<float>(ShapeProperty::Type::path_line));
    ctx->path_draw_data.append_range(std::ranges::to<std::vector<float>>(points
      | std::views::transform([](auto const& p) { return std::array<float, 2>{ p.x, p.y }; })
      | std::views::join));
  }
  else
    add_shape(ShapeProperty::Type::line, color, {}, { p0.x, p0.y, p1.x, p1.y }, get_bounding_rectangle({ p0, p1 }));
}

void bezier(glm::vec2 const& p0, glm::vec2 const& p1, glm::vec2 const& p2, uint32_t color) noexcept
{
  auto ctx = UIContext::instance();
  if (ctx->path_draw)
  {
    ctx->path_draw_data[0] = std::bit_cast<float>(std::bit_cast<uint32_t>(ctx->path_draw_data[0]) + 1);
    auto points = { p0, p1, p2 };
    ctx->path_draw_points.append_range(points);
    ctx->path_draw_data.emplace_back(std::bit_cast<float>(ShapeProperty::Type::path_bezier));
    ctx->path_draw_data.append_range(std::ranges::to<std::vector<float>>(points
      | std::views::transform([](auto const& p) { return std::array<float, 2>{ p.x, p.y }; })
      | std::views::join));
  }
  else
    add_shape(ShapeProperty::Type::bezier, color, {}, { p0.x, p0.y, p1.x, p1.y, p2.x, p2.y }, get_bounding_rectangle({ p0, p1, p2 }));
}

}}
