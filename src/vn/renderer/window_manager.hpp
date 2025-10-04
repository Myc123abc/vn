#pragma once

#include "window.hpp"

#include <thread>
#include <latch>
#include <atomic>
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
  void destroy() noexcept;
  
  void create_window(int x, int y, uint32_t width, uint32_t height) noexcept;

  auto window_count() const noexcept { return _window_count.load(std::memory_order_relaxed); }

  void begin_moving_window() const noexcept;
  void end_moving_window() const noexcept;
  void begin_resizing_window() const noexcept;
  void end_resizing_window() const noexcept;

private:
  static LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

  void process_message(MSG const& msg) noexcept;

  void process_message_create_window(int x, int y, uint32_t width, uint32_t height) noexcept;

  void process_begin_moving_window() noexcept;
  void process_end_moving_window() noexcept;
  void process_begin_resizing_window() noexcept;
  void process_end_resizing_window() noexcept;

private:
  std::thread                      _thread;
  DWORD                            _thread_id{};
  std::latch                       _message_queue_create_complete{ 1 };
  std::atomic_uint32_t             _window_count;
  std::unordered_map<HWND, Window> _windows;
  HWND                             _fullscreen_window_handle;
  Window                           _moving_window;
  Window                           _resizing_window;
};

}}