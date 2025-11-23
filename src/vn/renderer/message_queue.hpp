#pragma once

#include "window.hpp"

#include <queue>
#include <variant>

namespace vn { namespace renderer {

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

  struct Message_Create_Window_Render_Resource
  {
    Window window;
    bool   transparent;
  };

  struct Message_Destroy_Window_Render_Resource
  {
    HWND handle;
  };

  struct Message_Update_Window
  { 
    Window window;
  };

  struct Message_Capture_Window
  {
    HWND     handle;
    uint32_t max_width{};
    uint32_t max_height{};
  };

  using Message = std::variant<
    Message_Create_Window_Render_Resource,
    Message_Destroy_Window_Render_Resource,
    Message_Update_Window,
    Message_Capture_Window
  >;

  void send_message(Message const& msg) noexcept { _message_queue.push(msg); }
  void process_messages() noexcept;

private:
  std::queue<Message> _message_queue;
};

}}
