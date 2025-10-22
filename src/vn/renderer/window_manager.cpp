#include "window_manager.hpp"
#include "util.hpp"
#include "renderer.hpp"
#include "message_queue.hpp"
#include "../ui/ui_context.hpp"

using namespace vn::renderer;

namespace {

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

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param) noexcept
{
  static auto wm        = WindowManager::instance();
  static auto renderer  = Renderer::instance();
  static auto msg_queue = MessageQueue::instance();

  static auto last_pos           = POINT{};
  static auto last_resize_type   = Window::ResizeType{};
  static auto lm_downresize_type = Window::ResizeType{};
  static auto lm_down_pos        = POINT{};

  auto finish_window_moving_or_resizing = [&]
  {
    auto& window = wm->_windows[handle];
    if (window.left_button_down)
    {
      ReleaseCapture();
      window.left_button_down = false;

      auto& window = wm->_windows[handle];

      if (window.moving)
      {
        window.moving = {};
        wm->_moving_or_resizing_window = window;
        msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
      }
      else if (lm_downresize_type != Window::ResizeType::none)
      {
        window.resizing    = {};
        window.cursor_type = {};
        wm->_moving_or_resizing_window = window;
        msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
        msg_queue->send_message(MessageQueue::Message_Resize_window{ window });
      }
    }
  };

  switch (msg)
  {
  case WM_CANCELMODE:
  {
    if (LOWORD(w_param) == WA_INACTIVE)
    {
      // reset curosr
      set_cursor();
      finish_window_moving_or_resizing();
    }
    break;
  }

  case WM_DESTROY:
  {
    msg_queue->send_message(MessageQueue::Message_Destroy_Window_Render_Resource{ handle });
    ui::UIContext::instance()->windows.erase(handle);
    wm->_windows.erase(handle);
    return 0;
  }

  case WM_LBUTTONDOWN:
  {
    SetCapture(handle);
    GetCursorPos(&last_pos);
    lm_downresize_type = wm->_windows[handle].get_resize_type(last_pos);
    wm->_windows[handle].left_button_down = true;
    lm_down_pos = last_pos;
    break;
  }

  case WM_LBUTTONUP:
  {
    finish_window_moving_or_resizing();
    break;
  }

  case WM_MOUSEMOVE:
  {
    // set cursor if can resize
    auto cursor_pos = POINT{};
    GetCursorPos(&cursor_pos);
    if (auto type = wm->_windows[handle].get_resize_type(cursor_pos);
        type != last_resize_type)
    {
      last_resize_type = type;
      set_cursor(last_resize_type);
    }

    auto& window = wm->_windows[handle];

    // move or resize window
    if (window.left_button_down)
    {
      POINT pos;
      GetCursorPos(&pos);
      auto offset_x = pos.x - last_pos.x;
      auto offset_y = pos.y - last_pos.y;

      // window moving
      if (lm_downresize_type == Window::ResizeType::none)
      {
        // first moving
        if (!window.moving)
        {
          if (window.is_move_area(lm_down_pos.x, lm_down_pos.y))
          {
            window.move(offset_x, offset_y);
            wm->_moving_or_resizing_window = window;
            msg_queue->send_message(MessageQueue::Message_Begin_Use_Fullscreen_Window{ window });
          }
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
        window.adjust_offset(lm_downresize_type, pos, offset_x, offset_y);

        // first resizing
        if (!window.resizing)
        {
          window.resize(lm_downresize_type, offset_x, offset_y);
          wm->_moving_or_resizing_window = window;
          msg_queue->send_message(MessageQueue::Message_Begin_Use_Fullscreen_Window{ window });
        }
        // continuely resizing
        else
        {
          window.resize(lm_downresize_type, offset_x, offset_y);
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
  _fullscreen_window_handle = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP, Fullscreen_Class, nullptr, WS_POPUP,
    0, 0, screen_size.x, screen_size.y, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!_fullscreen_window_handle,  "failed to create window");
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Fullscreen_Window_Render_Resource{ Window{ _fullscreen_window_handle, {}, 0, 0, screen_size.x, screen_size.y } });
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

  // clear last frame window dynamic data
  for (auto& [_, window] : _windows)
  {
    window.move_invalid_area.clear();
  }
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

auto WindowManager::create_window(std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept -> HWND
{
  auto handle = CreateWindowExW(0, Window_Class, nullptr, WS_POPUP,
    x, y, width, height, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!handle,  "failed to create window");
  auto window = Window{ handle, name, x, y, width, height};
  _windows.emplace(handle, window);
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Window_Render_Resource{ window });
  return handle;
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
  while (ShowCursor(false) >= 0);
  ShowWindow(_fullscreen_window_handle, SW_SHOW);
  auto rect = RECT{};
  SetWindowRgn(_moving_or_resizing_window.handle, CreateRectRgnIndirect(&rect), false);
}

void WindowManager::process_end_use_fullscreen_window() noexcept
{
  while (ShowCursor(true) < 0);
  SetWindowPos(_moving_or_resizing_window.handle, 0, _moving_or_resizing_window.x, _moving_or_resizing_window.y, _moving_or_resizing_window.width, _moving_or_resizing_window.height, 0);
  SetWindowRgn(_moving_or_resizing_window.handle, nullptr, false);
  ShowWindow(_fullscreen_window_handle, SW_HIDE);
}

auto WindowManager::get_window_name(HWND handle) noexcept -> std::string_view
{
  err_if(!_windows.contains(handle), "failed to get name of window");
  return _windows[handle].name;
}

}}
