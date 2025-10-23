#pragma once

#include "../renderer/shader_type.hpp"
#include "../renderer/window.hpp"

#include <string_view>
#include <functional>
#include <unordered_map>
#include <optional>

#include <windows.h>

namespace vn { namespace renderer {

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

}}

namespace vn { namespace ui {

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

  void add_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func) noexcept;
  void close_current_window() noexcept;

  void add_move_invalid_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

  void render() noexcept;

  auto set_window_render_pos(int x, int y) noexcept { windows[window.handle].render_pos = { x, y }; }
  auto window_render_pos() noexcept { return windows[window.handle].render_pos; }

private:
  void update_cursor() noexcept;
  void update_wireframe() noexcept;

public:
  std::unordered_map<HWND, Window>  windows;
  WindowRenderData                  render_data;
  renderer::Window                  window;
  uint32_t                          shape_properties_offset{};

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

  std::optional<uint32_t> tmp_color;
};

}}
