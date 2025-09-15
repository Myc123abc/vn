#pragma once

#include "renderer.hpp"

#include <windows.h>

#include <mutex>
#include <deque>
#include <variant>
#include <future>
#include <functional>
#include <algorithm>

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
};

struct WindowResizeInfo
{
  HWND handle;
};

using MessageInfo = std::variant<
  WindowCreateInfo,
  WindowCloseInfo,
  WindowResourceDestroyInfo,
  WindowMinimizedInfo,
  WindowResizeInfo
>;

struct Message
{
  MessageInfo        info;
  std::promise<void> done;
};

class MessageQueue
{
private:
  MessageQueue()                                       = default;
  ~MessageQueue()                                      = default;
public:
  MessageQueue(MessageQueue const&)            = delete;
  MessageQueue(MessageQueue&&)                 = delete;
  MessageQueue& operator=(MessageQueue const&) = delete;
  MessageQueue& operator=(MessageQueue&&)      = delete;

  static auto const instance() noexcept
  {
    static MessageQueue instance;
    return &instance;
  }

  template <typename T>
  auto push(T&& info) noexcept
  {
    Message msg;
    msg.info = std::forward<T>(info);

    std::lock_guard lock(_mutex);

    // if have existing resize of same handle, jump this message
    if constexpr (std::is_same_v<T, WindowResizeInfo>)
    {
      if (auto it = std::ranges::find_if(_messages, [&](auto const& msg)
          {
            return std::holds_alternative<WindowResizeInfo>(msg.info)         &&
                   std::get<WindowResizeInfo>(msg.info).handle == info.handle;
          });
          it != _messages.end())
        return msg.done.get_future();
    }

    auto fut = msg.done.get_future();

    _messages.emplace_back(std::move(msg));

    Renderer::instance()->acquire_render();

    return std::move(fut);
  }

  void pop_all() noexcept;

private:
  std::mutex          _mutex;
  std::deque<Message> _messages;
};

}}