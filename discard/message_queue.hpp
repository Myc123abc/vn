#pragma once

#include "renderer.hpp"

#include <windows.h>

#include <mutex>
#include <deque>
#include <variant>
#include <functional>
#include <algorithm>
#include <semaphore>

namespace vn { namespace renderer {

struct FullscreenWindowCreateInfo
{
  HWND handle;
};

struct WindowMoveStartInfo
{
  HWND handle;
};
struct WindowMoveEndInfo{};
struct WindowMoveInfo
{
  glm::vec2 offset;
};

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

struct FrameBufferDestroyInfo
{
  std::function<bool()> func;
};

using MessageInfo = std::variant<
  FullscreenWindowCreateInfo,
  WindowMoveStartInfo,
  WindowMoveEndInfo,
  WindowCreateInfo,
  WindowCloseInfo,
  WindowResourceDestroyInfo,
  WindowMinimizedInfo,
  WindowResizeInfo,
  FrameBufferDestroyInfo,
  WindowMoveInfo
>;

struct Message
{
  MessageInfo info;
};

class MessageQueue
{
private:
  MessageQueue()                               = default;
  ~MessageQueue()                              = default;
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
  auto push(T&& info) noexcept -> MessageQueue&
  {
    Message msg;
    msg.info = std::forward<T>(info);

    std::lock_guard lock(_mutex);

    if (exist<WindowResizeInfo>(info) ||
        exist<WindowMoveInfo>(info))
      return *this;

    _messages.emplace_back(std::move(msg));

    Renderer::instance()->acquire_render();

    return *this;
  }

  void pop_all() noexcept;

  void signal() noexcept { _sem.release(); }
  void wait()   noexcept { _sem.acquire(); }

private:
  template <typename MsgType, typename T>
  auto exist(T&& info) noexcept
  {
    if constexpr (std::is_same_v<T, MsgType>)
    {
      if (auto it = std::ranges::find_if(_messages, [&](auto const& msg)
          {
            return std::holds_alternative<MsgType>(msg.info)         &&
                   std::get<MsgType>(msg.info).handle == info.handle;
          });
          it != _messages.end())
        return true;
    }
    return false;
  }

private:
  std::mutex            _mutex;
  std::deque<Message>   _messages;
  std::binary_semaphore _sem{ 0 };
};

}}