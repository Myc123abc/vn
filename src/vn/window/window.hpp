#pragma once

#include <windows.h>

#include <cstdint>

namespace vn {

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

  struct Size
  {
    uint32_t width;
    uint32_t height;
  };
  auto size() const noexcept -> Size;

private:
  HWND       _handle{};
};

}