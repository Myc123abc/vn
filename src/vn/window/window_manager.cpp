#include "window_manager.hpp"
#include "../renderer/message_queue.hpp"
#include "../util.hpp"

#include <windowsx.h>

namespace vn {

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param)
{
  static POINT dragging_mouse_pos{};
  static RECT  dragging_window_rect{};
  static bool  dragging{};

  switch (msg)
  {
  case WM_CLOSE:
  {
    ShowWindow(handle, SW_HIDE);
    EnableWindow(handle, false);
    WindowManager::instance()->_window_count.fetch_sub(1, std::memory_order_relaxed);
    renderer::MessageQueue::instance()->push(renderer::WindowCloseInfo{ handle });
  }
  return 0;

  case WM_SIZE:
  {
    if (w_param == SIZE_MINIMIZED)
      renderer::MessageQueue::instance()->push(renderer::WindowMinimizedInfo{ handle });
    else if (w_param == SIZE_RESTORED)
      renderer::MessageQueue::instance()->push(renderer::WindowResizeInfo{ handle });
  }
  return 0;

  case WM_LBUTTONDOWN:
  {
    GetCursorPos(&dragging_mouse_pos);
    GetWindowRect(handle, &dragging_window_rect);
    // TODO: customize move dragging area
    //if (dragging_mouse_pos.y < 25 + dragging_window_rect.top)
    {
      dragging = true;
      SetCapture(handle);
    }
  }
  break;

  case WM_LBUTTONUP:
  {
    if (dragging)
    {
      dragging = false;
      ReleaseCapture();
    }
  }
  break;

  case WM_MOUSEMOVE:
  {
    if (dragging)
    {
      POINT pos;
      GetCursorPos(&pos);
      dragging_window_rect.left += pos.x - dragging_mouse_pos.x;
      dragging_window_rect.top  += pos.y - dragging_mouse_pos.y;
      dragging_mouse_pos = pos;
      SetWindowPos(handle, nullptr,
        dragging_window_rect.left, dragging_window_rect.top, 0, 0,
        SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
    }
  }
  break;

  // TODO: i find a "best" way to resolve, use a fullscreen transparent window as backup.
  // when move or resize window, render the window content to the backup transparent fullscreen window as a virtual rendering window
  // and in fullscreen rendering it will be sliky move or resize, and restore the window to the new size and position after move or resize operation.
  case WM_NCHITTEST:
  {
    auto pos = POINT{ GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) };

    RECT wr;
    GetWindowRect(handle, &wr);
    int constexpr border = 10;

    if (pos.x >= wr.left && pos.x < wr.left + border &&
        pos.y >= wr.top  && pos.y < wr.top  + border)
      return HTTOPLEFT;

    if (pos.x >= wr.right - border && pos.x < wr.right        &&
        pos.y >= wr.top            && pos.y < wr.top + border)
      return HTTOPRIGHT;

    if (pos.x >= wr.left            && pos.x < wr.left + border &&
        pos.y >= wr.bottom - border && pos.y < wr.bottom)
      return HTBOTTOMLEFT;

    if (pos.x >= wr.right  - border && pos.x < wr.right  &&
        pos.y >= wr.bottom - border && pos.y < wr.bottom)
      return HTBOTTOMRIGHT;

    if (pos.x >= wr.left            && pos.x < wr.left + border) return HTLEFT;
    if (pos.x >= wr.right - border  && pos.x < wr.right)         return HTRIGHT;
    if (pos.y >= wr.top             && pos.y < wr.top + border)  return HTTOP;
    if (pos.y >= wr.bottom - border && pos.y < wr.bottom)        return HTBOTTOM;
  }
  return HTCLIENT;
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
  err_if(!RegisterClassExW(&wnd_class), "failed register class");

  // enable window manager thread
  _thread = std::thread{[this] {
    _thread_id = GetCurrentThreadId();
    PeekMessageW(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    _wait_post_thread_message_valid.count_down();

    MSG msg{};
    while (true)
    {
      while (GetMessageW(&msg, nullptr, 0, 0))
      {
        if (message_process(msg)) return;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
  }};
}

void WindowManager::destroy() noexcept
{
  PostThreadMessageW(_thread_id, Message_Exit, 0, 0);
  _thread.join();
  err_if(!UnregisterClassW(Class_Name, GetModuleHandleW(nullptr)), "failed unregister class");
}

void WindowManager::create_window() noexcept
{
  // create window
  _window_create_info.handle = CreateWindowExW(
    WS_EX_NOREDIRECTIONBITMAP, Class_Name, nullptr, WS_POPUP,
    _window_create_info.x, _window_create_info.y, _window_create_info.width, _window_create_info.height,
    0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!_window_create_info.handle, "failed to create window");

  // TODO: don't move or resize, use a fullscreen as transparent to move rendered window context, and this window also should be transparent not duplication
  // WARN: mini version of windows: windows 10 2004 Edition
  // exclude the window from desktop duplication
  //err_if(!SetWindowDisplayAffinity(_window_create_info.handle, WDA_EXCLUDEFROMCAPTURE), "failed to exclude window from desktop duplicaiton");

  // init renderer resource
  renderer::MessageQueue::instance()->push(renderer::WindowCreateInfo{ _window_create_info.handle }).wait();

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
  err_if(!PostThreadMessageW(_thread_id, Message_Create_Window, 0, 0), "failed to post message");

  // wait create finish
  _window_create_info.finish.acquire();
}

void WindowManager::destroy_window(HWND handle) const noexcept
{
  PostThreadMessageW(_thread_id, Message_Destroy_Window, reinterpret_cast<WPARAM>(handle), 0);
}

auto WindowManager::message_process(MSG const& msg) noexcept -> bool
{
  switch (msg.message)
  {
  case Message_Exit:
    return true;

  case Message_Create_Window:
    create_window();
    break;
  
  case Message_Destroy_Window:
    err_if(!DestroyWindow(reinterpret_cast<HWND>(msg.wParam)), "failed to destroy window");
    break;
  }
  return false;
}

// TODO: only primary screen
auto WindowManager::screen_size() noexcept -> glm::vec2
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

}