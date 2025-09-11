#include "window_manager.hpp"
#include "../renderer/renderer.hpp"
#include "../util.hpp"

namespace vn {

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param)
{
  switch (msg)
  {
    case WM_DESTROY:
      WindowManager::instance()->_window_count.fetch_sub(1, std::memory_order_relaxed);
      return 0;
  }
  return DefWindowProcW(handle, msg, w_param, l_param);
}

void WindowManager::init() noexcept
{
  // register class
  WNDCLASSEXW wnd_class{};
  wnd_class.lpszClassName = Class_Name;
  wnd_class.cbSize        = sizeof(wnd_class);
  wnd_class.hInstance     = GetModuleHandleW(nullptr);
  wnd_class.lpfnWndProc   = wnd_proc;
  exit_if(!RegisterClassExW(&wnd_class), "failed register class");

  // enable window manager thread
  _thread = std::thread{[this] {
    _thread_id = GetCurrentThreadId();
    PeekMessage(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    _wait_post_thread_message_valid.count_down();

    MSG msg{};
    while (true)
    {
      while (GetMessageW(&msg, nullptr, 0, 0))
      {
        if (msg.message == WM_CREATE_WINDOW)
        {
          create_window();
          continue;
        }
        else if (msg.message == WM_EXIT)
          return;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
  }};
}

void WindowManager::destroy() noexcept
{
  PostThreadMessageW(_thread_id, WM_EXIT, 0, 0);
  _thread.join();
  exit_if(!UnregisterClassW(Class_Name, GetModuleHandleW(nullptr)), "failed unregister class");
}

void WindowManager::create_window() noexcept
{
  // create window
  _window_create_info.handle = CreateWindowExW(
    0, Class_Name, nullptr, WS_OVERLAPPEDWINDOW,
    _window_create_info.x, _window_create_info.y, _window_create_info.width, _window_create_info.height,
    0, 0, GetModuleHandleW(nullptr), 0);
  exit_if(!_window_create_info.handle, "failed to create window");

  // init renderer resource
  Renderer::instance()->create_window_resources(_window_create_info.handle);

  // show window
  ShowWindow(_window_create_info.handle, SW_SHOW);

  // add window count
  _window_count.fetch_add(1, std::memory_order_relaxed);

  // create finish
  _window_create_info.finish.release();
}

void WindowManager::create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept
{
  // fill create info
  _window_create_info.x      = x;
  _window_create_info.y      = y;
  _window_create_info.width  = width;
  _window_create_info.height = height;

  // promise window manger is already init complete
  _wait_post_thread_message_valid.wait();

  // post create message
  exit_if(!PostThreadMessageW(_thread_id, WM_CREATE_WINDOW, 0, 0), "failed to post message");

  // wait create finish
  _window_create_info.finish.acquire();
}

}