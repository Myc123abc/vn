#pragma once

#include "renderer.hpp"

#include <windows.h>

#include <mutex>
#include <deque>
#include <variant>
#include <future>
#include <functional>

namespace vn { namespace renderer {

struct WindowCreateInfo
{
  HWND handle;
};

struct WindowCloseInfo
{
  HWND handle;
};

struct WindowResourceDestroyInfo
{
  std::function<bool()> func;
};

struct WindowMinimizedInfo
{
  HWND handle;
  bool is_minimized{};
};

using MessageInfo = std::variant<
  WindowCreateInfo,
  WindowCloseInfo,
  WindowResourceDestroyInfo,
  WindowMinimizedInfo
>;

struct Message
{
  MessageInfo        info;
  std::promise<void> done;
};

class RendererMessageQueue
{
private:
  RendererMessageQueue()                                       = default;
  ~RendererMessageQueue()                                      = default;
public:
  RendererMessageQueue(RendererMessageQueue const&)            = delete;
  RendererMessageQueue(RendererMessageQueue&&)                 = delete;
  RendererMessageQueue& operator=(RendererMessageQueue const&) = delete;
  RendererMessageQueue& operator=(RendererMessageQueue&&)      = delete;

  static auto const instance() noexcept
  {
    static RendererMessageQueue instance;
    return &instance;
  }

  template <typename T>
  auto push(T&& info) noexcept
  {
    Message msg;
    msg.info = std::forward<T>(info);
    auto fut = msg.done.get_future();
    {
      std::lock_guard lock(_mutex);
      _messages.emplace_back(std::move(msg));
    }
    Renderer::instance()->acquire_render();
    return std::move(fut);
  }

  void pop_all() noexcept;

private:
  std::mutex          _mutex;
  std::deque<Message> _messages;
};

}}