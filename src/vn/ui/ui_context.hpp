#pragma once

#include "../renderer/shader_type.hpp"
#include "../renderer/window.hpp"
#include "lerp_animation.hpp"
#include "../hash.hpp"

#include <string_view>
#include <functional>
#include <unordered_map>
#include <optional>
#include <span>

#include <windows.h>

namespace vn { namespace renderer {

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

}}

namespace vn { namespace ui {

#define Tmp_Render_Pos(__x, __y) \
  for (auto __call_once = true; __call_once;) \
    for (auto __old_render_pos = get_render_pos(); __call_once; set_render_pos(__old_render_pos.x, __old_render_pos.y)) \
      for (set_render_pos(__x, __y); __call_once; __call_once = false)

void add_vertices_indices(std::pair<glm::vec2, glm::vec2> const& bounding_rectangle) noexcept;

void add_shape_property(renderer::ShapeProperty::Type type, glm::vec4 color, float thickness, std::vector<float> const& values) noexcept;

struct WindowRenderData
{
  std::vector<renderer::Vertex>        vertices;
  std::vector<uint16_t>                indices;
  uint16_t                             idx_beg{};
  std::vector<renderer::ShapeProperty> shape_properties;

  void clear() noexcept
  {
    vertices.clear();
    indices.clear();
    idx_beg = {};
    shape_properties.clear();
  }
};

struct Window
{
  std::function<void()> update;
  glm::vec2             render_pos{};
  uint32_t              widget_count{};
  bool                  draw_title_bar{};
  WindowRenderData      render_data{};
  bool                  need_clear{};
};

class UIContext
{
  friend LRESULT CALLBACK renderer::wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

private:
  UIContext()                           = default;
  ~UIContext()                          = default;
public:
  UIContext(UIContext const&)            = delete;
  UIContext(UIContext&&)                 = delete;
  UIContext& operator=(UIContext const&) = delete;
  UIContext& operator=(UIContext&&)      = delete;

  static auto const instance() noexcept
  {
    static UIContext instance;
    return &instance;
  }

  void add_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func, bool use_title_bar) noexcept;
  void close_current_window() noexcept;

  auto content_extent() noexcept -> std::pair<uint32_t, uint32_t>;

  void add_move_invalid_area(glm::vec2 left_top, glm::vec2 right_bottom) noexcept;

  void render() noexcept;
  void message_process() noexcept;

  auto set_window_render_pos(int x, int y) noexcept { windows[window.handle].render_pos = { x, y }; }
  auto window_render_pos() noexcept { return windows[window.handle].render_pos; }

  auto is_click_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool;

  auto add_lerp_anim(uint32_t id, uint32_t dur) noexcept -> LerpAnimation*;

  auto current_render_data() noexcept { return &windows[window.handle].render_data; }

private:
  void update_cursor()        noexcept;
  void update_wireframe()     noexcept;
  void update_window_shadow() noexcept;

  static constexpr auto Titler_Bar_Height             = 35;
  static constexpr auto Titler_Bar_Button_Width       = 46;
  static constexpr auto Titler_Bar_Button_Icon_Width  = 10;
  static constexpr auto Titler_Bar_Button_Icon_Height = 10;
  void update_title_bar() noexcept;

  void generate_render_data(vn::renderer::Window const& render_window) noexcept;

public:
  std::unordered_map<HWND, Window> windows;
  renderer::Window                 window;
  uint32_t                         shape_properties_offset{};

  struct OperatorShapeRenderData
  {
    renderer::ShapeProperty::Operator op{};
    std::vector<glm::vec2>            points{};
    uint32_t                          offset{};
  } op_data;

  bool                   path_draw{};
  std::vector<float>     path_draw_data;
  std::vector<glm::vec2> path_draw_points;

  bool updating{}; // promise ui functinos only call in update callback
  bool using_union{};

  std::optional<glm::vec4> tmp_color;

  size_t              prev_hovered_widget_id{};
  std::vector<size_t> hovered_widget_ids;

  HWND mouse_on_window{};

  HWND moving_or_resizing_finish_window{};

private:
  HWND                     _mouse_down_window{};
  std::optional<glm::vec2> _mouse_down_pos{};
  HWND                     _mouse_up_window{};
  std::optional<glm::vec2> _mouse_up_pos{};

  Timer                                     _lerp_anim_timer;
  std::unordered_map<size_t, LerpAnimation> _lerp_anims;
};

template <typename... T>
constexpr auto generic_id(T&&... args) noexcept
{
  auto ctx = UIContext::instance();
  return generic_hash(ctx->window.handle, ++ctx->windows[ctx->window.handle].widget_count, std::forward<T>(args)...);
}

}}
