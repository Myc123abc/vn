#include "window.hpp"
#include "window_manager.hpp"
#include "../util.hpp"

#include <DirectXMath.h>

namespace vn {

auto Window::size() const noexcept -> glm::vec2
{
  RECT rect;
  err_if(!GetClientRect(_handle, &rect), "failed to get window size");
  return { rect.right - rect.left, rect.bottom - rect.top };
}

// TODO: single buffer multiple windows vertices
auto Window::uv_rect_coord() const noexcept -> UVRectCoord
{
  RECT rect{};
  GetWindowRect(_handle, &rect);
  auto screen_size = WindowManager::instance()->screen_size();
  return
  {
    glm::vec2{ rect.left,  rect.top    } / screen_size,
    glm::vec2{ rect.right, rect.top    } / screen_size,
    glm::vec2{ rect.left,  rect.bottom } / screen_size,
    glm::vec2{ rect.right, rect.bottom } / screen_size,
  };
}

}