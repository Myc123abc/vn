#pragma once

#include <windows.h>

#include <thread>
#include <latch>
#include <atomic>
#include <vector>

namespace vn { namespace renderer {

struct Window
{
  HWND handle;
  int  x;
  int  y;
  int  width;
  int  height;
  RECT rect;

public:
  Window() = default;
  Window(HWND handle, int x, int y, int width, int height);

  void move(int x, int y) noexcept;
  
  static auto constexpr Resize_Width  = 10;
  static auto constexpr Resize_Height = 10;

  static auto constexpr Min_Width  = 50;
  static auto constexpr Min_Height = 50;

  enum class ResizeType
  {
    none,
    left_top,
    right_top,
    left_bottom,
    right_bottom,
    left,
    right,
    top,
    bottom,
  };
  void resize(ResizeType type, int x, int y) noexcept;
  auto get_resize_type(POINT const& p) const noexcept;

private:
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_rect()         noexcept;
  void update_by_rect()      noexcept;
};

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

private:
  static LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept;

  void process_message(MSG const& msg) noexcept;

  void process_message_create_window(int x, int y, uint32_t width, uint32_t height) noexcept;

private:
  std::thread          _thread;
  DWORD                _thread_id{};
  std::latch           _message_queue_create_complete{ 1 };
  std::atomic_uint32_t _window_count;
  std::vector<Window>  _windows;
};

}}