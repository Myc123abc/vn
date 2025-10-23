#include "vn.hpp"
#include "vn/util.hpp"
#include "vn/ui.hpp"

#include <chrono>
#include <functional>

using namespace vn;
using namespace vn::ui;

uint32_t secs;

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
  uint32_t                                icon_hover_color) noexcept
{
  auto left_top     = glm::vec<2, uint32_t>{ x,         y          };
  auto right_bottom = glm::vec<2, uint32_t>{ x + width, y + height };

  uint32_t button_colors[2] = { button_color, button_hover_color };
  uint32_t icon_colors[2]   = { icon_color,   icon_hover_color   };

  auto hovered = is_hover_on(left_top, right_bottom); // TODO: soft color exchange, color line change with a little time

  ui::rectangle(left_top, right_bottom, button_colors[hovered]);
  ui::move_invalid_area(left_top.x, left_top.y, right_bottom.x, right_bottom.y); // TODO: every button add area will be so much!!

  auto x_offset = (width  - icon_width)  / 2;
  auto y_offset = (height - icon_height) / 2;

  Tmp_Render_Pos(x + x_offset, y + y_offset)
  {
    enable_tmp_color(icon_colors[hovered]);
    if (icon_update_func) icon_update_func(icon_width, icon_height);
    disable_tmp_color();
  }

  return is_click_on(left_top, right_bottom);
}

void title_bar(uint32_t height) noexcept
{
  Tmp_Render_Pos(0, 0)
  {
    auto width = 46.f / 34 * height;
    
    uint32_t background_colors[2] = { 0xffffffff, 0xeeeeeeff };
    auto i = is_active() || is_moving() || is_resizing();

    auto [w, h] = window_extent();
    ui::rectangle({}, { w, height }, background_colors[i]);
    ui::move_invalid_area(0, height, w, h);

    // minimize button
    if (button(w - width * 3, 0, width, height, background_colors[i], 0x0cececeff,
      [] (uint32_t width, uint32_t height) { ui::line({ 0, height / 2 }, { width, height / 2 }); },
      10, 10, 0x395063ff, 0x395063ff))
      minimize_window();

    // maximize button
    if (button(w - width * 2, 0, width, height, background_colors[i], 0x0cececeff, 
      [] (uint32_t width, uint32_t height) { ui::rectangle({}, { width, height }, 0, 1); },
      10, 10, 0x395063ff, 0x395063ff))
      maximize_window();

    // close button
    if (button(w - width, 0, width, height, background_colors[i], 0xeb1123ff,
      [] (uint32_t width, uint32_t height)
      {
        ui::line({}, { width, height });
        ui::line({ width, 0 }, { 0, height });
      }, 10, 10, 0x395063ff, 0xffffffff))
      close_window();
  }
}

void render_window_1() noexcept
{
  // use title bar, move draw position under the title bar
  set_render_pos(0, 34);

  // set background
  auto [width, height] = window_extent();
  ui::rectangle({}, { width, height }, 0x282C34FF, 0);

  title_bar(34);
}

void render_window_2() noexcept
{
  auto [width, height] = window_extent();
  ui::rectangle({ 10, 10 }, { 30, 30 }, 0x0ff000ff, 1);
  if (secs < 2)
    ui::circle({ 40, 40 }, 20, 0x00ff00ff, 1);
  ui::triangle({}, { width, height / 2 }, { 0, height }, 0x00ff00ff, 1);
}

void render_window_3() noexcept
{
  auto [width, height] = window_extent();
  ui::rectangle({ 10, 10 }, { 30, 30 }, 0x0ff0f0ff, 1);

  ui::begin_union();
  ui::circle({ 50, 40 }, 20);
  ui::circle({ 30, 40 }, 20);
  ui::end_union(0x00ff00ff, 4);

  ui::begin_union();
  ui::circle({ 50, 60 }, 20);
  ui::circle({ 30, 60 }, 20);
  ui::end_union(0x00ff004f, 1);

  ui::begin_union();
  ui::circle({ 50, 70 }, 20);
  ui::circle({ 30, 80 }, 20);
  ui::end_union(0x00ff0fff);

  ui::rectangle({ 60, 10 }, { 80, 30 }, 0x0ff0f0ff, 4);
}

int main()
{
  vn::init();

  ui::create_window("first window", 100, 100, 200, 100, render_window_1);
  //ui::create_window("second window", 200, 200, 100, 100, render_window_2);

  auto beg = std::chrono::steady_clock::now();
  uint32_t count{};

  while (ui::window_count())
  {
    vn::message_process();
    vn::render();

    ++count;
    auto now = std::chrono::steady_clock ::now();
    auto dur = std::chrono::duration<float>(now - beg).count();
    if (dur >= 1.f)
    {
      ++secs;
      info("[fps] {}", count / dur);
      count = 0;
      beg = now;

      //if (secs == 3)
      //  ui::create_window("third window", 300, 300, 100, 100, render_window_3);
    }
  }

  vn::destroy();
}
