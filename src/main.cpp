#include "vn/vn.hpp"
#include "vn/window/window_manager.hpp"

#include <chrono>

using namespace vn;

int main()
{
  vn::init();

  WindowManager::instance()->create_window(50, 50, 200, 200);
  WindowManager::instance()->create_window(100, 100, 500, 500);

  auto beg = std::chrono::high_resolution_clock::now();

  while (WindowManager::instance()->window_count())
  {
    vn::render();
  
    static bool ok{};
    auto now = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - beg).count();
    if (dur >= 1000 && !ok)
    {
      ok = true;
      WindowManager::instance()->create_window(150, 150, 200, 200);
    }
  }

  vn::destroy();
}