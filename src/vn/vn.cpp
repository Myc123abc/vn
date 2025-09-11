#include "vn.hpp"
#include "window/window_manager.hpp"
#include "renderer/renderer.hpp"

namespace vn {

void init() noexcept
{
  WindowManager::instance()->init();
  Renderer::instance()->init();
}

void destroy() noexcept
{
  WindowManager::instance()->destroy();
}

}