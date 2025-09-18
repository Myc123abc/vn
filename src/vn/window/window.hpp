#pragma once

#include <windows.h>

#include <glm/glm.hpp>

namespace vn {

struct UVRectCoord
{
  glm::vec2 left_top{};
  glm::vec2 right_top{};
  glm::vec2 left_bottom{};
  glm::vec2 right_bottom{};
};

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

  auto size() const noexcept -> glm::vec2;

  auto uv_rect_coord() const noexcept -> UVRectCoord;

private:
  HWND _handle{};
};

}