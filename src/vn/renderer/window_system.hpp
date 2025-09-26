#pragma once

#include <windows.h>

#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <atomic>
#include <latch>

namespace vn { namespace renderer {

struct Window
{
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
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

  auto const* updated_data() const noexcept { return _updated_data.load(std::memory_order_relaxed); }

  void create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

private:
  void process_message(MSG const& msg) noexcept;

  void process_message_create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;

private:
  HWND                             _handle{};
  DWORD                            _thread_id{};
  std::latch                       _message_queue_create_complete{ 1 };
  glm::vec<2, uint32_t>            _screen_size;

  std::unique_ptr<WindowResources> _data0{ std::make_unique<WindowResources>() };
  std::unique_ptr<WindowResources> _data1{ std::make_unique<WindowResources>() };
  WindowResources*                 _data         = _data0.get();
  std::atomic<WindowResources*>    _updated_data = _data1.get();
  bool                             _updated{};
};
  
}}