#pragma once

#include <windows.h>

#include <glm/glm.hpp>

namespace vn { namespace renderer {

struct Window
{
  HWND     handle;
  int      x;
  int      y;
  uint32_t width;
  uint32_t height;
  RECT     rect;
  bool     moving{};
  bool     resizing{};

public:
  Window() = default;
  Window(HWND handle, int x, int y, uint32_t width, uint32_t height);

  auto pos() const noexcept { return glm::vec<2, int32_t>{ x, y }; }

  void move(int dx, int dy) noexcept;
  
  static auto constexpr Resize_Width  = 10;
  static auto constexpr Resize_Height = 10;

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

  auto get_resize_type(POINT const& p) const noexcept -> ResizeType;

  auto point_on(POINT const& p) const noexcept -> bool;

  void adjust_offset(ResizeType type, POINT const& point, LONG& dx, LONG& dy) const noexcept;

private:
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_rect()         noexcept;
  void update_by_rect()      noexcept;
};

}}