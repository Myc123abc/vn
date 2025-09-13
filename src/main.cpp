#include "vn/vn.hpp"
#include "vn/window/window_manager.hpp"

using namespace vn;

int main()
{
  vn::init();

  WindowManager::instance()->create_window(50, 50, 200, 200);
  WindowManager::instance()->create_window(100, 100, 500, 500);

  while (WindowManager::instance()->window_count())
  {
    vn::render();
  
    static bool ok{};
    if (!ok)
    {
      ok = true;
      WindowManager::instance()->create_window(150, 150, 200, 200);
    }

    Sleep(1000 / 180);
  }

  vn::destroy();
}