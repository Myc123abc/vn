#include "vn.hpp"
#include "vn/timer.hpp"
#include "vn/ui.hpp"

using namespace vn;
using namespace vn::ui;

Timer timer;

void render_window_1() noexcept
{
  set_background_color(0xffffffff);

  //ui::line({0, 25}, {100,25}, 0x0000ffff);
  //ui::line({0, 25}, {100,25}, 0xff0000ff);

  if (ui::button(0, 0, 50, 50, 0xff0000ff, 0x0000ffff))
    info("1");
  ////ui::circle({25,25}, 25, 0x0000ffff);
  if (ui::button(0, 0, 50, 50, 0xffffffff, 0x00ff00ff))
    info("2");
}

void render_window_2() noexcept
{
  set_background_color(0x282C34FF);

  auto [width, height] = content_extent();
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
  set_background_color(0x000000ff);

  auto [width, height] = content_extent();
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
