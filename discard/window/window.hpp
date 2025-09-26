#pragma once

#include <windows.h>

#include <glm/glm.hpp>

#include <span>
#include <vector>
#include <optional>

namespace vn {

class Window;

auto get_screen_size() noexcept -> glm::vec<2, uint32_t>;
auto window_rect(HWND handle) noexcept -> RECT;
auto is_rect_intersect(RECT const& x, RECT const& y) noexcept -> bool;
auto rect_difference(RECT const& x, RECT const& y) noexcept -> std::vector<RECT>;
auto rect_difference(RECT const& x, std::span<RECT> ys) noexcept -> std::vector<RECT>;

class Window
{
  friend class WindowManager;

public:
  Window()                         = default;
  ~Window()                        = default;
  Window(Window const&)            = default;
  Window(Window&&)                 = delete;
  Window& operator=(Window const&) = default;
  Window& operator=(Window&&)      = delete;

  void init(HWND handle) noexcept { _handle = handle; }
  
  auto handle() const noexcept { return _handle; }

  auto size() const noexcept -> glm::vec<2, uint32_t>;

  auto rect() const noexcept -> RECT { return window_rect(_handle); }

  auto intersect_region(Window const& window) const noexcept -> std::optional<RECT>;

  auto position() const noexcept -> glm::vec<2, int32_t>;

  auto convert_to_window_coordinate(RECT rect) const noexcept -> RECT;
  auto convert_to_window_coordinate(uint32_t x, uint32_t y) const noexcept -> glm::vec<2, int32_t>;

private:
  HWND _handle{};
};

}