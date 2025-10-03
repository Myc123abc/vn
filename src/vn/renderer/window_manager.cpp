#include "window_manager.hpp"
#include "../util.hpp"
#include "renderer.hpp"
#include "message_queue.hpp"

namespace
{

constexpr wchar_t Fullscreen_Class[] = L"vn::WindowManager::Fullscreen";
constexpr wchar_t Window_Class[]     = L"vn::WindowManager::Window";

constexpr auto Msg_Create_Window = WM_APP + 0;

auto to_64_bits(uint32_t x, uint32_t y) noexcept
{
  return static_cast<uint64_t>(x) << 32 | y;
}

auto to_32_bits(uint64_t x) noexcept -> std::pair<uint32_t, uint32_t>
{
  return { x >> 32, x & 0xffffffff };
}

}

namespace vn { namespace renderer {

void WindowManager::init() noexcept
{
  _thread = std::thread([this]
  {
    _thread_id = GetCurrentThreadId();

    // register window class
    WNDCLASSEXW wnd_class{};
    wnd_class.lpszClassName = Fullscreen_Class;
    wnd_class.cbSize        = sizeof(wnd_class);
    wnd_class.hInstance     = GetModuleHandleW(nullptr);
    wnd_class.lpfnWndProc   = DefWindowProcW;
    err_if(!RegisterClassExW(&wnd_class), "failed register class");
    wnd_class.lpszClassName = Window_Class;
    wnd_class.lpfnWndProc   = wnd_proc;
    err_if(!RegisterClassExW(&wnd_class), "failed register class");
    
    // create fullscreen
    auto screen_size = get_screen_size();
    auto handle = CreateWindowExW(WS_EX_TOOLWINDOW, Fullscreen_Class, nullptr, WS_POPUP,
      0, 0, screen_size.x, screen_size.y, 0, 0, GetModuleHandleW(nullptr), 0);
    err_if(!handle,  "failed to create window");
    auto rect   = RECT{};
    auto region = CreateRectRgnIndirect(&rect);
    SetWindowRgn(handle, region, false);
    MessageQueue::instance()->send_message(MessageQueue::Message_Create_Fullscreen_Window_Render_Resource{ Window{ handle, 0, 0, screen_size.x, screen_size.y } });
    
    // create message queue
    PeekMessageW(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    _message_queue_create_complete.count_down();
    
    // message loop
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
      if (msg.message == WM_QUIT)
        return;
      process_message(msg);
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  });
}

void WindowManager::destroy() noexcept
{
  PostThreadMessageW(_thread_id, WM_QUIT, 0, 0);
  _thread.join();
}

LRESULT CALLBACK WindowManager::wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept
{
  static auto wm        = WindowManager::instance();
  static auto renderer  = Renderer::instance();
  static auto msg_queue = MessageQueue::instance();

  static POINT              last_pos{};
  static Window::ResizeType resize_type{};
  static bool               lm_down{};
  static bool               moving{};
  static HWND               moving_window{};

  switch (msg)
  {
  case WM_DESTROY:
  {
    msg_queue->send_message(MessageQueue::Message_Destroy_Window_Render_Resource{ handle });
    wm->_window_count.fetch_sub(1, std::memory_order_relaxed);
    wm->_windows.erase(handle);
    return 0;
  }

  case WM_LBUTTONDOWN:
  {
    SetCapture(handle);
    GetCursorPos(&last_pos);
    resize_type = wm->_windows[handle].get_resize_type(last_pos);
    lm_down = true;
    break;
  }

  case WM_LBUTTONUP:
  {
    ReleaseCapture();
    lm_down = false;
    
    if (moving)
    {
      moving        = {};
      moving_window = {};
      msg_queue->send_message(MessageQueue::Message_End_Moving_Window{});
    }
    else if (resize_type != Window::ResizeType::none)
    {

    }
    break;
  }

  case WM_MOUSEMOVE:
  {
    if (lm_down)
    {
      POINT pos;
      GetCursorPos(&pos);
      auto offset_x = pos.x - last_pos.x;
      auto offset_y = pos.y - last_pos.y;

      // window moving
      if (resize_type == Window::ResizeType::none)
      {
        // first moving
        if (!moving)
        {
          wm->_windows[handle].move(offset_x, offset_y);
          msg_queue->send_message(MessageQueue::Message_Begin_Moving_Window{ wm->_windows[handle] });
          moving_window = handle;
        }
        // continuely moving
        else
        {
          wm->_windows[moving_window].move(offset_x, offset_y);
          msg_queue->send_message(MessageQueue::Message_Moving_Window{ wm->_windows[moving_window] });
        }
      }
      // window resizing
      else
      {

      }
      #if 0
      // window move
      if (resize_type == Window::ResizeType::none)
      {
        it->move(offset_x, offset_y);
        // TODO: is use render in fullscreen can have more better fps?
        SetWindowPos(handle, HWND_TOP, it->x, it->y, 0, 0, SWP_NOSIZE);
      }
      // window resize
      else
      {
        it->resize(resize_type, offset_x, offset_y);
        renderer->send_message(Renderer::Message_Resize_Window{ *it });  
      }
      #endif
      last_pos = pos;
    }
    break;
  }
  }
  return DefWindowProcW(handle, msg, w_param, l_param);
}

void WindowManager::process_message(MSG const& msg) noexcept
{
  switch (msg.message)
  {
  case Msg_Create_Window:
    auto [x, y]          = to_32_bits(msg.wParam);
    auto [width, height] = to_32_bits(msg.lParam);
    process_message_create_window(x, y, width, height);
    break;
  }
}

void WindowManager::create_window(int x, int y, uint32_t width, uint32_t height) noexcept
{
  _message_queue_create_complete.wait();
  PostThreadMessageW(_thread_id, Msg_Create_Window, to_64_bits(x, y), to_64_bits(width, height));
  _window_count.fetch_add(1, std::memory_order_relaxed);
}

void WindowManager::process_message_create_window(int x, int y, uint32_t width, uint32_t height) noexcept
{
  auto handle = CreateWindowExW(0, Window_Class, nullptr, WS_POPUP,
    x, y, width, height, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!handle,  "failed to create window");
  auto window = Window{ handle, x, y, width, height };
  _windows.emplace(handle, window);
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Window_Render_Resource{ window });
}

}}