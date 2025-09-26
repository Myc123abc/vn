#pragma once

#include "window.hpp"

#include <windows.h>

#include <cstdint>
#include <thread>
#include <latch>
#include <semaphore>
#include <atomic>

namespace vn {

class WindowManager
{
  friend void init()    noexcept;
  friend void destroy() noexcept;
  
  friend LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
  
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

  // thread unsafe
  void create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

  void destroy_window(HWND handle) const noexcept;

  auto window_count() const noexcept { return _window_count.load(std::memory_order_relaxed); }

private:
  void init()    noexcept;
  void destroy() noexcept;

  void init_fullscreen_window() noexcept;

  void create_window() noexcept;

  auto message_process(MSG const& msg) noexcept -> bool;

private:
  static constexpr auto Message_Create_Window  = WM_APP;
  static constexpr auto Message_Exit           = WM_APP + 1;
  static constexpr auto Message_Destroy_Window = WM_APP + 2;

  static constexpr auto Class_Name = L"WindowManager";

  std::thread          _thread;
  DWORD                _thread_id{};
  std::latch           _wait_post_thread_message_valid{ 1 };
  std::atomic_uint32_t _window_count;
  HWND                 _fullscreen_window{};
  HWND                 _moved_window{};

  struct WindowCreateInfo
  {
    std::binary_semaphore finish{ 0 };
    HWND                  handle{};
    uint32_t              x{};
    uint32_t              y{};
    uint32_t              width{};
    uint32_t              height{};
  } _window_create_info;
};

}