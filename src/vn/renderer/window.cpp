#include "window.hpp"
#include "window_manager.hpp"
#include "util.hpp"

#include <algorithm>

namespace vn { namespace renderer {

Window::Window(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept
  : handle(handle), name(name), x(x), y(y), width(width), height(height)
{
  err_if(width < Min_Width || height < Min_Height, "too small window!");
  update_rect();
}

void Window::update_rect() noexcept
{
  rect.left   = x;
  rect.top    = y;
  rect.right  = x + width;
  rect.bottom = y + height;
}

void Window::update_by_rect() noexcept
{
  x      = rect.left;
  y      = rect.top;
  width  = rect.right  - rect.left;
  height = rect.bottom - rect.top;
}

void Window::move(int32_t dx, int32_t dy) noexcept
{
  moving   = true;
  this->x += dx;
  this->y += dy;
  update_rect();
}

auto Window::point_on(POINT const& p) const noexcept -> bool
{
  return p.x > rect.left && p.x < rect.right  &&
         p.y > rect.top  && p.y < rect.bottom;
}

void Window::adjust_offset(ResizeType type, POINT const& point, LONG& dx, LONG& dy) const noexcept
{
  using enum ResizeType;
  switch (type)
  {
  case none:
    break;

  case left_top:
    if (width == Min_Width && point.x > rect.left) dx = 0;
    if (height == Min_Height && point.y > rect.top) dy = 0;
    break;

  case right_top:
    if (width == Min_Width && point.x < rect.right) dx = 0;
    if (height == Min_Height && point.y > rect.top) dy = 0;
    break;

  case left_bottom:
    if (width == Min_Width && point.x > rect.left) dx = 0;
    if (height == Min_Height && point.y < rect.bottom) dy = 0;
    break;

  case right_bottom:
    if (width == Min_Width && point.x < rect.right) dx = 0;
    if (height == Min_Height && point.y < rect.bottom) dy = 0;
    break;

  case left:
    if (width == Min_Width && point.x > rect.left) dx = 0;
    break;

  case right:
    if (width == Min_Width && point.x < rect.right) dx = 0;
    break;

  case top:
    if (height == Min_Height && point.y > rect.top) dy = 0;
    break;

  case bottom:
    if (height == Min_Height && point.y < rect.bottom) dy = 0;
    break;
  }
}

void Window::resize(ResizeType type, int dx, int dy) noexcept
{
  resizing = true;

  cursor_type = get_cursor_type(type);

  using enum ResizeType;
  switch (type)
  {
  case none:
    return;

  case left_top:
    left_offset(dx);
    top_offset(dy);
    break;

  case right_top:
    right_offset(dx);
    top_offset(dy);
    break;

  case left_bottom:
    left_offset(dx);
    bottom_offset(dy);
    break;

  case right_bottom:
    right_offset(dx);
    bottom_offset(dy);
    break;

  case left:
    left_offset(dx);
    break;

  case right:
    right_offset(dx);
    break;

  case top:
    top_offset(dy);
    break;

  case bottom:
    bottom_offset(dy);
    break;
  }
  update_by_rect();
}

auto Window::get_resize_type(POINT const& p) const noexcept -> ResizeType
{
  using enum ResizeType;

  if (p.x < rect.left || p.x > rect.right  ||
      p.y < rect.top  || p.y > rect.bottom ||
      is_maximized)
    return none;

  bool left_side   = p.x >= rect.left                   && p.x <= rect.left + Resize_Width;
  bool right_side  = p.x >= rect.right - Resize_Width   && p.x <= rect.right;
  bool top_side    = p.y >= rect.top                    && p.y <= rect.top + Resize_Height;
  bool bottom_side = p.y >= rect.bottom - Resize_Height && p.y <= rect.bottom;

  if (top_side)
  {
    if (left_side)  return left_top;
    if (right_side) return right_top;
    return top;
  }
  if (bottom_side)
  {
    if (left_side)  return left_bottom;
    if (right_side) return right_bottom;
    return bottom;
  }
  if (left_side)  return left;
  if (right_side) return right;
  return none;
}

// FIXME: offsets have problem
void Window::left_offset(int dx) noexcept
{
  auto screen_size = get_screen_size();

  if (rect.left < 0) return;

  LONG max_left;
  if (rect.right > screen_size.x)
  {
    if (screen_size.x - rect.left < Min_Width)
      max_left = rect.left;
    else
      max_left = screen_size.x - Min_Width;
  }
  else
    max_left = rect.right - Min_Width;
  rect.left = std::clamp(rect.left + dx, 0l, max_left);
}

void Window::top_offset(int dy) noexcept
{
  auto screen_size = get_screen_size();

  if (rect.top < 0) return;

  LONG max_top;
  if (rect.bottom > screen_size.y)
  {
    if (screen_size.y - rect.top < Min_Height)
      max_top = rect.top;
    else
      max_top = screen_size.y - Min_Height;
  }
  else
    max_top = rect.bottom - Min_Height;
  rect.top = std::clamp(rect.top + dy, 0l, max_top);
}

void Window::right_offset(int dx) noexcept
{
  auto screen_size = get_screen_size();

  if (rect.right > screen_size.x) return;

  LONG min_right;
  if (rect.left < 0)
  {
    if (rect.right < Min_Width)
      min_right = rect.right;
    else
      min_right = Min_Width;
  }
  else
    min_right = rect.left + Min_Width;
  rect.right = std::clamp(rect.right + dx, min_right, static_cast<LONG>(screen_size.x));
}

void Window::bottom_offset(int dy) noexcept
{
  auto screen_size = get_screen_size();

  if (rect.bottom > screen_size.y) return;

  LONG min_bottom;
  if (rect.top < 0)
  {
    if (rect.bottom < Min_Height)
      min_bottom = rect.bottom;
    else
      min_bottom = Min_Height;
  }
  else
    min_bottom = rect.top + Min_Height;
  rect.bottom = std::clamp(rect.bottom + dy, min_bottom, static_cast<LONG>(screen_size.y));
}

auto get_cursor_type(Window::ResizeType type) noexcept -> CursorType
{
  using enum Window::ResizeType;
  using enum CursorType;
  switch (type)
  {
  case top:
  case bottom:
    return up_down;
  case left:
  case right:
    return left_rigtht;
  case right_top:
  case left_bottom:
    return diagonal;
  case left_top:
  case right_bottom:
    return anti_diagonal;
  case none:
    return arrow;
  }
}

void set_cursor(Window::ResizeType type) noexcept
{
  using enum Window::ResizeType;
  auto cursor = IDC_ARROW;
  switch (type)
  {
  case top:
  case bottom:
    cursor = IDC_SIZENS;
    break;
  case left:
  case right:
    cursor = IDC_SIZEWE;
    break;
  case right_top:
  case left_bottom:
    cursor = IDC_SIZENESW;
    break;
  case left_top:
  case right_bottom:
    cursor = IDC_SIZENWSE;
    break;
  case none:
    break;
  }
  SetCursor(LoadCursorA(nullptr, cursor));
}

auto Window::cursor_pos() const noexcept -> glm::vec<2, int>
{
  auto pos = get_cursor_pos();
  return { pos.x - x, pos.y - y };
}

auto Window::is_move_area(int x, int y) const noexcept -> bool
{
  x -= this->x;
  y -= this->y;
  for (auto const& area : move_invalid_area)
    if (x >= area.x && x <= area.z && y >= area.y && y <= area.w)
      return false;
  return true;
}

auto Window::cursor_valid_area() const noexcept -> bool
{
  auto pos = cursor_pos();
  if (is_maximized)
    return pos.x >= rect.left  && pos.x <= rect.right &&
           pos.y >= rect.top && pos.y <= rect.bottom;
  else
    return pos.x >= Resize_Width  && pos.x <= width  - Resize_Width &&
           pos.y >= Resize_Height && pos.y <= height - Resize_Height;
}

void Window::maximize() noexcept
{
  is_maximized = true;
  backup_rect = rect;
  rect = get_maximize_rect();
  update_by_rect();
  mouse_state = {};
}

void Window::restore() noexcept
{
  is_maximized = {};
  rect = backup_rect;
  update_by_rect();
}

}}
