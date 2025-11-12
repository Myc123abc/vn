#include "window.hpp"
#include "window_manager.hpp"
#include "error_handling.hpp"

#include <algorithm>

namespace vn { namespace renderer {

Window::Window(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept
  : _handle(handle), _name(name), _x(x), _y(y), _width(width), _height(height)
{
  err_if(this->width() < Min_Width || this->height() < Min_Height, "too small window!");
  update_rect();
}

void Window::update_rect() noexcept
{
  _rect.left   = x();
  _rect.top    = y();
  _rect.right  = x() + width();
  _rect.bottom = y() + height();
}

void Window::update_by_rect() noexcept
{
  _x      = _rect.left - External_Thickness.left;
  _y      = _rect.top  - External_Thickness.top;
  _width  = _rect.right  - _rect.left + External_Thickness.left + External_Thickness.right;
  _height = _rect.bottom - _rect.top  + External_Thickness.top  + External_Thickness.bottom;
}

void Window::move(int32_t dx, int32_t dy) noexcept
{
  _moving  = true;
  _x      += dx;
  _y      += dy;
  update_rect();
}

void Window::move_from_maximize(int x, int y) noexcept
{
  auto ratio_x = static_cast<float>(x) / width();

  _moving       = true;
  _is_maximized = {};
  _width        = _backup_rect.right  - _backup_rect.left + External_Thickness.left + External_Thickness.right;
  _height       = _backup_rect.bottom - _backup_rect.top  + External_Thickness.top  + External_Thickness.bottom;;
  _x            = x - width() * ratio_x - External_Thickness.left;
  _y            = -External_Thickness.top;
  update_rect();
  _need_resize_swapchain = true;
}

auto Window::point_on(glm::vec<2, int> const& p) const noexcept -> bool
{
  return p.x >= _rect.left && p.x <= _rect.right  &&
         p.y >= _rect.top  && p.y <= _rect.bottom;
}

void Window::adjust_offset(ResizeType type, glm::vec<2, int> const& point, int& dx, int& dy) const noexcept
{
  using enum ResizeType;
  switch (type)
  {
  case none:
    break;

  case left_top:
    if (width()  == Min_Width  && point.x > _rect.left) dx = 0;
    if (height() == Min_Height && point.y > _rect.top)  dy = 0;
    break;

  case right_top:
    if (width()  == Min_Width  && point.x < _rect.right) dx = 0;
    if (height() == Min_Height && point.y > _rect.top)   dy = 0;
    break;

  case left_bottom:
    if (width()  == Min_Width  && point.x > _rect.left)   dx = 0;
    if (height() == Min_Height && point.y < _rect.bottom) dy = 0;
    break;

  case right_bottom:
    if (width()  == Min_Width  && point.x < _rect.right)  dx = 0;
    if (height() == Min_Height && point.y < _rect.bottom) dy = 0;
    break;

  case left:
    if (width() == Min_Width && point.x > _rect.left) dx = 0;
    break;

  case right:
    if (width() == Min_Width && point.x < _rect.right) dx = 0;
    break;

  case top:
    if (height() == Min_Height && point.y > _rect.top) dy = 0;
    break;

  case bottom:
    if (height() == Min_Height && point.y < _rect.bottom) dy = 0;
    break;
  }
}

void Window::resize(ResizeType type, int dx, int dy) noexcept
{
  _resizing = true;

  _cursor_type = get_cursor_type(type);

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

  if (p.x < _rect.left || p.x > _rect.right  || p.y < _rect.top  || p.y > _rect.bottom || _is_maximized)
    return none;

  bool left_side   = p.x >= _rect.left                   && p.x <= _rect.left + Resize_Width;
  bool right_side  = p.x >= _rect.right  - Resize_Width  && p.x <= _rect.right;
  bool top_side    = p.y >= _rect.top                    && p.y <= _rect.top  + Resize_Height;
  bool bottom_side = p.y >= _rect.bottom - Resize_Height && p.y <= _rect.bottom;

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
  _rect.left = std::clamp(
    _rect.left + dx,
    rc.left,
    std::min(_rect.right, rc.right) - Min_Width
  );
  auto p = get_cursor_pos();
  if (dx < 0 && _rect.left < p.x && rc.right - _rect.left > Min_Width) _rect.left = p.x;
  if (_rect.right > rc.right && rc.right - _rect.left < Min_Width) _rect.left = rc.right - Min_Width;
}

void Window::top_offset(int dy) noexcept
{
  auto rc = get_maximize_rect();
  _rect.top = std::clamp(
      _rect.top + dy,
    rc.top,
    std::min(_rect.bottom, rc.bottom) - Min_Height
  );
  auto p = get_cursor_pos();
  if (dy < 0 && _rect.top < p.y && rc.bottom - _rect.top > Min_Height) _rect.top = p.y;
  if (_rect.bottom > rc.bottom && rc.bottom - _rect.top < Min_Height) _rect.top = rc.bottom - Min_Width;
}

void Window::right_offset(int dx) noexcept
{
  auto rc = get_maximize_rect();
  _rect.right = std::clamp(
      _rect.right + dx,
    std::max(_rect.left, rc.left) + Min_Width,
    rc.right
  );
  auto p = get_cursor_pos();
  if (dx > 0 && _rect.right > p.x && _rect.right - rc.left > Min_Width) _rect.right = p.x;
  if (_rect.left < rc.left && _rect.right - rc.left < Min_Width) _rect.right = rc.left + Min_Width;
  if (_rect.right == rc.right - 1) _rect.right = rc.right; // I don't know why this case the dx is always 0
}

void Window::bottom_offset(int dy) noexcept
{
  auto rc = get_maximize_rect();
  _rect.bottom = std::clamp(
      _rect.bottom + dy,
    std::max(_rect.top, rc.top) + Min_Height,
    rc.bottom
  );
  auto p = get_cursor_pos();
  if (dy > 0 && _rect.bottom > p.y && _rect.bottom - rc.top > Min_Height) _rect.bottom = p.y;
  if (_rect.top < rc.top && _rect.bottom - rc.top < Min_Height) _rect.bottom = rc.top + Min_Height;
  if (_rect.bottom == rc.bottom - 1) _rect.bottom = rc.bottom; // I don't know why this case the dy is always 0
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
  return { pos.x - x(), pos.y - y()};
}

auto Window::is_move_area(int x, int y) const noexcept -> bool
{
  x -= this->x();
  y -= this->y();

  for (auto const& area : _move_invalid_area)
    if (x >= area.left && x <= area.right && y >= area.top && y <= area.bottom)
      return false;
  return true;
}

auto Window::cursor_valid_area() const noexcept -> bool
{
  auto pos = cursor_pos();
  if (_is_maximized)
    return pos.x >= _rect.left && pos.x <= _rect.right  &&
           pos.y >= _rect.top  && pos.y <= _rect.bottom;
  else
    // TODO: change to multiple rectangles for external pop subwindow
    return pos.x > Resize_Width  && pos.x < width()  - Resize_Width &&
           pos.y > Resize_Height && pos.y < height() - Resize_Height;
}

void Window::maximize() noexcept
{
  _is_maximized = true;
  _backup_rect  = _rect;
  _rect = get_maximize_rect();
  update_by_rect();
  _mouse_state = {};
}

void Window::restore() noexcept
{
  _is_maximized = {};
  _rect         = _backup_rect;
  update_by_rect();
}

auto Window::is_mouse_pass_through_area() const noexcept -> bool
{
  auto pos = get_cursor_pos();
  return pos.x < _rect.left || pos.x > _rect.right || pos.y < _rect.top || pos.y > _rect.bottom;
}

}}
