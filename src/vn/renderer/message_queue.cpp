#include "message_queue.hpp"
#include "renderer.hpp"
#include "window_manager.hpp"

namespace vn { namespace renderer {

void MessageQueue::process_messages() noexcept
{
  static auto  renderer = Renderer::instance();
  static auto& wr       = renderer->_window_resources;
  static auto  wm       = WindowManager::instance();

  auto removed_windows = std::unordered_set<HWND>{};
  while (!_message_queue.empty())
  {
    auto& msg = _message_queue.front();

    std::visit([&](auto&& data)
    {
      using T = std::decay_t<decltype(data)>;
      if constexpr (std::is_same_v<T, Message_Create_Window_Render_Resource>)
      { 
        wr[data.window.handle].init(data.window, data.transparent);
      }
      else if constexpr (std::is_same_v<T, Message_Destroy_Window_Render_Resource>)
      {
        renderer->add_current_frame_render_finish_proc([old_window_resource = wr[data.handle], handle = data.handle] { DestroyWindow(handle); });
        wr.erase(data.handle);
        removed_windows.emplace(data.handle);
      }
      else if constexpr (std::is_same_v<T, Message_Update_Window>)
      {
        std::swap(wr[data.window.handle].window, data.window);
      }
      else if constexpr (std::is_same_v<T, Message_Capture_Window>)
      {
        if (!removed_windows.contains(data.handle))
          wr[data.handle].capture_window(data.max_width, data.max_height);
      }
      else
        static_assert(false, "unexist message type of renderer");
    }, msg);

    _message_queue.pop();
  }
}

}}
