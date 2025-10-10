#pragma once

#include "window.hpp"

#include <unordered_map>

namespace vn { namespace renderer {

inline auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

class WindowManager
{  
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
  
  void create_window(int x, int y, uint32_t width, uint32_t height) noexcept;

  auto window_count() const noexcept { return _windows.size(); }

  void begin_use_fullscreen_window() const noexcept;
  void end_use_fullscreen_window() const noexcept;

private:
  static LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

  void process_message(MSG const& msg) noexcept;

  void process_begin_use_fullscreen_window() noexcept;
  void process_end_use_fullscreen_window() noexcept;

private:
  std::unordered_map<HWND, Window> _windows;
  HWND                             _fullscreen_window_handle;
  Window                           _moving_or_resizing_window;
};

}}