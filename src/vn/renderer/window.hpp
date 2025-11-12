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

class Window
{
  friend LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;
public:
  auto static constexpr External_Thickness = RECT{ 20, 20, 20, 20 };
  auto static External_Thickness_Offset() noexcept { return glm::vec2{ External_Thickness.left, External_Thickness.top }; }

public:
  Window() = default;
  Window(HWND handle, std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept;

  auto handle() const noexcept { return _handle; }
  auto name()   const noexcept { return _name;   }

  auto x()      const noexcept { return _x      + External_Thickness.left;                             }
  auto y()      const noexcept { return _y      + External_Thickness.top;                              }
  auto width()  const noexcept { return _width  - External_Thickness.left - External_Thickness.right;  }
  auto height() const noexcept { return _height - External_Thickness.top  - External_Thickness.bottom; }
  auto rect()   const noexcept { return _rect; }

  auto real_x()      const noexcept { return _x;      }
  auto real_y()      const noexcept { return _y;      }
  auto real_width()  const noexcept { return _width;  }
  auto real_height() const noexcept { return _height; }
  auto real_rect()   const noexcept
  {
    return RECT
    {
      _rect.left   - External_Thickness.left,
      _rect.top    - External_Thickness.top,
      _rect.right  + External_Thickness.right,
      _rect.bottom + External_Thickness.bottom
    };
  }

	auto cursor_type()  const noexcept { return _cursor_type;  }
  auto mouse_state()  const noexcept { return _mouse_state;  }
  auto is_resizing()  const noexcept { return _resizing;     }
  auto is_moving()    const noexcept { return _moving;       }
  auto is_minimized() const noexcept { return _is_minimized; }
  auto is_maximized() const noexcept { return _is_maximized; }
	auto is_moving_or_resizing() const noexcept { return _moving || _resizing; }

  auto real_pos() const noexcept { return glm::vec<2, int32_t>{ _x, _y }; }

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

  auto is_active() const noexcept { return GetForegroundWindow() == _handle; }

  auto is_move_area(int x, int y) const noexcept -> bool;

  auto cursor_valid_area() const noexcept -> bool;

  auto is_mouse_pass_through_area() const noexcept -> bool;

  auto clear_move_invalid_area() noexcept { _move_invalid_area.clear(); }

  void set(MouseState state) noexcept { _mouse_state = state; }

  void set_minimize(bool b) noexcept { _is_minimized = b; }

  void add_move_invalid_area(float l, float t, float r, float b) noexcept { _move_invalid_area.emplace_back(l, t, r, b); }

private:
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_rect()         noexcept;
  void update_by_rect()      noexcept;

private:
  HWND        _handle{};
  std::string _name{};
  int         _x{};
  int         _y{};
  uint32_t    _width{};
  uint32_t    _height{};
  RECT        _rect{};
  bool        _moving{};
  bool        _resizing{};
  CursorType  _cursor_type{};
  MouseState  _mouse_state{};
  bool        _is_minimized{};
  bool        _is_maximized{};
  RECT        _backup_rect{};
  bool        _need_resize_swapchain{};
  bool        _is_mouse_pass_through{};

  std::vector<RECT> _move_invalid_area{};
};

void set_cursor(HWND handle, Window::ResizeType type = Window::ResizeType::none) noexcept;
auto get_cursor_type(Window::ResizeType type) noexcept -> CursorType;

}}
