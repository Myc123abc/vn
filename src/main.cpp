#include "vn/vn.hpp"
#include "vn/window/window_manager.hpp"

using namespace vn;

int main()
{
  vn::init();

  WindowManager::instance()->create_window(50, 50, 200, 200);
  WindowManager::instance()->create_window(100, 100, 200, 200);

  while (WindowManager::instance()->window_count())
  {
    Sleep(1);
  }

  vn::destroy();
}