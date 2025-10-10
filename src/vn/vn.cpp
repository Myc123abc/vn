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
}

void message_process() noexcept
{
  WindowManager::instance()->message_process();
}

void render() noexcept
{
  Renderer::instance()->run();
}

}