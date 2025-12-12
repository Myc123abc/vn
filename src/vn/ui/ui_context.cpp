#include "ui_context.hpp"
#include "error_handling.hpp"
#include "../renderer/window_manager.hpp"
#include "../renderer/renderer.hpp"
#include "ui.hpp"

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
  // clear last frame hovered widget ids
  hovered_widget_ids.clear();

  // get unminimized windows as render targets
  auto render_windows = WindowManager::instance()->_windows
    | std::views::values
    | std::views::filter([](auto const& window) { return !window.is_minimized; });

  // generate render data
  std::ranges::for_each(render_windows, [this](auto const& render_window) { generate_render_data(render_window); });

  // if have any rendering window
  if (!render_windows.empty())
  {
    auto renderer = Renderer::instance();

    // commit render commands
    auto need_clear_window     = HWND{};
    auto use_fullscreen_window = HWND{};
    for (auto const& render_window : render_windows)
    {
			auto& window = windows[render_window.handle];

      // for moving or resizing window, use fullscreen promise smooth winodw size change and move sync with cursor
      if (render_window.is_moving_or_resizing())
      {
        use_fullscreen_window = render_window.handle;

        // use fullscreen render first frame need to clear window content
        if (window.need_clear)
        {
          need_clear_window = render_window.handle;
					renderer->clear_window(render_window.handle);
        }

        // use window render data to render on fullscreen
        renderer->render_fullscreen(render_window.handle, window.render_data);
      }
      else
        renderer->render(render_window.handle, window.render_data);

      // clear render data
      window.render_data.clear();
    }

    if (moving_or_resizing_finish_window) renderer->clear_fullscreen();

    // present windows
    // at least one vsync present promise all window vsync support present barrier
    if (use_fullscreen_window)
    {
      if (need_clear_window)
        renderer->present(need_clear_window);
      std::ranges::for_each(render_windows | std::views::filter([&](auto const& window) { return window.handle != use_fullscreen_window; }),
        [&](auto const& window) { renderer->present(window.handle); });
      renderer->present_fullscreen(true);
    }
    else
    {
      // HACK:
      // I can't implement two window present at same monitor frame beside nvidia which NvApi 
      // intel and amd driver I'm not see any api support present barrier
      // so this process which present two windows, mostly it's ok
      // but some pc's performance problem can be cause two windows present in differet monitor frame
      // so the filcker will happen
      if (moving_or_resizing_finish_window)
      {
        std::ranges::for_each(render_windows | std::views::filter([&](auto const& window) { return window.handle != moving_or_resizing_finish_window; }),
          [&](auto const& window) { renderer->present(window.handle); });
        renderer->present_fullscreen();
        renderer->present(moving_or_resizing_finish_window, true);
        moving_or_resizing_finish_window = {};
      }
      else
      {
        std::ranges::for_each(render_windows | std::views::take(std::ranges::distance(render_windows) - 1),
          [&](auto const& window) { renderer->present(window.handle); });
        std::ranges::for_each(render_windows | std::views::reverse | std::views::take(1),
          [&](auto const& window) { renderer->present(window.handle, true); });
      }
    }

    // update mouse hovered widget id
    if (!hovered_widget_ids.empty())
      prev_hovered_widget_id = hovered_widget_ids.back();

    // timer events process
    _lerp_anim_timer.process_events();
  }
  else
    Sleep(1); // FIXME: any better way?
}

void UIContext::generate_render_data(vn::renderer::Window const& render_window) noexcept
{
  // initialize data per window
  auto& window = windows[render_window.handle];
  this->window            = render_window;
  shape_properties_offset = {};
  updating                = true;
  window.widget_count     = {};
  op_data.offset          = {};

  // use title bar, move draw position under the title bar
  if (window.draw_title_bar)
    set_render_pos(0, Titler_Bar_Height);

  // update render data from user render callback
  window.update();

  // promise last shape is normal operator
  err_if(op_data.op != ShapeProperty::Operator::none, "must clear operator after using finish");

  // draw title bar
  if (window.draw_title_bar)
    update_title_bar();

  update_window_shadow();

  update_cursor();

  // update render data finish
  updating = false;
}

void UIContext::add_move_invalid_area(glm::vec2 left_top, glm::vec2 right_bottom) noexcept
{
  WindowManager::instance()->_windows.at(window.handle).move_invalid_area.emplace_back(left_top.x, left_top.y, right_bottom.x, right_bottom.y);
}

void UIContext::update_cursor() noexcept
{
  auto renderer    = Renderer::instance();
  auto render_data = current_render_data();
  if (window.is_moving_or_resizing())
  {
    auto pos = window.cursor_pos();
    if (window.cursor_type != CursorType::arrow)
    {
      pos.x -= renderer->_cursors[window.cursor_type].pos.x;
      pos.y -= renderer->_cursors[window.cursor_type].pos.y;
    }
    render_data->vertices.append_range(std::vector<Vertex>
    {
      { { pos.x,      pos.y,      0.f }, { 0, 0 }, shape_properties_offset },
      { { pos.x + 32, pos.y,      0.f }, { 1, 0 }, shape_properties_offset },
      { { pos.x + 32, pos.y + 32, 0.f }, { 1, 1 }, shape_properties_offset },
      { { pos.x,      pos.y + 32, 0.f }, { 0, 1 }, shape_properties_offset },
    });
    render_data->indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(render_data->idx_beg + 0),
      static_cast<uint16_t>(render_data->idx_beg + 1),
      static_cast<uint16_t>(render_data->idx_beg + 2),
      static_cast<uint16_t>(render_data->idx_beg + 0),
      static_cast<uint16_t>(render_data->idx_beg + 2),
      static_cast<uint16_t>(render_data->idx_beg + 3),
    });
    render_data->idx_beg += 4;

    render_data->shape_properties.emplace_back(ShapeProperty{ ShapeProperty::Type::cursor });
    shape_properties_offset += render_data->shape_properties.back().byte_size();
  }
}

void UIContext::update_window_shadow() noexcept
{
	// ui::rectangle({}, { window.real_width(), window.real_height() });
  //add_vertices_indices({{ rect.left, rect.top }, { rect.right, rect.bottom }});
  //add_shape_property(ShapeProperty::Type::rectangle, {}, {}, { static_cast<float>(window.rect.left), static_cast<float>(window.rect.top), static_cast<float>(window.rect.right), static_cast<float>(window.rect.bottom) });
  // current_render_data()->shape_properties.back().set_flags(ShapeProperty::Flag::window_shadow);
}

void UIContext::update_title_bar() noexcept
{
  auto btn_width   = Titler_Bar_Button_Width;
  auto btn_height  = Titler_Bar_Height;
  auto icon_width  = Titler_Bar_Button_Icon_Width;
  auto icon_height = Titler_Bar_Button_Icon_Height;

  uint32_t background_colors[2] = { 0xffffffff, 0xeeeeeeff };
  auto i = is_active() || is_moving() || is_resizing();

  auto background_color = color_lerp(background_colors[0], background_colors[1], add_lerp_anim(generic_id("__update_title_bar"), 200)->update(i).get_lerp());

  auto [w, h] = window_extent();

  Tmp_Render_Pos(0, 0)
  {
    ui::rectangle({}, { w, btn_height }, background_color);
    ui::add_move_invalid_area({ 0, btn_height }, { w, h });

    // minimize button
    if (button(w - btn_width * 3, 0, btn_width, btn_height, background_color, 0x0cececeff,
      [] (uint32_t width, uint32_t height) { ui::line({ 0, height / 2 }, { width, height / 2 }); },
      icon_width, icon_height, 0x395063ff, 0x395063ff))
      minimize_window();

    // maximize / restore button
    if (button(w - btn_width * 2, 0, btn_width, btn_height, background_color, 0x0cececeff,
      [&] (uint32_t width, uint32_t height)
      {
        if (is_maxmized())
        {
          auto padding_x = width / 5;
          auto padding_y = width / 5;
          ui::rectangle({ padding_x, 0 }, { width, height - padding_y }, 0, 1);
          ui::discard_rectangle({ 0, padding_y }, { width - padding_x, height });
          ui::rectangle({ 0, padding_y }, { width - padding_x, height }, 0, 1);
        }
        else
          ui::rectangle({}, { width, height }, 0, 1);
      },
      icon_width, icon_height, 0x395063ff, 0x395063ff))
      is_maxmized() ? restore_window() : maximize_window();

    // close button
    if (button(w - btn_width, 0, btn_width, btn_height, background_color, 0xeb1123ff,
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

  auto z_orders = wm->get_window_z_orders();
  if (auto it = std::ranges::find_if(z_orders, [wm](auto handle) { return wm->_windows.at(handle).point_on(get_cursor_pos()); });
      it != z_orders.end())
    mouse_on_window = *it;
  else
    mouse_on_window = {};

  for (auto const& [handle, _] : windows)
  {
    auto const& window = wm->_windows.at(handle);
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

  if (!window.is_active()             ||
      !window.cursor_valid_area()     ||
		   window.is_moving_or_resizing() ||
      !_mouse_down_pos.has_value()    ||
      !_mouse_up_pos.has_value()      ||
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
