#include "window_system.hpp"
#include "../util.hpp"
#include "renderer.hpp"

#include <thread>
#include <span>
#include <algorithm>

namespace
{

constexpr wchar_t Class_Name[] = L"vn::renderer::WindowSystem";

constexpr auto Wnd_Msg_Change_Mouse_Interaction_Region = WM_APP;

auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

auto get_window_region(HWND handle, std::span<RECT> rects)
{
  auto region = CreateRectRgnIndirect(&rects[0]);
  for (auto i = 1; i < rects.size(); ++i)
  {
    auto tmp = CreateRectRgnIndirect(&rects[i]);
    CombineRgn(region, region, tmp, RGN_OR);
    DeleteObject(tmp);
  }
  return region;
}

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

Window::Window(int x, int y, int width, int height)
  : id(generate_id()), x(x), y(y), width(width), height(height)
{
  reset_scissor_rect();
}

void Window::reset_scissor_rect() noexcept
{
  auto screen_size = get_screen_size();
  scissor_rect.left   = std::clamp(x,          0, static_cast<int>(screen_size.x));
  scissor_rect.top    = std::clamp(y,          0, static_cast<int>(screen_size.y));
  scissor_rect.right  = std::clamp(x + width,  0, static_cast<int>(screen_size.x));
  scissor_rect.bottom = std::clamp(y + height, 0, static_cast<int>(screen_size.y));
}

void Window::move(int32_t x, int32_t y) noexcept
{
  this->x += x;
  this->y += y;
  reset_scissor_rect();
}

// FIXME: rewrite resize!!!
void Window::resize(ResizeType type, int x, int y) noexcept
{
  using enum Window::ResizeType;
  auto screen_size = get_screen_size();

  switch (type)
  {
  case none:
    return;

  case left_top:

    this->x = std::clamp(this->x + x, 0, static_cast<int>(scissor_rect.right)  - Resize_Width * 2);
    this->y = std::clamp(this->y + y, 0, static_cast<int>(scissor_rect.bottom) - Resize_Width * 2);
    if (this->x > 0 && this->x < static_cast<int>(scissor_rect.right) - Resize_Width * 2)
      width = std::clamp(width - x, Resize_Width * 2, static_cast<int>(screen_size.x));
    if (this->y > 0 && this->y < static_cast<int>(scissor_rect.bottom) - Resize_Width * 2)
      height = std::clamp(height - y, Resize_Width * 2, static_cast<int>(screen_size.y));
    break;

  case right_top:
    this->y = std::clamp(this->y + y, 0, static_cast<int>(scissor_rect.bottom) - Resize_Width * 2);
    width   = std::clamp(width  + x, Resize_Width * 2, static_cast<int>(screen_size.x));
    if (this->y > 0 && this->y < static_cast<int>(scissor_rect.bottom) - Resize_Width * 2)
      height = std::clamp(height - y, Resize_Width * 2, static_cast<int>(screen_size.y));
    break;

  case left_bottom:
    this->x = std::clamp(this->x + x, 0, static_cast<int>(scissor_rect.right)  - Resize_Width * 2);
    if (this->x > 0 && this->x < static_cast<int>(scissor_rect.right) - Resize_Width * 2)
      width = std::clamp(width - x, Resize_Width * 2, static_cast<int>(screen_size.x));
    height  = std::clamp(height + y, Resize_Width * 2, static_cast<int>(screen_size.y));
    break;
  
  case right_bottom:
    width   = std::clamp(width  + x, Resize_Width * 2, static_cast<int>(screen_size.x));
    height  = std::clamp(height + y, Resize_Width * 2, static_cast<int>(screen_size.y));
    break;

  case left:
    left_offset(x);
    break;

  case right:
    width = std::clamp(width + x, Resize_Width * 2, static_cast<int>(screen_size.x));
    break;

  case top:
    top_offset(y);
    break;

  case bottom:
    height  = std::clamp(height + y, Resize_Width * 2, static_cast<int>(screen_size.y));
    break;
  }
  reset_scissor_rect();
}

auto Window::get_resize_type(POINT const& p) const noexcept
{
  using enum ResizeType;

  if (p.x < scissor_rect.left || p.x > scissor_rect.right ||
      p.y < scissor_rect.top  || p.y > scissor_rect.bottom)
    return none;

  bool left_side   = p.x < x + Resize_Width;
  bool right_side  = p.x > scissor_rect.right - Resize_Width;
  bool top_side    = p.y < y + Resize_Width;
  bool bottom_side = p.y > scissor_rect.bottom - Resize_Width;

  if (top_side)
  {
    if (left_side)  return left_top;
    if (right_side) return right_top;
    return top;
  }
  if (bottom_side)
  {
    if (left_side)  return left_bottom;
    if (right_side) return right_bottom;
    return bottom;
  }
  if (left_side)  return left;
  if (right_side) return right;
  return none;
}

void Window::left_offset(int dx) noexcept
{
  if (dx > 0)
  {
    auto width_bound = width - Resize_Width * 2;
    auto offset = width_bound - dx;
    if (offset >= 0)
    {
      x     += dx;
      width -= dx;
    }
    else
    {
      x    += width_bound;
      width = Resize_Width * 2;
    }
  }
  else
  {
    auto new_x = x + dx;
    if (new_x >= 0)
    {
      x      = new_x;
      width -= dx;
    }
    else
    {
      x     = 0;
      width = scissor_rect.right;
    }
  }
}

void Window::top_offset(int dy) noexcept
{
  if (dy > 0)
  {
    auto height_bound = height - Resize_Height * 2;
    auto offset       = height_bound - dy;
    if (offset >= 0)
    {
      y      += dy;
      height -= dy;
    }
    else
    {
      y           += height_bound;
      height_bound = Resize_Height * 2;
    }
  }
  else
  {
    auto new_y = y + dy;
    if (new_y > 0)
    {
      y       = new_y;
      height -= dy;
    }
    else
    {
      y      = 0;
      height = scissor_rect.bottom;
    }
  }
}

LRESULT CALLBACK WindowSystem::wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param)
{
  auto ws = instance();

  static POINT              last_pos;
  static uint32_t           lm_down_on_widnow_id{};
  static Window::ResizeType resize_type{};

  switch (msg)
  {
  case WM_TIMER:
  {
    if (w_param == 1)
      ws->send_message_to_renderer();
    return 0;
  }

  case WM_SYSCOMMAND:
    return 0;
  
  case WM_LBUTTONDOWN:
  {
    GetCursorPos(&last_pos);
    auto& windows = ws->_window_resources.windows;
    if (auto it = std::ranges::find_last_if(windows, [&](auto const& window)
        {
          auto const& rect = window.scissor_rect;
          return PtInRect(&rect, last_pos);
        });
        it.begin() != windows.end())
    {
      auto window = it.begin();
      lm_down_on_widnow_id = window->id;
      SetWindowRgn(ws->handle(), nullptr, false);

      // move window to topest
      if (window + 1 != windows.end())
      {
        std::ranges::rotate(window, window + 1, windows.end());
        ws->_window_resources_changed = true;
      }

      // resize
      resize_type = window->get_resize_type(last_pos);
    }
    break;
  }

  case WM_LBUTTONUP:
  {
    if (lm_down_on_widnow_id)
    {
      ws->_fullscreen_region_changed = true;
      lm_down_on_widnow_id = {};
      resize_type          = {};
    }
    break;
  }

  case WM_MOUSEMOVE:
  {
    if (lm_down_on_widnow_id)
    {
      auto& windows = ws->_window_resources.windows;
      auto it = std::ranges::find_if(windows, [](auto const& window) { return window.id == lm_down_on_widnow_id; });
      err_if(it == windows.end(), "failed to get the window of left button down");

      POINT cur_pos;
      GetCursorPos(&cur_pos);
      if (resize_type != Window::ResizeType::none)
        it->resize(resize_type, cur_pos.x - last_pos.x, cur_pos.y - last_pos.y);
      else
        it->move(cur_pos.x - last_pos.x, cur_pos.y - last_pos.y);
      last_pos = cur_pos;

      ws->_window_resources_changed  = true;
    }
    break;
  }
  
  }
  return DefWindowProcW(handle, msg, w_param, l_param);
}

void WindowSystem::init() noexcept
{
  std::thread([this]
  {
    _thread_id = GetCurrentThreadId();
    
    // register window class
    WNDCLASSEXW wnd_class{};
    wnd_class.lpszClassName = Class_Name;
    wnd_class.cbSize        = sizeof(wnd_class);
    wnd_class.hInstance     = GetModuleHandleW(nullptr);
    wnd_class.lpfnWndProc   = wnd_proc;
    err_if(!RegisterClassExW(&wnd_class), "failed register class");

    // create fullscreen window
    _screen_size = get_screen_size();
    _handle = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, Class_Name, nullptr, WS_POPUP,
      0, 0, _screen_size.x, _screen_size.y, 0, 0, GetModuleHandleW(nullptr), 0);
    err_if(!_handle,  "failed to create window");

    // exclude window from desktop duplication
    err_if(!SetWindowDisplayAffinity(_handle, WDA_EXCLUDEFROMCAPTURE), "failed to exclude window from desktop duplicaiton");

    // set timer for window resource update
    err_if(!SetTimer(_handle, 1, 0, nullptr), "failed to set timer");

    auto rect   = RECT{};
    auto region = CreateRectRgnIndirect(&rect);
    SetWindowRgn(_handle, region, false);
    ShowWindow(_handle, SW_SHOW);

    // create message queue
    PeekMessageW(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    _message_queue_create_complete.count_down();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
      process_message(msg);
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }).detach();
}

void WindowSystem::send_message_to_renderer() noexcept
{
  auto renderer = Renderer::instance();

  if (_window_resources_changed)
  {
    _window_resources_changed = false;
    renderer->push_message(Renderer::Message_Update_Window_Resource{ _window_resources });
  }

  if (_fullscreen_region_changed)
  {
    _fullscreen_region_changed = false;

    // get new mouse interaction region of fullscreen window
    auto rects = std::vector<RECT>(_window_resources.windows.size());
    for (auto i = 0; i < rects.size(); ++i)
    {
      auto&       rect   = rects[i];
      auto const& window = _window_resources.windows[i];
    
      rect.left   = window.x;
      rect.right  = window.x + window.width;
      rect.top    = window.y;
      rect.bottom = window.y + window.height;
    }
    renderer->push_message(Renderer::Message_Update_Fullscreen_Region{ get_window_region(_handle, rects) });
  }
}

void WindowSystem::process_message(MSG const& msg) noexcept
{
  using namespace vn::renderer;
  switch (msg.message)
  {
  case Wnd_Msg_Change_Mouse_Interaction_Region:
    auto [x, y]          = to_32_bits(msg.wParam);
    auto [width, height] = to_32_bits(msg.lParam);
    process_message_create_window(x, y, width, height);
    break;
  }
}

void WindowSystem::create_window(int x, int y, int width, int height) noexcept
{
  _message_queue_create_complete.wait();
  PostThreadMessageW(_thread_id, Wnd_Msg_Change_Mouse_Interaction_Region, to_64_bits(x, y), to_64_bits(width, height));
}

void WindowSystem::process_message_create_window(int x, int y, int width, int height) noexcept
{
  _window_resources_changed  = true;
  _fullscreen_region_changed = true;
  _window_resources.windows.emplace_back(x, y, width, height);
}

}}