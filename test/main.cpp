#include "vn.hpp"
#include "vn/timer.hpp"
#include "vn/ui/lerp_animation.hpp"
#include "vn/ui.hpp"

using namespace vn;
using namespace vn::ui;

Timer timer;

void title_bar() noexcept
{
  auto btn_width   = 46;
  auto btn_height  = 34;
  auto icon_width  = 10;
  auto icon_height = 10;

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

void render_window_1() noexcept
{
  // use title bar, move draw position under the title bar
  set_render_pos(0, 34);

  // set background
  auto [width, height] = window_extent();
  ui::rectangle({}, { width, height }, 0x282C34FF, 0);

  if (button(0, 0, 50, 50, 0xeeeeeeff, 0xff0000ff))
    info("1");
  if (button(0, 0, 50, 50, 0xeeeeeeff, 0x00ff004f))
    info("2");

  static auto timer     = Timer{};
  static auto lerp_anim = LerpAnimation{ &timer, 200 };
  static auto color_beg = 0xeeeeeeff;
  static auto color_end = 0xff0000ff;

  //ui::rectangle({}, { 100, 100 });
  //ui::lerp_color(color_beg, color_end, lerp_anim.get_lerp());

  using enum LerpAnimation::State;
  auto state = lerp_anim.state();
  if (is_hover_on({}, { 100, 100 }))
  {
    if (lerp_anim.is_reversed())
    {
      if (state != idle)
      {
        std::swap(color_beg, color_end);
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
      std::swap(color_beg, color_end);
      lerp_anim.reverse();
    }
  }

  timer.process_events();

  title_bar();
}

void render_window_2() noexcept
{
  auto [width, height] = window_extent();
  ui::rectangle({ 10, 10 }, { 30, 30 }, 0x0ff000ff, 1);

  static auto draw_circle = false;
  static auto& timer = [&] -> Timer&
  {
    static auto timer = Timer{};
    timer.add_repeat_event(1000, []
    {
      draw_circle = !draw_circle;
    });
    return timer;
  }();

  if (draw_circle) ui::circle({ 40, 40 }, 20, 0x00ff00ff, 1);

  timer.process_events();
  
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

  auto fps_count = uint32_t{};

  timer.add_repeat_event(1000, [&]
  {
    info("[fps] {}", fps_count);
    fps_count = {};
  });
  //timer.add_single_event(3000, [&]
  //{
  //  ui::create_window("third window", 300, 300, 100, 100, render_window_3);
  //});

  while (ui::window_count())
  {
    vn::message_process();
    vn::render();

    ++fps_count;
    timer.process_events();
  }

  vn::destroy();
}
