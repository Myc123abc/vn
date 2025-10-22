#include "vn.hpp"
#include "renderer/renderer.hpp"
#include "renderer/window_manager.hpp"
#include "ui/ui_context.hpp"

using namespace vn::renderer;
using namespace vn::ui;

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
  Renderer::instance()->message_process();
  UIContext::instance()->message_process();
}

void render() noexcept
{
  Renderer::instance()->render_begin();
  UIContext::instance()->render();
  Renderer::instance()->render_end();
}

}
