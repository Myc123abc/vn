#include "message_queue.hpp"
#include "renderer.hpp"
#include "window_manager.hpp"

namespace vn { namespace renderer {

void MessageQueue::process_messages() noexcept
{
  static auto  renderer = Renderer::instance();
  static auto& wr       = renderer->_window_resources;
  static auto  wm       = WindowManager::instance();

  while (!_message_queue.empty())
  {
    auto msg = *_message_queue.front();

    std::visit([&](auto&& data)
    {
      using T = std::decay_t<decltype(data)>;
      if constexpr (std::is_same_v<T, Message_Create_Window_Render_Resource>)
      {
        wr[data.window.handle].init(data.window);
      }
      else if constexpr (std::is_same_v<T, Message_Destroy_Window_Render_Resource>)
      {
        renderer->add_current_frame_render_finish_proc([old_window_resource = wr[data.handle]] mutable { old_window_resource.destroy(); });
        wr.erase(data.handle);
      }
      else if constexpr (std::is_same_v<T, Message_Create_Fullscreen_Window_Render_Resource>)
      {
        renderer->_fullscreen_swapchain_resource.init(data.window.handle, data.window.width, data.window.height, true);
      }
      else if constexpr (std::is_same_v<T, Message_Begin_Use_Fullscreen_Window>)
      {
        renderer->add_current_frame_render_finish_proc([&]
        {
          wm->begin_use_fullscreen_window();
        });
        std::swap(renderer->_window_resources[data.window.handle].window, data.window);
      }
      else if constexpr (std::is_same_v<T, Message_Update_Window>)
      {
        std::swap(renderer->_window_resources[data.window.handle].window, data.window);
      }
      else if constexpr (std::is_same_v<T, Message_End_Use_Fullscreen_Window>)
      {
        renderer->add_current_frame_render_finish_proc([&]
        {
          wm->end_use_fullscreen_window();
        });
        std::swap(renderer->_window_resources[data.window.handle].window, data.window);
      }
      else if constexpr (std::is_same_v<T, Message_Resize_window>)
      {
        renderer->_window_resources[data.window.handle].swapchain_resource.resize(data.window.width, data.window.height);
      }
      else
        static_assert(false, "unexist message type of renderer");
    }, msg);

    _message_queue.pop();
  }
}

}}