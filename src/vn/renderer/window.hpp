#pragma once

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
  std::string name;
  HWND        handle{};
  int         x{};
  int         y{};
  uint32_t    width{};
  uint32_t    height{};
  RECT        rect;
  bool        moving{};
  bool        resizing{};
  CursorType  cursor_type{};
  MouseState  mouse_state{};
  bool        is_minimized{};
  bool        is_maximized{};
  RECT        backup_rect{};
  bool        need_resize_swapchain{};

  std::vector<RECT> move_invalid_area{};

public:
  Window() = default;
  Window(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept;

  auto pos() const noexcept { return glm::vec<2, int32_t>{ x, y }; }

  void move(int dx, int dy) noexcept;
  void move_from_maximize(int x, int y) noexcept;

  static auto constexpr Resize_Width  = 5;
  static auto constexpr Resize_Height = 5;

  static auto constexpr Min_Width  = 50;
  static auto constexpr Min_Height = 50;

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

private:
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_rect()         noexcept;
  void update_by_rect()      noexcept;
};

void set_cursor(Window::ResizeType type = Window::ResizeType::none) noexcept;
auto get_cursor_type(Window::ResizeType type) noexcept -> CursorType;

}}
