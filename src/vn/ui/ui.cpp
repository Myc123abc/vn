#include "ui.hpp"
#include "../renderer/window_manager.hpp"
#include "ui_context.hpp"
#include "error_handling.hpp"
#include "lerp_animation.hpp"

#include <ranges>

using namespace vn::renderer;
using namespace vn::ui;
using namespace vn;

namespace {

inline void check_in_update_callback() noexcept { err_if(!UIContext::instance()->updating, "failed to call this function because it's not called in update callback"); }
inline void check_not_path_draw()      noexcept { err_if(UIContext::instance()->path_draw, "failed ot call this function because it cannot be used in path draw"); }

auto get_color(uint32_t color) noexcept -> glm::vec4
{
  auto r = static_cast<float>((color >> 24) & 0xFF) / 255;
  auto g = static_cast<float>((color >> 16) & 0xFF) / 255;
  auto b = static_cast<float>((color >> 8 ) & 0xFF) / 255;
  auto a = static_cast<float>((color      ) & 0xFF) / 255;
  return { r, g, b, a };
}

auto color_lerp(uint32_t x, uint32_t y, float v) noexcept
{
  auto x_color = get_color(x);
  auto y_color = get_color(y);
  auto lerp_color = glm::vec4
  {
    std::lerp(x_color.r, y_color.r, v),
    std::lerp(x_color.g, y_color.g, v),
    std::lerp(x_color.b, y_color.b, v),
    std::lerp(x_color.a, y_color.a, v)
  };
  return lerp_color;
}

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

void add_shape_property(
  ShapeProperty::Type       type,
  glm::vec4                 color,
  float                     thickness,
  std::vector<float> const& values) noexcept
{
  auto ctx = UIContext::instance();
  ctx->render_data.shape_properties.emplace_back(ShapeProperty
  {
    type,
    ctx->tmp_color.value_or(color),
    thickness,
    ctx->op_data.op,
    values
  });
  ctx->shape_properties_offset += ctx->render_data.shape_properties.back().byte_size();
}

void add_shape(
  ShapeProperty::Type                    type,
  glm::vec4                              color,
  float                                  thickness,
  std::vector<float> const&              values,
  std::pair<glm::vec2, glm::vec2> const& bounding_rectangle) noexcept
{
  auto ctx        = UIContext::instance();
  auto [min, max] = bounding_rectangle;

  if (ctx->op_data.op == ShapeProperty::Operator::u)
  {
    ctx->op_data.points.emplace_back(min);
    ctx->op_data.points.emplace_back(max);
    goto add_shape_property;
  }

  add_vertices_indices(bounding_rectangle);

add_shape_property:
  add_shape_property(type, color, thickness, values);
}

void add_shape(
  ShapeProperty::Type                    type,
  uint32_t                               color,
  float                                  thickness,
  std::vector<float> const&              values,
  std::pair<glm::vec2, glm::vec2> const& bounding_rectangle) noexcept
{ add_shape(type, get_color(color), thickness, values, bounding_rectangle); }

template <typename T>
inline void combine_hash(std::size_t& seed, const T& v) noexcept
{
  seed ^= std::hash<T>{}(v) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

template <typename... Args>
constexpr std::size_t generic_hash(const Args&... args) noexcept
{
  auto seed = std::size_t{};
  (combine_hash(seed, args), ...);
  return seed;
}

}

namespace vn { namespace ui {

////////////////////////////////////////////////////////////////////////////////
///                                Window
////////////////////////////////////////////////////////////////////////////////

void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func) noexcept
{
  UIContext::instance()->add_window(name, x, y, width, height, update_func);
}

void close_window() noexcept
{
  UIContext::instance()->close_current_window();
}

auto window_count() noexcept -> uint32_t
{
  return WindowManager::instance()->window_count();
}

auto window_extent() noexcept -> std::pair<uint32_t, uint32_t>
{
  check_in_update_callback();
  return { UIContext::instance()->window.width, UIContext::instance()->window.height };
}

void add_move_invalid_area(glm::vec2 left_top, glm::vec2 right_bottom) noexcept
{
  check_in_update_callback();
  auto render_pos = get_render_pos();
  UIContext::instance()->add_move_invalid_area(left_top + render_pos, right_bottom + render_pos);
}

auto is_active() noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->window.is_active();
}

auto is_moving() noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->window.moving;
}

auto is_resizing() noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->window.resizing;
}

auto is_maxmize() noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->window.is_maximized;
}

auto is_minimize() noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->window.is_minimized;
}

void minimize_window() noexcept
{
  check_in_update_callback();
  ShowWindow(UIContext::instance()->window.handle, SW_MINIMIZE);
}

void maximize_window() noexcept
{
  check_in_update_callback();
  PostMessageW(UIContext::instance()->window.handle, WM_SIZE, SIZE_MAXIMIZED, 0);
}

void restore_window() noexcept
{
  check_in_update_callback();
  auto ctx = UIContext::instance();
  ShowWindow(ctx->window.handle, SW_RESTORE);
  if (is_maxmize())
    PostMessageW(ctx->window.handle, static_cast<uint32_t>(WindowManager::Message::window_restore_from_maximize), 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
///                            Shape Operator
////////////////////////////////////////////////////////////////////////////////

void set_render_pos(int x, int y) noexcept
{
  check_in_update_callback();
  UIContext::instance()->set_window_render_pos(x, y);
}

auto get_render_pos() noexcept -> glm::vec2
{
  check_in_update_callback();
  return UIContext::instance()->window_render_pos();
}

void enable_tmp_color(glm::vec4 const& color) noexcept
{
  check_in_update_callback();
  UIContext::instance()->tmp_color = color;
}

void disable_tmp_color() noexcept
{
  check_in_update_callback();
  UIContext::instance()->tmp_color = {};
}

void begin_union() noexcept
{
  check_in_update_callback();
  auto ctx = UIContext::instance();
  err_if(ctx->using_union, "cannot call begin union twice");
  err_if(ctx->path_draw, "cannot call begin union operator in a path draw");
  ctx->using_union    = true;
  ctx->op_data.op     = ShapeProperty::Operator::u;
  ctx->op_data.offset = ctx->shape_properties_offset;
}

void end_union(uint32_t color, float thickness) noexcept
{
  check_in_update_callback();
  auto ctx = UIContext::instance();
  err_if(!ctx->using_union, "cannot call end union in an uncomplete unino operator");
  err_if(ctx->path_draw, "cannot call end union in an uncomplete path draw");
  ctx->using_union = false;
  ctx->render_data.shape_properties.back().set_color(ctx->tmp_color.value_or(get_color(color)));
  ctx->render_data.shape_properties.back().set_thickness(thickness);
  ctx->render_data.shape_properties.back().set_operator({});

  add_vertices_indices(get_bounding_rectangle(ctx->op_data.points));

  ctx->op_data.op     = {};
  ctx->op_data.offset = {};
  ctx->op_data.points.clear();
}

void begin_path() noexcept
{
  check_in_update_callback();

  auto ctx = UIContext::instance();
  err_if(ctx->path_draw, "cannot call begin path twice");

  ctx->path_draw = true;
  ctx->path_draw_data.push_back(std::bit_cast<float>(0u)); // record count
}

void end_path(uint32_t color, float thickness) noexcept
{
  check_in_update_callback();
  
  auto ctx = UIContext::instance();
  err_if(!ctx->path_draw, "cannot call end path in an uncomplete path draw");
  err_if(ctx->path_draw_points.empty(), "path drawing not have any data");

  add_shape(ShapeProperty::Type::path, color, thickness, ctx->path_draw_data, get_bounding_rectangle(ctx->path_draw_points));

  ctx->path_draw = {};
  ctx->path_draw_data.clear();
  ctx->path_draw_points.clear();
}

void discard_rectangle(glm::vec2 left_top, glm::vec2 right_bottom) noexcept
{
  check_in_update_callback();
  
  auto ctx = UIContext::instance();
  err_if(ctx->render_data.shape_properties.empty(), "failed must draw a shape then use discard rectangle");
  err_if(ctx->using_union, "don't use discard rectangle in union operator, I'm not test for this");
  err_if(ctx->path_draw, "don't use discard rectangle in part draw, I'm not test for this");

  auto& shape_property = ctx->render_data.shape_properties.back();
  shape_property.set_operator(ShapeProperty::Operator::discard);

  auto render_pos = ctx->window_render_pos();
  left_top     += render_pos;
  right_bottom += render_pos;

  add_shape_property(ShapeProperty::Type::rectangle, {}, {}, { left_top.x, left_top.y, right_bottom.x, right_bottom.y });
}

////////////////////////////////////////////////////////////////////////////////
///                            Basic Shape
////////////////////////////////////////////////////////////////////////////////

void triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color, float thickness) noexcept
{
  check_in_update_callback();
  check_not_path_draw();
  
  auto render_pos = UIContext::instance()->window_render_pos();
  p0 += render_pos;
  p1 += render_pos;
  p2 += render_pos;

  add_shape(ShapeProperty::Type::triangle, color, thickness, { p0.x, p0.y, p1.x, p1.y, p2.x, p2.y }, get_bounding_rectangle({ p0, p1, p2 }));
}

void rectangle(glm::vec2 left_top, glm::vec2 right_bottom, uint32_t color, float thickness) noexcept
{
  check_in_update_callback();
  check_not_path_draw();

  auto render_pos = UIContext::instance()->window_render_pos();
  left_top     += render_pos;
  right_bottom += render_pos;

  add_shape(ShapeProperty::Type::rectangle, color, thickness, { left_top.x, left_top.y, right_bottom.x, right_bottom.y }, { left_top, right_bottom });
}

void circle(glm::vec2 center, float radius, uint32_t color, float thickness) noexcept
{
  check_in_update_callback();
  check_not_path_draw();

  auto render_pos = UIContext::instance()->window_render_pos();
  center += render_pos;

  add_shape(ShapeProperty::Type::circle, color, thickness, { center.x, center.y, radius }, { center - radius, center + radius });
}

void line(glm::vec2 p0, glm::vec2 p1, uint32_t color) noexcept
{
  check_in_update_callback();

  auto ctx = UIContext::instance();

  auto render_pos = ctx->window_render_pos();
  p0 += render_pos;
  p1 += render_pos;

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

void bezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, uint32_t color) noexcept
{
  check_in_update_callback();

  auto ctx = UIContext::instance();

  auto render_pos = ctx->window_render_pos();
  p0 += render_pos;
  p1 += render_pos;
  p2 += render_pos;

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

////////////////////////////////////////////////////////////////////////////////
///                              UI Widget
////////////////////////////////////////////////////////////////////////////////

auto is_hover_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool
{
  check_in_update_callback();

  auto ctx = UIContext::instance();

  auto render_pos = ctx->window_render_pos();
  left_top     += render_pos;
  right_bottom += render_pos;

  if (!ctx->window.cursor_valid_area() || ctx->window.moving || ctx->window.resizing) return false;
  auto p = ctx->window.cursor_pos();
  return p.x >= left_top.x && p.x <= right_bottom.x && p.y >= left_top.y && p.y <= right_bottom.y;
}

auto is_click_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool
{
  check_in_update_callback();
  return UIContext::instance()->is_click_on(left_top, right_bottom);
}

auto is_hover_on(uint32_t id, glm::vec2 left_top, glm::vec2 right_bottom, LerpAnimation& lerp_anim) noexcept
{
  auto ctx = UIContext::instance();

  auto state = lerp_anim.state();
  using enum LerpAnimation::State;

  auto hovered = false;
  if (is_hover_on(left_top, right_bottom))
  {
    ctx->hovered_widget_ids.push_back(id);
    hovered = id == ctx->prev_hovered_widget_id;
  }

  if (hovered)
  {
    if (lerp_anim.is_reversed())
    {
      if (state != idle)
      {
        lerp_anim.reverse();
      }
    }
    else
    {
      if (state == idle)
        lerp_anim.start();
    }
  }
  else
  {
    if (!lerp_anim.is_reversed() && state != idle)
    {
      lerp_anim.reverse();
    }
  }

  return hovered;
}

auto button(
  uint32_t                                x,
  uint32_t                                y,
  uint32_t                                width,
  uint32_t                                height,
  uint32_t                                button_color,
  uint32_t                                button_hover_color,
  std::function<void(uint32_t, uint32_t)> icon_update_func,
  uint32_t                                icon_width,
  uint32_t                                icon_height,
  uint32_t                                icon_color,
  uint32_t                                icon_hover_color) noexcept-> bool
{
  auto ctx = UIContext::instance();

  auto id = generic_hash(++ctx->windows[ctx->window.handle].widget_count, ctx->window.handle, x, y, width, height);

  auto lerp_anim  = ctx->add_lerp_anim(id, 200);
  auto lerp_value = lerp_anim->get_lerp();

  auto left_top     = glm::vec<2, uint32_t>{ x,         y          };
  auto right_bottom = glm::vec<2, uint32_t>{ x + width, y + height };

  auto hovered = is_hover_on(id, left_top, right_bottom, *lerp_anim);

  enable_tmp_color(color_lerp(button_color, button_hover_color, lerp_value));
  ui::rectangle(left_top, right_bottom);
  disable_tmp_color();

  auto x_offset = (width  - icon_width)  / 2;
  auto y_offset = (height - icon_height) / 2;

  Tmp_Render_Pos(x + x_offset, y + y_offset)
  {
    enable_tmp_color(color_lerp(icon_color, icon_hover_color, lerp_value));
    if (icon_update_func) icon_update_func(icon_width, icon_height);
    disable_tmp_color();
  }

  return hovered && is_click_on(left_top, right_bottom);
}

}}
