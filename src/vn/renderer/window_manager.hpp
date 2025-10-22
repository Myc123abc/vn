#pragma once

#include "window.hpp"

#include <unordered_map>

/*
TODO:
dynamic set WS_EX_TRANSPARENT to make window can mouse pass through
and in interactive area, remove WS_EX_TRANSPARENT
so dynamic mosue pass through, this can be try
*/

namespace vn { namespace ui {

class UIContext;

}}

namespace vn { namespace renderer {

inline auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

inline auto get_cursor_pos() noexcept -> glm::vec<2, int>
{
  POINT p;
  GetCursorPos(&p);
  return { p.x, p.y };
}

inline auto is_mouse_down() noexcept { return GetKeyState(VK_LBUTTON) & 0x8000; }

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

class WindowManager
{
  friend LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;
  friend class ui::UIContext;

private:
  WindowManager()                                = default;
  ~WindowManager()                               = default;
public:
  WindowManager(WindowManager const&)            = delete;
  WindowManager(WindowManager&&)                 = delete;
  WindowManager& operator=(WindowManager const&) = delete;
  WindowManager& operator=(WindowManager&&)      = delete;

  static auto const instance() noexcept
  {
    static WindowManager instance;
    return &instance;
  }

  void init() noexcept;

  void message_process() noexcept;

  auto create_window(std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept -> HWND;

  auto window_count() const noexcept { return _windows.size(); }

  void begin_use_fullscreen_window() const noexcept;
  void end_use_fullscreen_window() const noexcept;

  auto get_window_name(HWND handle) noexcept -> std::string_view;
  auto get_window(HWND handle) noexcept { return _windows[handle]; }

private:
  void process_message(MSG const& msg) noexcept;

  void process_begin_use_fullscreen_window() noexcept;
  void process_end_use_fullscreen_window() noexcept;

private:
  std::unordered_map<HWND, Window> _windows;
  HWND                             _fullscreen_window_handle;
  Window                           _moving_or_resizing_window;
};

}}
