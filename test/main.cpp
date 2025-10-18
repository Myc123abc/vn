#include "vn/vn.hpp"
#include "vn/util.hpp"
#include "vn/ui/ui.hpp"

#include <chrono>

// TODO: test hide window

using namespace vn;
using namespace vn::ui;

int main()
{
  vn::init();

  ui::create_window("first window", 100, 100, 100, 100);
  ui::create_window("second window", 200, 200, 100, 100);

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
        ui::create_window("third window", 300, 300, 100, 100);
    }
  }

  vn::destroy();
}
