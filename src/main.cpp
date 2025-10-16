#include "vn/vn.hpp"
#include "vn/renderer/window_manager.hpp"
#include "vn/util.hpp"

#include <chrono>

using namespace vn;

int main()
{
  vn::init();

  auto wm = renderer::WindowManager::instance();

  wm->create_window(50, 50, 50, 50);
  wm->create_window(75, 75, 50, 50);

  auto beg = std::chrono::steady_clock::now();
  uint32_t count{};
  uint32_t secs{};

  while (wm->window_count())
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
        wm->create_window(125, 125, 50, 50);
    }
  }

  vn::destroy();
}
