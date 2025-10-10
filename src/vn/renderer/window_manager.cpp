#include "window_manager.hpp"
#include "../util.hpp"
#include "renderer.hpp"
#include "message_queue.hpp"

using namespace vn::renderer;

namespace
{

constexpr wchar_t Fullscreen_Class[] = L"vn::WindowManager::Fullscreen";
constexpr wchar_t Window_Class[]     = L"vn::WindowManager::Window";

constexpr auto Msg_Begin_Use_Fullscreen_Window = WM_APP + 0;
constexpr auto Msg_End_Use_Fullscreen_Window   = WM_APP + 1;

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
  _fullscreen_window_handle = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP, Fullscreen_Class, nullptr, WS_POPUP,
    0, 0, screen_size.x, screen_size.y, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!_fullscreen_window_handle,  "failed to create window");
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Fullscreen_Window_Render_Resource{ Window{ _fullscreen_window_handle, 0, 0, screen_size.x, screen_size.y } });
}

void WindowManager::message_process() noexcept
{
  MSG msg{};
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    process_message(msg);
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

LRESULT CALLBACK WindowManager::wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept
{
  static auto wm        = WindowManager::instance();
  static auto renderer  = Renderer::instance();
  static auto msg_queue = MessageQueue::instance();

  static auto last_pos    = POINT{};
  static auto resize_type = Window::ResizeType{};
  static auto lm_down     = bool{};

  switch (msg)
  {
  case WM_DESTROY:
  {
    msg_queue->send_message(MessageQueue::Message_Destroy_Window_Render_Resource{ handle });
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
    
    auto& window = wm->_windows[handle];

    if (window.moving)
    {
      window.moving      = {};
      wm->_moving_or_resizing_window = window;
      msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
    }
    else if (resize_type != Window::ResizeType::none)
    {
      window.resizing      = {};
      wm->_moving_or_resizing_window = window;
      msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
      msg_queue->send_message(MessageQueue::Message_Resize_window{ window });
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

      auto& window = wm->_windows.at(handle);

      // window moving
      if (resize_type == Window::ResizeType::none)
      {
        // first moving
        if (!window.moving)
        {
          window.move(offset_x, offset_y);
          wm->_moving_or_resizing_window = window;
          msg_queue->send_message(MessageQueue::Message_Begin_Use_Fullscreen_Window{ window });
        }
        // continuely moving
        else
        {
          window.move(offset_x, offset_y);
          msg_queue->send_message(MessageQueue::Message_Update_Window{ window });
        }
      }
      // window resizing
      else
      {
        window.adjust_offset(resize_type, pos, offset_x, offset_y);

        // first resizing
        if (!window.resizing)
        {
          window.resize(resize_type, offset_x, offset_y);
          wm->_moving_or_resizing_window = window;
          msg_queue->send_message(MessageQueue::Message_Begin_Use_Fullscreen_Window{ window });
        }
        // continuely resizing
        else
        {
          window.resize(resize_type, offset_x, offset_y);
          msg_queue->send_message(MessageQueue::Message_Update_Window{ window });
        }
      }
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
  case Msg_Begin_Use_Fullscreen_Window:
  {
    process_begin_use_fullscreen_window();
    break;
  }

  case Msg_End_Use_Fullscreen_Window:
  {
    process_end_use_fullscreen_window();
    break;
  }
  }
}

void WindowManager::create_window(int x, int y, uint32_t width, uint32_t height) noexcept
{
  auto handle = CreateWindowExW(0, Window_Class, nullptr, WS_POPUP,
    x, y, width, height, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!handle,  "failed to create window");
  auto window = Window{ handle, x, y, width, height };
  _windows.emplace(handle, window);
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Window_Render_Resource{ window });
}

void WindowManager::begin_use_fullscreen_window() const noexcept
{
  PostMessageW(_windows.begin()->first, Msg_Begin_Use_Fullscreen_Window, 0, 0);
}

void WindowManager::end_use_fullscreen_window() const noexcept
{
  PostMessageW(_windows.begin()->first, Msg_End_Use_Fullscreen_Window, 0, 0);
}

void WindowManager::process_begin_use_fullscreen_window() noexcept
{
  ShowWindow(_fullscreen_window_handle, SW_SHOW);
  ShowWindow(_moving_or_resizing_window.handle, SW_HIDE);
}

void WindowManager::process_end_use_fullscreen_window() noexcept
{
  SetWindowPos(_moving_or_resizing_window.handle, 0, _moving_or_resizing_window.x, _moving_or_resizing_window.y, _moving_or_resizing_window.width, _moving_or_resizing_window.height, 0);
  ShowWindow(_moving_or_resizing_window.handle, SW_SHOW);
  ShowWindow(_fullscreen_window_handle, SW_HIDE);
}

}}