#include "ui/ui.hpp"
#include "../renderer/window_manager.hpp"
#include "util.hpp"

using namespace vn::renderer;

namespace vn { namespace ui {

void create_window(std::string_view name, uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept
{
  // empty window is renderer used fullscreen window for moving and resive other windows
  err_if(name.empty(), "window name cannot be empty");
  WindowManager::instance()->create_window(name, x, y, width, height);
}

auto window_count() noexcept -> uint32_t
{
  return WindowManager::instance()->window_count();
}

}}
