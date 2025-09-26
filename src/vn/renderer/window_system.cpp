#include "window_system.hpp"
#include "../util.hpp"

#include <thread>
#include <span>

namespace
{

constexpr wchar_t Class_Name[] = L"vn::renderer::WindowSystem";

constexpr auto Wnd_Msg_Change_Mouse_Interaction_Region = WM_APP;

LRESULT CALLBACK wnd_proc(HWND handle, UINT msg, WPARAM w_param, LPARAM l_param)
{
  switch (msg)
  {
  case WM_SYSCOMMAND:
    return 0;
  }
  return DefWindowProcW(handle, msg, w_param, l_param);
}

auto get_screen_size() noexcept -> glm::vec<2, uint32_t>
{
  return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

void change_mouse_interaction_region(HWND handle, std::span<RECT> rects)
{
  if (rects.empty()) return;
//_redIconCheckPeriod.StartDate = _redIconCheckPeriod.StartDate.AddDays(-23)
  auto region = CreateRectRgnIndirect(&rects[0]);
  for (auto i = 1; i < rects.size(); ++i)
  {
    auto tmp = CreateRectRgnIndirect(&rects[i]);
    CombineRgn(region, region, tmp, RGN_OR);
    DeleteObject(tmp);
  }

  SetWindowRgn(handle, region, false);
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

    ShowWindow(_handle, SW_SHOW);

    // create message queue
    PeekMessageW(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    _message_queue_create_complete.count_down();

    MSG msg{};
    while (true)
    {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
      {
        process_message(msg);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }

      if (_updated)
      {
        _updated = false;

        auto tmp = *_data;

        _updated_data.store(_data, std::memory_order_relaxed);
        _data = _data == _data0.get() ? _data1.get() : _data0.get();

        *_data = tmp;
      }
    }
  }).detach();
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
  _updated = true;
  _data->windows.emplace_back(x, y, width, height);

  auto rects = std::vector<RECT>(_data->windows.size());
  for (auto i = 0; i < rects.size(); ++i)
  {
    auto&       rect   = rects[i];
    auto const& window = _data->windows[i];

    rect.left   = window.x;
    rect.right  = window.x + window.width;
    rect.top    = window.y;
    rect.bottom = window.y + window.height;
  }
  change_mouse_interaction_region(_handle, rects);
}

}}