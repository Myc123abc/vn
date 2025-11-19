#pragma once

#include "window.hpp"

#include <unordered_map>
#include <unordered_set>

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
    left_button_press = WM_APP,
    mouse_idle,
    window_restore_from_maximize,
  };
  void message_process() noexcept;

  auto create_window(std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept -> HWND;

  auto window_count() const noexcept { return _windows.size(); }

  auto get_window_name(HWND handle) noexcept -> std::string;
  auto get_window(HWND handle) const noexcept { return _windows.at(handle); }

  auto get_window_z_orders() const noexcept -> std::vector<HWND>;

private:
  std::unordered_map<HWND, Window> _windows;
  std::unordered_set<HWND>         _using_mouse_pass_through_windows;
};

}}
