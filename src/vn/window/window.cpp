#include "window.hpp"
#include "../util.hpp"

#include <DirectXMath.h>
#include <dwmapi.h>

namespace vn {

// TODO: only primary screen
auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

auto window_rect(HWND handle) noexcept -> RECT
{
  RECT rect{};
  DwmGetWindowAttribute(handle, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
  return rect;
}

auto is_rect_intersect(RECT const& x, RECT const& y) noexcept -> bool
{
  return x.left < y.right  && x.right  > y.left &&
         x.top  < y.bottom && x.bottom > y.top;
}

auto rect_difference(RECT const& x, RECT const& y) noexcept -> std::vector<RECT>
{
  auto rects = std::vector<RECT>{};

  // no intersect
  auto intersect_rect = RECT{};
  if (!IntersectRect(&intersect_rect, &x, &y)) return { x };

  // top part
  if (intersect_rect.top > x.top)
    rects.emplace_back(x.left, x.top, x.right, intersect_rect.top );

  // bottom part
  if (intersect_rect.bottom < x.bottom)
    rects.emplace_back(x.left, intersect_rect.bottom, x.right, x.bottom);

  // left part
  if (intersect_rect.left > x.left)
    rects.emplace_back(x.left, intersect_rect.top, intersect_rect.left, intersect_rect.bottom);

  // right part
  if (intersect_rect.right < x.right)
    rects.emplace_back(intersect_rect.right, intersect_rect.top, x.right, intersect_rect.bottom);

  return rects;
}

auto rect_difference(RECT const& x, std::span<RECT> ys) noexcept -> std::vector<RECT>
{
  auto xs = std::vector<RECT>{ x };

  for (auto const& y : ys)
  {
    auto res = std::vector<RECT>{};
    for (auto const& x : xs)
    {
      res.append_range(rect_difference(x, y));
    }
    xs.swap(res);
    if (xs.empty()) return {};
  }

  return xs;
}

auto Window::size() const noexcept -> glm::vec<2, uint32_t>
{
  RECT rect;
  err_if(!GetClientRect(_handle, &rect), "failed to get window size");
  return { rect.right - rect.left, rect.bottom - rect.top };
}

auto Window::intersect_region(Window const& window) const noexcept -> std::optional<RECT>
{
  RECT rect{};
  auto rect_x = this->rect();
  auto rect_y = window.rect();
  if (IntersectRect(&rect, &rect_x, &rect_y))
    return rect;
  return {};
}

auto Window::position() const noexcept -> glm::vec<2, int32_t>
{
  RECT rect{};
  GetWindowRect(_handle, &rect);
  return { rect.left, rect.top };
}

auto Window::convert_to_window_coordinate(RECT rect) const noexcept -> RECT
{
  auto pos = position();
  rect.left   -= pos.x;
  rect.right  -= pos.x;
  rect.top    -= pos.y;
  rect.bottom -= pos.y;
  return rect;
}

auto Window::convert_to_window_coordinate(uint32_t x, uint32_t y) const noexcept -> glm::vec<2, int32_t>
{
  auto pos = this->position();
  x -= pos.x;
  y -= pos.y;
  return { x, y };
}

}