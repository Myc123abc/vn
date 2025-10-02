#include "vn.hpp"
#include "renderer/renderer.hpp"
#include "renderer/window_manager.hpp"

using namespace vn::renderer;

namespace vn {

void init() noexcept
{
  WindowManager::instance()->init();
  Renderer::instance()->init();
}

void destroy() noexcept
{
  Renderer::instance()->destroy();
  WindowManager::instance()->destroy();
}

void render() noexcept
{
  Renderer::instance()->acquire_render();
}

}