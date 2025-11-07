#pragma once

#include "window.hpp"

#include <unordered_map>

/*
TODO:
dynamic set WS_EX_TRANSPARENT to make window can mouse pass through
and in interactive area, remove WS_EX_TRANSPARENT
so dynamic mosue pass through, this can be try

LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
if (PtInRect(&clickable, pt))
    SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_TRANSPARENT);
else
    SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);

*/

namespace vn { namespace ui {

class UIContext;

}}

namespace vn { namespace renderer {

inline auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

inline auto get_maximize_rect() noexcept
{
  auto rect = RECT{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);
  return rect;
}

inline auto get_cursor_pos() noexcept -> glm::vec<2, int>
{
  POINT p;
  GetCursorPos(&p);
  return { p.x, p.y };
}

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

  enum class Message
  {
    begin_use_fullscreen_window = WM_APP,
    end_use_fullscreen_window,
    left_button_press,
    mouse_idle,
    window_restore_from_maximize,
  };
  void message_process() noexcept;

  auto create_window(std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept -> HWND;

  auto window_count() const noexcept { return _windows.size(); }

  void begin_use_fullscreen_window() const noexcept;
  void end_use_fullscreen_window() const noexcept;

  auto get_window_name(HWND handle) noexcept -> std::string_view;
  auto get_window(HWND handle) noexcept { return _windows[handle]; }

  auto get_window_z_orders() const noexcept -> std::vector<HWND>;

private:
  void process_message(MSG const& msg) noexcept;

  void process_begin_use_fullscreen_window() noexcept;
  void process_end_use_fullscreen_window() noexcept;

private:
  std::unordered_map<HWND, Window> _windows;
  HWND                             _fullscreen_window_handle;
  std::vector<HWND>                _using_mouse_pass_through_windows;
};

}}
