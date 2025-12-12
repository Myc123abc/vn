#pragma once

#include "config.hpp"

#include <windows.h>

#include <glm/glm.hpp>

#include <string>

namespace vn { namespace renderer {

enum class CursorType
{
  arrow,
  up_down,
  left_rigtht,
  diagonal,
  anti_diagonal,
  Number
};

enum class MouseState
{
  idle,
  left_button_down,
  left_button_press,
  left_button_up,
};

struct Window
{
  friend LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;
  friend class MessageQueue;

  HWND              handle{};
  std::string       name{};
  int               x{};
  int               y{};
  uint32_t          width{};
  uint32_t          height{};
  RECT              rect{};
  bool              moving{};
  bool              resizing{};
  CursorType        cursor_type{};
  MouseState        mouse_state{};
  bool              is_minimized{};
  bool              is_maximized{};
  RECT              backup_rect{};
  bool              is_mouse_pass_through{};
  std::vector<RECT> move_invalid_area{};
  bool              need_resize_window{};
  uint32_t          min_width{ 50 };
  uint32_t          min_height{ 50 };

  auto real_x()      const noexcept { return x      - Window_Shadow_Thickness;     }
  auto real_y()      const noexcept { return y      - Window_Shadow_Thickness;     }
  auto real_width()  const noexcept { return width  + Window_Shadow_Thickness * 2; }
  auto real_height() const noexcept { return height + Window_Shadow_Thickness * 2; }
  auto real_rect()   const noexcept { return RECT{ rect.left   - static_cast<LONG>(Window_Shadow_Thickness),
                                                   rect.top    - static_cast<LONG>(Window_Shadow_Thickness),
                                                   rect.right  + static_cast<LONG>(Window_Shadow_Thickness),
                                                   rect.bottom + static_cast<LONG>(Window_Shadow_Thickness) }; }
  auto content_pos() const noexcept { return glm::vec2{ Window_Shadow_Thickness, Window_Shadow_Thickness }; }

  void init(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept;

	auto is_moving_or_resizing() const noexcept { return moving || resizing; }

	auto pos() const noexcept { return glm::vec2{ x, y }; }

  void move(int dx, int dy) noexcept;
  void move_from_maximize(int x, int y) noexcept;

  enum class ResizeType
  {
    none,
    left_top,
    right_top,
    left_bottom,
    right_bottom,
    left,
    right,
    top,
    bottom,
  };

  void resize(ResizeType type, int dx, int dy) noexcept;

  void maximize() noexcept;
  void restore() noexcept;

  auto get_resize_type(glm::vec<2, int> const& p) const noexcept -> ResizeType;

  auto point_on(glm::vec<2, int> const& p) const noexcept -> bool;
  void adjust_offset(ResizeType type, glm::vec<2, int> const& point, int& dx, int& dy) const noexcept;

  auto cursor_pos() const noexcept -> glm::vec<2, int>;

  auto is_active() const noexcept { return GetForegroundWindow() == handle; }

  auto is_move_area(int x, int y) const noexcept -> bool;

  auto cursor_valid_area() const noexcept -> bool;

  auto is_mouse_pass_through_area() const noexcept -> bool;

private:
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_rect()         noexcept;
  void update_by_rect()      noexcept;
};

void set_cursor(HWND handle, Window::ResizeType type = Window::ResizeType::none) noexcept;
auto get_cursor_type(Window::ResizeType type) noexcept -> CursorType;

}}
