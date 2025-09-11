#pragma once

#include <windows.h>

#include <cstdint>

namespace vn {

class Window
{
  friend class WindowManager;

private:
  Window(HWND handle) noexcept
    : _handle(handle) {}

public:
  Window(Window const&)            = delete;
  Window(Window&&)                 = delete;
  Window& operator=(Window const&) = delete;
  Window& operator=(Window&&)      = delete;
  
  auto handle() const noexcept { return _handle; }

  struct Size
  {
    uint32_t width;
    uint32_t height;
  };
  auto size() const noexcept -> Size;

private:
  HWND _handle{};
};

}