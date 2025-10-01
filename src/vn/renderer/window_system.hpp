#pragma once

#include <windows.h>

#include <glm/glm.hpp>

#include <vector>
#include <latch>

namespace vn { namespace renderer {

struct Window
{
  uint32_t id;
  int      x;
  int      y;
  int      width;
  int      height;
  RECT     scissor_rect;
  RECT     rect;

public:
  Window(int x, int y, int width, int height);

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
  static auto generate_id() noexcept
  {
    static uint32_t id{};
    return ++id;
  }
  
  void left_offset(int dx)   noexcept;
  void top_offset(int dy)    noexcept;
  void right_offset(int dx)  noexcept;
  void bottom_offset(int dy) noexcept;

  void update_scissor_rect() noexcept;
  void update_rect()         noexcept;
  void update_by_rect()      noexcept;
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

  void create_window(int x, int y, int width, int height) noexcept;

private:
  static LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param);

  void process_message(MSG const& msg) noexcept;

  void process_message_create_window(int x, int y, int width, int height) noexcept;

private:
  HWND                  _handle{};
  DWORD                 _thread_id{};
  std::latch            _message_queue_create_complete{ 1 };
  glm::vec<2, uint32_t> _screen_size;

  WindowResources       _window_resources;
  bool                  _window_resources_changed{};
  bool                  _fullscreen_region_changed{};
};
  
}}