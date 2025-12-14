#include "window.hpp"
#include "window_manager.hpp"
#include "error_handling.hpp"

#include <algorithm>

namespace vn { namespace renderer {

void Window::init(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept
{
  err_if(width < min_width || height < min_height, "too small window!");

	this->handle = handle;
  this->name   = name;
	this->x      = x;
	this->y      = y;
	this->width  = width;
	this->height = height;

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
  moving  = true;
  x      += dx;
  y      += dy;
  update_rect();
}

void Window::move_from_maximize(int x, int y) noexcept
{
  auto ratio_x = static_cast<float>(x) / width;

  moving             = true;
  is_maximized       = false;
  width              = backup_rect.right  - backup_rect.left;
  height             = backup_rect.bottom - backup_rect.top;
  this->x            = x - width * ratio_x;
  this->y            = 0;
  need_resize_window = true;
  update_rect();
}

auto Window::point_on(glm::vec<2, int> const& p) const noexcept -> bool
{
  return p.x >= rect.left && p.x <= rect.right  &&
         p.y >= rect.top  && p.y <= rect.bottom;
}

void Window::adjust_offset(ResizeType type, glm::vec<2, int> const& point, int& dx, int& dy) const noexcept
{
  using enum ResizeType;
  switch (type)
  {
  case none:
    break;

  case left_top:
    if (width  == min_width  && point.x > rect.left) dx = 0;
    if (height == min_height && point.y > rect.top)  dy = 0;
    break;

  case right_top:
    if (width  == min_width  && point.x < rect.right) dx = 0;
    if (height == min_height && point.y > rect.top)   dy = 0;
    break;

  case left_bottom:
    if (width  == min_width  && point.x > rect.left)   dx = 0;
    if (height == min_height && point.y < rect.bottom) dy = 0;
    break;

  case right_bottom:
    if (width  == min_width  && point.x < rect.right)  dx = 0;
    if (height == min_height && point.y < rect.bottom) dy = 0;
    break;

  case left:
    if (width == min_width && point.x > rect.left) dx = 0;
    break;

  case right:
    if (width == min_width && point.x < rect.right) dx = 0;
    break;

  case top:
    if (height == min_height && point.y > rect.top) dy = 0;
    break;

  case bottom:
    if (height == min_height && point.y < rect.bottom) dy = 0;
    break;
  }
}

void Window::resize(ResizeType type, int dx, int dy) noexcept
{
  resizing    = true;
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

auto Window::get_resize_type(glm::vec<2, int> const& p) const noexcept -> ResizeType
{
  using enum ResizeType;

  if (p.x < rect.left || p.x > rect.right  || p.y < rect.top  || p.y > rect.bottom || is_maximized)
    return none;

  bool left_side   = p.x >= rect.left                          && p.x <= rect.left + Window_Resize_Width;
  bool right_side  = p.x >= rect.right  - Window_Resize_Width  && p.x <= rect.right;
  bool top_side    = p.y >= rect.top                           && p.y <= rect.top  + Window_Resize_Height;
  bool bottom_side = p.y >= rect.bottom - Window_Resize_Height && p.y <= rect.bottom;

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

void Window::left_offset(int dx) noexcept
{
  auto rc = get_maximize_rect();
  rect.left = std::clamp(
    rect.left + dx,
    rc.left,
    static_cast<LONG>(std::min(rect.right, rc.right) - min_width)
  );
  auto p = get_cursor_pos();
  if (dx < 0 && rect.left < p.x && rc.right - rect.left > min_width) rect.left = p.x;
  if (rect.right > rc.right && rc.right - rect.left < min_width) rect.left = rc.right - min_width;
}

void Window::top_offset(int dy) noexcept
{
  auto rc = get_maximize_rect();
  rect.top = std::clamp(
    rect.top + dy,
    rc.top,
    static_cast<LONG>(std::min(rect.bottom, rc.bottom) - min_height)
  );
  auto p = get_cursor_pos();
  if (dy < 0 && rect.top < p.y && rc.bottom - rect.top > min_height) rect.top = p.y;
  if (rect.bottom > rc.bottom && rc.bottom - rect.top < min_height) rect.top = rc.bottom - min_width;
}

void Window::right_offset(int dx) noexcept
{
  auto rc = get_maximize_rect();
  rect.right = std::clamp(
    rect.right + dx,
    static_cast<LONG>(std::max(rect.left, rc.left) + min_width),
    rc.right
  );
  auto p = get_cursor_pos();
  if (dx > 0 && rect.right > p.x && rect.right - rc.left > min_width) rect.right = p.x;
  if (rect.left < rc.left && rect.right - rc.left < min_width) rect.right = rc.left + min_width;
  if (rect.right == rc.right - 1) rect.right = rc.right; // I don't know why this case the dx is always 0
}

void Window::bottom_offset(int dy) noexcept
{
  auto rc = get_maximize_rect();
  rect.bottom = std::clamp(
    rect.bottom + dy,
    static_cast<LONG>(std::max(rect.top, rc.top) + min_height),
    rc.bottom
  );
  auto p = get_cursor_pos();
  if (dy > 0 && rect.bottom > p.y && rect.bottom - rc.top > min_height) rect.bottom = p.y;
  if (rect.top < rc.top && rect.bottom - rc.top < min_height) rect.bottom = rc.top + min_height;
  if (rect.bottom == rc.bottom - 1) rect.bottom = rc.bottom; // I don't know why this case the dy is always 0
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
  return {};
}

void set_cursor(HWND handle, Window::ResizeType type) noexcept
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
  SetClassLongPtrA(handle, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursorA(nullptr, cursor)));
}

auto Window::cursor_pos() const noexcept -> glm::vec<2, int>
{
  auto pos = get_cursor_pos();
  return { pos.x - x, pos.y - y};
}

auto Window::is_move_area(int x, int y) const noexcept -> bool
{
  x -= this->x;
  y -= this->y;

  for (auto const& area : move_invalid_area)
    if (x >= area.left && x <= area.right && y >= area.top && y <= area.bottom)
      return false;
  return true;
}

auto Window::cursor_valid_area() const noexcept -> bool
{
  auto pos = cursor_pos();
  if (is_maximized)
    return pos.x >= rect.left && pos.x <= rect.right  &&
           pos.y >= rect.top  && pos.y <= rect.bottom;
  else
    // TODO: change to multiple rectangles for external pop subwindow
    return pos.x > Window_Resize_Width  && pos.x < width  - Window_Resize_Width &&
           pos.y > Window_Resize_Height && pos.y < height - Window_Resize_Height;
}

void Window::maximize() noexcept
{
  is_maximized = true;
  backup_rect  = rect;
  rect = get_maximize_rect();
  update_by_rect();
  mouse_state = {};
}

void Window::restore() noexcept
{
  is_maximized = false;
  rect         = backup_rect;
  update_by_rect();
}

auto Window::is_mouse_pass_through_area() const noexcept -> bool
{
  auto pos = get_cursor_pos();
  return pos.x < rect.left || pos.x > rect.right || pos.y < rect.top || pos.y > rect.bottom;
}

}}
