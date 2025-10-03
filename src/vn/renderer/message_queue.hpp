#pragma once

#include "window.hpp"

#include <rigtorp/SPSCQueue.h>

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
  };

  struct Message_Destroy_Window_Render_Resource
  {
    HWND handle;
  };

  struct Message_Create_Fullscreen_Window_Render_Resource
  {
    Window window;
  };

  struct Message_Begin_Moving_Window { Window window; };
  struct Message_Moving_Window       { Window window; };
  struct Message_End_Moving_Window   {                };

  using Message = std::variant<
    Message_Create_Window_Render_Resource,
    Message_Destroy_Window_Render_Resource,
    Message_Create_Fullscreen_Window_Render_Resource,
    Message_Begin_Moving_Window,
    Message_Moving_Window,
    Message_End_Moving_Window
  >;

  void send_message(Message const& msg) noexcept { return _message_queue.push(msg); }
  void process_messages() noexcept;

private:
  rigtorp::SPSCQueue<Message> _message_queue{ 10 };
};

}}