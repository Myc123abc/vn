#include "vn.hpp"
#include "vn/util.hpp"
#include "vn/ui.hpp"

#include <chrono>

using namespace vn;
using namespace vn::ui;

uint32_t secs;

void render_window_1() noexcept
{
  auto [width, height] = window_extent();
  ui::rectangle({}, { width, height }, 0x282C34FF, 0);
  ui::rectangle({ 10, 10 }, { width - 40, height - 30 }, 0xff00004f, 0);
  ui::rectangle({ 20, 10 }, { width - 30, height - 30 }, 0x0000ff4f, 0);
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
  ui::rectangle({ 10, 10 }, { 30, 30 }, 0x0ff0f0ff, 4);

  ui::begin_union(0x00ff00ff, 1);
  ui::circle({ 40, 40 }, 20);
  ui::circle({ 30, 40 }, 20);
  ui::end_union();
}

int main()
{
  vn::init();

  ui::create_window("first window", 100, 100, 100, 100, render_window_1);
  ui::create_window("second window", 200, 200, 100, 100, render_window_2);

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

      if (secs == 3)
        ui::create_window("third window", 300, 300, 100, 100, render_window_3);
    }
  }

  vn::destroy();
}
