#include "vn.hpp"
#include "vn/timer.hpp"
#include "vn/ui.hpp"

using namespace vn;
using namespace vn::ui;

Timer timer;

void render_window_1() noexcept
{

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
  set_background_color(0xffffffff);

  if (ui::button(0, 0, 50, 50, 0xff0000ff, 0x0000ffff))
    info("1");
  if (ui::button(0, 0, 50, 50, 0xffffffff, 0xff0000ff))
    info("2");

  ui::circle({25,25}, 25, 0x0000ffff);

  ui::line({0, 60}, {100,60}, 0x0000ffff);
  ui::line({0, 60}, {100,60}, 0xff0000ff);

  ui::rectangle({ 60, 0 }, { 110, 50 }, 0x0000ffff);
  ui::rectangle({ 80, 0 }, { 130, 50 }, 0xff0000ff);
}

int main()
{
  vn::init();

  ui::create_window("first window", 100, 100, 200, 100, render_window_1, false);
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
