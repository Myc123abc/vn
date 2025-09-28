#include "vn/vn.hpp"
#include "vn/renderer/window_system.hpp"

#include <thread>

using namespace vn;

int main()
{
  vn::init();

  auto window_system = renderer::WindowSystem::instance();

  window_system->create_window(50, 50, 100, 100);
  window_system->create_window(75, 75, 125, 125);

  std::thread([&]
  { 
    std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::seconds(1));
    window_system->create_window(150, 150, 250, 250);
  }).detach();

  while (!(GetKeyState(VK_SPACE) & 0x8000))
  {
    vn::render();
    std::this_thread::yield();
  }

  vn::destroy();
}