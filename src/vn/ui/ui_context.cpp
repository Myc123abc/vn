#include "ui_context.hpp"
#include "util.hpp"
#include "../renderer/window_manager.hpp"
#include "../renderer/renderer.hpp"

#include <algorithm>
#include <ranges>

using namespace vn::renderer;

namespace vn { namespace ui {

void UIContext::add_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> const& update_func) noexcept
{
  auto wm = WindowManager::instance();

  // empty window is renderer used fullscreen window for moving and resive other windows
  err_if(name.empty() || !update_func, "window name or update function cannot be empty");
  err_if(std::ranges::any_of(windows | std::views::keys,
    [&] (auto handle) { return wm->get_window_name(handle) == name; }), "duplicate window of {}", name);
  
  windows[wm->create_window(name, x, y, width, height)].update = update_func;
}

void UIContext::close_current_window() noexcept
{
  PostMessageW(window.handle, WM_CLOSE, 0, 0);
}

void UIContext::render() noexcept
{
  shape_properties_offset = {};
  for (auto& [handle, window] : windows)
  {
    this->window = WindowManager::instance()->get_window(handle);

    render_data.clear();

    updating = true;
    window.update();
    updating = false;

    err_if(op_data.op != ShapeProperty::Operator::none, "must clear operator after using finish");

    update_cursor();

    Renderer::instance()->render_window(handle, render_data);
  }
}

void UIContext::add_move_invalid_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept
{
  WindowManager::instance()->_windows[window.handle].move_invalid_area.emplace_back(x, y, width, height);
}

void UIContext::update_cursor() noexcept
{
  auto renderer = Renderer::instance();
  if (window.moving || window.resizing)
  {
    auto pos = POINT{};
    GetCursorPos(&pos);
    pos.x -= window.x;
    pos.y -= window.y;
    if (window.cursor_type != CursorType::arrow)
    {
      pos.x -= renderer->_cursors[window.cursor_type].pos.x;
      pos.y -= renderer->_cursors[window.cursor_type].pos.y;
    }
    render_data.vertices.append_range(std::vector<Vertex>
    {
      { { pos.x,      pos.y      }, { 0, 0 }, shape_properties_offset },
      { { pos.x + 32, pos.y      }, { 1, 0 }, shape_properties_offset },
      { { pos.x + 32, pos.y + 32 }, { 1, 1 }, shape_properties_offset },
      { { pos.x,      pos.y + 32 }, { 0, 1 }, shape_properties_offset },
    });
    render_data.indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(render_data.idx_beg + 0),
      static_cast<uint16_t>(render_data.idx_beg + 1),
      static_cast<uint16_t>(render_data.idx_beg + 2),
      static_cast<uint16_t>(render_data.idx_beg + 0),
      static_cast<uint16_t>(render_data.idx_beg + 2),
      static_cast<uint16_t>(render_data.idx_beg + 3),
    });
    render_data.idx_beg += 6;

    render_data.shape_properties.emplace_back(ShapeProperty{ ShapeProperty::Type::cursor });
    shape_properties_offset += render_data.shape_properties.back().byte_size();
  }
}

void UIContext::message_process() noexcept
{
  using enum MouseState;
  if (is_mouse_down())
  {
    if (mouse_state == left_button_up)
      mouse_state = left_button_down;
    else if (mouse_state == left_button_down)
      mouse_state = left_button_press;
  }
  else
    mouse_state = left_button_up;
}

}}
