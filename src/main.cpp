#include "vn/vn.hpp"
#include "vn/renderer/window_manager.hpp"

#include <thread>

using namespace vn;

int main()
{
  vn::init();

  auto wm = renderer::WindowManager::instance();

  wm->create_window(50, 50, 100, 100);
  wm->create_window(75, 75, 125, 125);

  std::thread([&]
  { 
    std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::seconds(1));
    wm->create_window(150, 150, 250, 250);
  }).detach();

  while (wm->window_count())
  {
    vn::render();
    std::this_thread::yield(); 
  } 

  vn::destroy();
}