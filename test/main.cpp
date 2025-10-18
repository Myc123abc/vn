#include "vn.hpp"
#include "vn/util.hpp"
#include "vn/ui.hpp"

#include <chrono>

using namespace vn;
using namespace vn::ui;

void render_window_1() noexcept
{
  auto [width, height] = window_extent();
  ui::triangle({}, { width, 0 }, { 0, height }, 0xff0000ff);
}

void render_window_2() noexcept
{
  auto [width, height] = window_extent();
  ui::triangle({}, { width, height / 2 }, { 0, height }, 0x00ff00ff);
}

void render_window_3() noexcept
{
  auto [width, height] = window_extent();
  ui::triangle({}, { width, height }, { 0, height }, 0x0000ffff);
}

int main()
{
  vn::init();

  ui::create_window("first window", 100, 100, 100, 100, render_window_1);
  ui::create_window("second window", 200, 200, 100, 100, render_window_2);

  auto beg = std::chrono::steady_clock::now();
  uint32_t count{};
  uint32_t secs{};

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
