#include "window.hpp"
#include "../util.hpp"

namespace vn {

auto Window::size() const noexcept -> Size
{
  RECT rect;
  exit_if(!GetClientRect(_handle, &rect), "failed to get window size");
  return { static_cast<uint32_t>(rect.right - rect.left), static_cast<uint32_t>(rect.bottom - rect.top) };
}

}