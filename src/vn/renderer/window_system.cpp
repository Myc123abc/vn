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

LRESULT CALLBACK WindowSystem::wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param)
{
  auto ws = instance();

  switch (msg)
  {
  case WM_TIMER:
    if (w_param == 1)
      ws->send_message_to_renderer();
    return 0;

  case WM_SYSCOMMAND:
    return 0;
  
  case WM_LBUTTONDOWN:
    auto pos = POINT{};
    GetCursorPos(&pos);
    auto& windows = ws->_window_resources.windows;
    if (auto it = std::ranges::find_last_if(windows, [&](auto const& window)
        {
          auto rect = window.rect();
          return PtInRect(&rect, pos);
        });
        it.begin() != windows.end() && it.begin() + 1 != windows.end())
    {
      std::ranges::rotate(it.begin(), it.begin() + 1, windows.end());
      ws->_window_resources_changed = true;
    }
    break;
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

void WindowSystem::create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept
{
  _message_queue_create_complete.wait();
  PostThreadMessageW(_thread_id, Wnd_Msg_Change_Mouse_Interaction_Region, to_64_bits(x, y), to_64_bits(width, height));
}

void WindowSystem::process_message_create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept
{
  _window_resources_changed  = true;
  _fullscreen_region_changed = true;
  _window_resources.windows.emplace_back(x, y, width, height);
}

}}