#include "vn/vn.hpp"
#include "vn/renderer/window_system.hpp"

#include <chrono>

using namespace vn;

int main()
{
  vn::init();

  auto window_system = renderer::WindowSystem::instance();

  window_system->create_window(50, 50, 100, 100);
  window_system->create_window(75, 75, 125, 125);

  auto beg = std::chrono::high_resolution_clock::now();

  while (!(GetKeyState(VK_SPACE) & 0x8000))
  {
    vn::render();

    static bool ok{};
    auto now = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - beg).count();
    if (dur >= 1000 && !ok)
    {
      ok = true;
      window_system->create_window(150, 150, 250, 250);
    }

    Sleep(1);
  }

  vn::destroy();
}