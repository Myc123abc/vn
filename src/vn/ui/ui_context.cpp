#include "ui_context.hpp"
#include "error_handling.hpp"
#include "../renderer/window_manager.hpp"
#include "../renderer/renderer.hpp"
#include "ui.hpp"

#include <algorithm>
#include <ranges>

using namespace vn::renderer;

namespace {

auto point_on_rect(glm::vec<2, int> const& p, glm::vec2 const& left_top, glm::vec2 const& right_bottom) noexcept
{
  return p.x >= left_top.x && p.x <= right_bottom.x && p.y >= left_top.y && p.y <= right_bottom.y;
}

}

namespace vn { namespace ui {

void UIContext::add_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::function<void()> update_func, bool use_title_bar) noexcept
{
  auto wm = WindowManager::instance();

  // empty window is renderer used fullscreen window for moving and resive other windows
  err_if(name.empty() || !update_func, "window name or update function cannot be empty");
  err_if(std::ranges::any_of(windows | std::views::keys,
    [&] (auto handle) { return wm->get_window_name(handle) == name; }), "duplicate window of {}", name);
  
  auto handle = wm->create_window(name, x, y, width, height);
  windows[handle].update         = update_func;
  windows[handle].draw_title_bar = use_title_bar;
}

void UIContext::close_current_window() noexcept
{
  PostMessageW(window.handle, WM_CLOSE, 0, 0);
}

auto UIContext::content_extent() noexcept -> std::pair<uint32_t, uint32_t>
{
  auto width  = window.width;
  auto height = window.height;
  if (windows[window.handle].draw_title_bar)
    height -= Titler_Bar_Height;
  return { width, height };
}

void UIContext::render() noexcept
{
  auto has_rendering = false;
  shape_properties_offset = {};
  hovered_widget_ids.clear();

  for (auto& [handle, window] : windows)
  {
    this->window = WindowManager::instance()->get_window(handle);
    if (this->window.is_minimized) break;

    has_rendering = true;

    render_data.clear();

    updating = true;

    window.widget_count = {};

    // use title bar, move draw position under the title bar
    if (window.draw_title_bar)
      set_render_pos(0, Titler_Bar_Height);
    window.update();

    err_if(op_data.op != ShapeProperty::Operator::none, "must clear operator after using finish");

    if (window.draw_title_bar)
      update_title_bar();
    if (!this->window.is_maximized)
      update_wireframe();

    updating = false;

    update_cursor();

    Renderer::instance()->render_window(handle, render_data);
  }

  if (has_rendering)
  {
    if (!hovered_widget_ids.empty())
      prev_hovered_widget_id = hovered_widget_ids.back();

    _lerp_anim_timer.process_events();
  }
  else
    Sleep(1); // FIXME: any better way?
}

void UIContext::add_move_invalid_area(glm::vec2 left_top, glm::vec2 right_bottom) noexcept
{
  WindowManager::instance()->_windows[window.handle].move_invalid_area.emplace_back(left_top.x, left_top.y, right_bottom.x, right_bottom.y);
}

void UIContext::update_cursor() noexcept
{
  auto renderer = Renderer::instance();
  if (window.moving || window.resizing)
  {
    auto pos = get_cursor_pos();
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

void UIContext::update_wireframe() noexcept
{
  Tmp_Render_Pos(0, 0)
  {
    ui::rectangle({}, { window.width, window.height }, 0xbbbbbbff, 1);
  }
}

void UIContext::update_title_bar() noexcept
{
  auto btn_width   = Titler_Bar_Button_Width;
  auto btn_height  = Titler_Bar_Height;
  auto icon_width  = Titler_Bar_Button_Icon_Width;
  auto icon_height = Titler_Bar_Button_Icon_Height;

  uint32_t background_colors[2] = { 0xffffffff, 0xeeeeeeff };
  auto i = is_active() || is_moving() || is_resizing();

  auto [w, h] = window_extent();

  Tmp_Render_Pos(0, 0)
  {
    ui::rectangle({}, { w, btn_height }, background_colors[i]);
    ui::add_move_invalid_area({ 0, btn_height }, { w, h });

    // minimize button
    if (button(w - btn_width * 3, 0, btn_width, btn_height, background_colors[i], 0x0cececeff,
      [] (uint32_t width, uint32_t height) { ui::line({ 0, height / 2 }, { width, height / 2 }); },
      icon_width, icon_height, 0x395063ff, 0x395063ff))
      minimize_window();

    // maximize / restore button
    if (button(w - btn_width * 2, 0, btn_width, btn_height, background_colors[i], 0x0cececeff, 
      [&] (uint32_t width, uint32_t height)
      {
        if (is_maxmize())
        {
          auto padding_x = width / 3;
          auto padding_y = width / 3;
          ui::rectangle({ padding_x, 0 }, { width, height - padding_y }, 0, 1);
          ui::discard_rectangle({ 0, padding_y }, { width - padding_x, height });
          ui::rectangle({ 0, padding_y }, { width - padding_x, height }, 0, 1);
        }
        else
          ui::rectangle({}, { width, height }, 0, 1);
      },
      icon_width, icon_height, 0x395063ff, 0x395063ff))
      is_maxmize() ? restore_window() : maximize_window();

    // close button
    if (button(w - btn_width, 0, btn_width, btn_height, background_colors[i], 0xeb1123ff,
      [] (uint32_t width, uint32_t height)
      {
        ui::line({}, { width, height });
        ui::line({ width, 0 }, { 0, height });
      }, icon_width, icon_height, 0x395063ff, 0xffffffff))
      close_window();

    ui::add_move_invalid_area({ w - btn_width * 3, 0 }, { w, btn_width });
  }
}

void UIContext::message_process() noexcept
{
  auto wm = WindowManager::instance();

  if (_mouse_up_window)
  {
    _mouse_down_window = {};
    _mouse_down_pos    = {};
    _mouse_up_window   = {};
    _mouse_up_pos      = {};
  }

  for (auto const& [handle, _] : windows)
  {
    auto const& window = wm->_windows[handle];
    if (window.mouse_state == MouseState::left_button_down)
    {
      _mouse_down_window = handle;
      _mouse_down_pos    = window.cursor_pos();
    }
    else if (window.mouse_state == MouseState::left_button_up)
    {
      _mouse_up_window = handle;
      _mouse_up_pos    = window.cursor_pos();
    }
  }
}

auto UIContext::is_click_on(glm::vec2 left_top, glm::vec2 right_bottom) noexcept -> bool
{
  auto render_pos = window_render_pos();
  left_top     += render_pos;
  right_bottom += render_pos;

  if (!window.is_active()          ||
      !window.cursor_valid_area()  ||
      window.moving                ||
      window.resizing              ||
      !_mouse_down_pos.has_value() ||
      !_mouse_up_pos.has_value()   ||
      _mouse_down_window != _mouse_up_window) return false;
  return point_on_rect(_mouse_down_pos.value(), left_top, right_bottom) &&
         point_on_rect(_mouse_up_pos.value(),   left_top, right_bottom);
}

auto UIContext::add_lerp_anim(uint32_t id, uint32_t dur) noexcept -> LerpAnimation*
{
  if (!_lerp_anims.contains(id))
    _lerp_anims[id].init(&_lerp_anim_timer, dur);
  return &_lerp_anims[id];
}

}}
