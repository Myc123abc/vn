#pragma once

#include <windows.h>

#include <glm/glm.hpp>

#include <vector>
#include <latch>

namespace vn { namespace renderer {

struct Window
{
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;

  auto rect() const noexcept
  {
    return RECT
    { 
      static_cast<LONG>(x), 
      static_cast<LONG>(y), 
      static_cast<LONG>(x + width),
      static_cast<LONG>(y + height)
    };
  }
};

struct WindowResources
{
  std::vector<Window> windows;
};

class WindowSystem
{  
private:
  WindowSystem()                               = default;
  ~WindowSystem()                              = default;
public:
  WindowSystem(WindowSystem const&)            = delete;
  WindowSystem(WindowSystem&&)                 = delete;
  WindowSystem& operator=(WindowSystem const&) = delete;
  WindowSystem& operator=(WindowSystem&&)      = delete;

  static auto const instance() noexcept
  {
    static WindowSystem instance;
    return &instance;
  }

  void init() noexcept;

  auto handle()      const noexcept { return _handle;      }
  auto screen_size() const noexcept { return _screen_size; }

  void send_message_to_renderer() noexcept;

  void create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

private:
  static LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param);

  void process_message(MSG const& msg) noexcept;

  void process_message_create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

private:
  HWND                             _handle{};
  DWORD                            _thread_id{};
  std::latch                       _message_queue_create_complete{ 1 };
  glm::vec<2, uint32_t>            _screen_size;

  WindowResources                  _window_resources;

  bool                             _window_resources_changed{};
  bool                             _fullscreen_region_changed{};
};
  
}}