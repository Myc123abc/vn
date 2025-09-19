#include "message_queue.hpp"

#include <iterator>

namespace vn { namespace renderer {

void MessageQueue::pop_all() noexcept
{
  std::lock_guard lock(_mutex);
  std::deque<Message> new_messages;
  for (auto it = _messages.begin(); it != _messages.end();)
  {
    std::visit([&](auto&& info)
    {
      using T = std::decay_t<decltype(info)>;
      if constexpr (std::is_same_v<T, WindowCreateInfo>)
      {
        Renderer::instance()->create_window_resources(info.handle);
        signal();
        it = _messages.erase(it);
      }
      else if constexpr (std::is_same_v<T, WindowCloseInfo>)
      {
        new_messages.emplace_back(WindowResourceDestroyInfo{ Renderer::instance()->add_closed_window_resources(info.handle) });
        it = _messages.erase(it);
      }
      else if constexpr (std::is_same_v<T, WindowResourceDestroyInfo>)
      {
        if (info.func())
          it = _messages.erase(it);
        else
        {
          ++it;
          Renderer::instance()->acquire_render();
        }
      }
      else if constexpr (std::is_same_v<T, WindowMinimizedInfo>)
      {
        Renderer::instance()->set_window_minimized(info.handle);
        it = _messages.erase(it);
      }
      else if constexpr (std::is_same_v<T, WindowResizeInfo>)
      {
        Renderer::instance()->window_resize(info.handle);
        it = _messages.erase(it);
      }
      else if constexpr (std::is_same_v<T, FrameBufferDestroyInfo>)
      {
        if (info.func())
          it = _messages.erase(it);
        else
        {
          ++it;
          Renderer::instance()->acquire_render();
        }
      }
      else
        static_assert(false, "[MessageQueue] unknow message type");
    }, it->info);
  }
  std::ranges::move(new_messages, std::back_inserter(_messages));
}

}}