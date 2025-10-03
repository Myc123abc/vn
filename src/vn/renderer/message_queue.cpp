#include "message_queue.hpp"
#include "renderer.hpp"

namespace vn { namespace renderer {

void MessageQueue::process_messages() noexcept
{
  auto  renderer = Renderer::instance();
  auto& wr       = renderer->_window_resources;

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
        renderer->add_current_frame_render_finish_proc([old_window_resource = wr[data.handle]] {});
        wr.erase(data.handle);
      }
      else if constexpr (std::is_same_v<T, Message_Create_Fullscreen_Window_Render_Resource>)
      {
        renderer->_fullscreen_window_resource.init(data.window);
      }
      else if constexpr (std::is_same_v<T, Message_Begin_Moving_Window>)
      {
        //std::swap(window, data.window);
        //it->moving     = true;
        //_moving_window = data.window.handle;
      }
      else if constexpr (std::is_same_v<T, Message_Moving_Window>)
      {
        //auto it = find_window(data.window.handle);
        //std::swap(it->window, data.window);
      }
      else if constexpr (std::is_same_v<T, Message_End_Moving_Window>)
      {
        //auto it = find_window(_moving_window);
        //it->moving     = {};
        //_moving_window = {};
      }
      else
        static_assert(false, "unexist message type of renderer");
    }, msg);

    _message_queue.pop();
  }
}

}}