#include "window_manager.hpp"
#include "error_handling.hpp"
#include "renderer.hpp"
#include "message_queue.hpp"
#include "../ui/ui_context.hpp"

using namespace vn::renderer;

namespace {

constexpr wchar_t Fullscreen_Class[] = L"vn::WindowManager::Fullscreen";
constexpr wchar_t Window_Class[]     = L"vn::WindowManager::Window";

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

  static auto last_pos           = glm::vec<2, int>{};
  static auto last_resize_type   = Window::ResizeType{};
  static auto lm_downresize_type = Window::ResizeType{};
  static auto lm_down_pos        = glm::vec<2, int>{};

  auto finish_window_moving_or_resizing = [&]
  {
    auto& window = wm->_windows[handle];
    if (window.mouse_state == MouseState::left_button_down || window.mouse_state == MouseState::left_button_press)
    {
      ReleaseCapture();
      window.mouse_state = MouseState::left_button_up;

      auto& window = wm->_windows[handle];

      if (window.moving)
      {
        window.moving = {};
        BringWindowToTop(handle);
        wm->_moving_or_resizing_window = window;
        msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
        if (window.need_resize_swapchain)
        {
          window.need_resize_swapchain = false;
          msg_queue->send_message(MessageQueue::Message_Resize_Window{ window });
        }
      }
      else if (lm_downresize_type != Window::ResizeType::none)
      {
        window.resizing    = {};
        BringWindowToTop(handle);
        window.cursor_type = {};
        wm->_moving_or_resizing_window = window;
        msg_queue->send_message(MessageQueue::Message_End_Use_Fullscreen_Window{ window });
        msg_queue->send_message(MessageQueue::Message_Resize_Window{ window });
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
    last_pos = get_cursor_pos();
    lm_downresize_type = wm->_windows[handle].get_resize_type(last_pos);
    lm_down_pos = last_pos;
    wm->_windows[handle].mouse_state = MouseState::left_button_down;
    break;
  }

  case static_cast<int>(WindowManager::Message::left_button_press):
  {
    wm->_windows[handle].mouse_state = MouseState::left_button_press;
    return 0;
  }

  case WM_LBUTTONUP:
  {
    finish_window_moving_or_resizing();
    break;
  }

  case WM_MOUSEMOVE:
  {
    // set cursor if can resize
    auto cursor_pos = get_cursor_pos();
    if (auto type = wm->_windows[handle].get_resize_type(cursor_pos);
        type != last_resize_type)
    {
      last_resize_type = type;
      set_cursor(last_resize_type);
    }

    auto& window = wm->_windows[handle];

    // change to mouse pass through when moving on shadow area
    if (window.is_mouse_pass_through_area())
    {
      auto style = GetWindowLong(handle, GWL_EXSTYLE);
      SetWindowLongPtrA(handle, GWL_EXSTYLE, style | WS_EX_TRANSPARENT | WS_EX_LAYERED);
      wm->_using_mouse_pass_through_windows.push_back(handle);
      return 0;
    }

    // move or resize window
    if (window.mouse_state == MouseState::left_button_down || window.mouse_state == MouseState::left_button_press)
    {
      auto offset_x = cursor_pos.x - last_pos.x;
      auto offset_y = cursor_pos.y - last_pos.y;

      if (!offset_x && !offset_y) break;

      // window moving
      if (lm_downresize_type == Window::ResizeType::none)
      {
        // first moving
        if (!window.moving)
        {
          if (window.is_move_area(lm_down_pos.x, lm_down_pos.y))
          {
            window.is_maximized ? window.move_from_maximize(cursor_pos.x, cursor_pos.y) : window.move(offset_x, offset_y);
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
        window.adjust_offset(lm_downresize_type, cursor_pos, offset_x, offset_y);

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
      last_pos = cursor_pos;
    }
    break;
  }

  case WM_SIZE:
  {
    if (!wm->_windows.contains(handle)) break;
    
    auto& window = wm->_windows[handle];

    if (w_param == SIZE_MINIMIZED)
    {
      window.is_minimized = true;
    }
    else if (w_param == SIZE_MAXIMIZED)
    {
      // TODO: it's not smooth, use animation
      window.maximize();
      msg_queue->send_message(MessageQueue::Message_Resize_Window{ window });
      SetWindowPos(handle, 0, window.x, window.y, window.width, window.height, 0);
      return 0;
    }
    else if (w_param == SIZE_RESTORED)
    {
      // because SetWindowPos will send WM_SIZE with SIZE_RESTORED when we use own maximize button
      // so process maximize restore in another place avoid click maximize also restore the window
      window.is_minimized = {};
    }
    break;
  }

  case static_cast<uint32_t>(WindowManager::Message::mouse_idle):
  {
    wm->_windows[handle].mouse_state = MouseState::idle;
    return 0;
  }

  case static_cast<uint32_t>(WindowManager::Message::window_restore_from_maximize):
  {
    auto& window = wm->_windows[handle];
    window.restore();
    msg_queue->send_message(MessageQueue::Message_Resize_Window{ window });
    SetWindowPos(handle, 0, window.x, window.y, window.width, window.height, 0);
    return 0;
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
  _fullscreen_window_handle = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, Fullscreen_Class, nullptr, WS_POPUP,
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

  for (auto& [handle, window] : _windows)
  {
    // clear last frame window dynamic data
    window.move_invalid_area.clear();

    // send WM_LBUTTONDOWN again, to change mouse state to press
    if (window.mouse_state == MouseState::left_button_down)
      PostMessageW(handle, static_cast<int>(Message::left_button_press), 0, 0);
    else if (window.mouse_state == MouseState::left_button_up)
      PostMessageW(handle, static_cast<int>(Message::mouse_idle), 0, 0);
  }

  for (auto it = _using_mouse_pass_through_windows.begin(); it != _using_mouse_pass_through_windows.end();)
  {
    auto handle = *it;
    if (!_windows[handle].is_mouse_pass_through_area())
    {
      SetWindowLongPtrA(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));
      it = _using_mouse_pass_through_windows.erase(it);
    }
    else
      ++it;
  }
}

void WindowManager::process_message(MSG const& msg) noexcept
{
  switch (msg.message)
  {
  case static_cast<int>(Message::begin_use_fullscreen_window):
  {
    process_begin_use_fullscreen_window();
    break;
  }

  case static_cast<int>(Message::end_use_fullscreen_window):
  {
    process_end_use_fullscreen_window();
    break;
  }
  }
}

auto WindowManager::create_window(std::string_view name, int x, int y, uint32_t width, uint32_t height) noexcept -> HWND
{
  auto handle = CreateWindowExW(0, Window_Class, nullptr, WS_POPUP | WS_MINIMIZEBOX,
    x, y, width, height, 0, 0, GetModuleHandleW(nullptr), 0);
  err_if(!handle,  "failed to create window");
  auto window = Window{ handle, name, x, y, width, height};
  _windows.emplace(handle, window);
  MessageQueue::instance()->send_message(MessageQueue::Message_Create_Window_Render_Resource{ window });
  return handle;
}

void WindowManager::begin_use_fullscreen_window() const noexcept
{
  PostMessageW(_windows.begin()->first, static_cast<int>(Message::begin_use_fullscreen_window), 0, 0);
}

void WindowManager::end_use_fullscreen_window() const noexcept
{
  PostMessageW(_windows.begin()->first, static_cast<int>(Message::end_use_fullscreen_window), 0, 0);
}

void WindowManager::process_begin_use_fullscreen_window() noexcept
{
  while (ShowCursor(false) >= 0);
  ShowWindow(_fullscreen_window_handle, SW_SHOW);
  auto rect = RECT{};
  SetWindowRgn(_moving_or_resizing_window.handle, CreateRectRgnIndirect(&rect), false);
  rect = get_maximize_rect();
  ClipCursor(&rect);
}

void WindowManager::process_end_use_fullscreen_window() noexcept
{
  while (ShowCursor(true) < 0);
  SetWindowPos(_moving_or_resizing_window.handle, 0, _moving_or_resizing_window.x, _moving_or_resizing_window.y, _moving_or_resizing_window.width, _moving_or_resizing_window.height, 0);
  SetWindowRgn(_moving_or_resizing_window.handle, nullptr, false);
  ShowWindow(_fullscreen_window_handle, SW_HIDE);
  ClipCursor(nullptr);
}

auto WindowManager::get_window_name(HWND handle) noexcept -> std::string_view
{
  err_if(!_windows.contains(handle), "failed to get name of window");
  return _windows[handle].name;
}

}}
