#include "vn.hpp"
#include "renderer/renderer.hpp"

using namespace vn::renderer;

namespace vn {

void init() noexcept
{
  WindowSystem::instance()->init();
  Renderer::instance()->init();
}

void destroy() noexcept
{
  Renderer::instance()->destroy();
}

void render() noexcept
{
  Renderer::instance()->acquire_render();
}

}