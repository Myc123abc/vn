#pragma once

#include "../renderer/shader_type.hpp"
#include "../renderer/window.hpp"

#include <string_view>
#include <functional>
#include <unordered_map>

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

  std::vector<glm::vec<2, uint32_t>> click_areas;

  void clear() noexcept
  {
    click_areas.clear();
  }
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

  void add_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> const& update_func) noexcept;
  void close_current_window() noexcept;

  void message_process() noexcept;
  void render() noexcept;

private:
  void update_cursor() noexcept;

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

  enum class MouseState
  {
    left_button_up,
    left_button_down,
    left_button_press,
  } mouse_state;
};

}}
