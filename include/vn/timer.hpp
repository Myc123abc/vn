#pragma once

#include "error_handling.hpp"

#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace vn {

class Timer
{
private:
  struct Event
  {
    enum class Type
    {
      single,
      repeat
    };

    static auto generic_id() noexcept
    {
      static auto id_generic = 0;
      return id_generic++;
    }

    void start() noexcept
    {
      time_point = std::chrono::steady_clock::now();
    }

    auto get_duration() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time_point).count();
    }

    auto get_progress() const noexcept
    {
      return std::clamp(static_cast<float>(get_duration()) / duratoin, 0.f, 1.f);
    }

    auto process() noexcept
    {
      if (type == Event::Type::single)
      {
        if (get_duration() >= duratoin)
        {
          func();
          return true;
        }
      }
      else if (type == Event::Type::repeat)
      {
        if (get_duration() > duratoin)
        {
          func();
          start();
          return false;
        }
      }
      if (iter_func) iter_func(get_progress());
      return false;
    }

    void set_progress(float progress) noexcept
    {
      time_point = std::chrono::steady_clock::now() - std::chrono::milliseconds(static_cast<uint32_t>(duratoin * progress));
    }

    uint32_t                                           id{};
    Type                                               type{};
    std::function<void()>                              func;
    std::chrono::time_point<std::chrono::steady_clock> time_point;
    uint32_t                                           duratoin{};
    std::function<void(float)>                         iter_func{};
  };

public:
  Timer()                        = default;
  ~Timer()                       = default;
  Timer(Timer const&)            = delete;
  Timer(Timer&&)                 = delete;
  Timer& operator=(Timer const&) = delete;
  Timer& operator=(Timer&&)      = delete;

  void remove_event(uint32_t id) noexcept
  {
    err_if(!_events.contains(id), "time event {} is not exist!", id);
    _events.erase(id);
  }

  auto contains(uint32_t id) const noexcept { return _events.contains(id); }

  auto add_repeat_event(uint32_t duration, std::function<void()> func, std::function<void(float)> iter_func = {}) noexcept
  {
    err_if(!func, "cannot set empty function in repeat time event");
    auto event = Event{};
    event.id        = Event::generic_id();
    event.type      = Event::Type::repeat;
    event.func      = func;
    event.duratoin  = duration;
    event.iter_func = iter_func;
    _events[event.id] = event;
    _events[event.id].start();
    return event.id;
  }

  auto add_single_event(uint32_t duration, std::function<void()> func, std::function<void(float)> iter_func = {}) noexcept
  {
    err_if(!func, "cannot set empty function in repeat time event");
    auto event = Event{};
    event.id        = Event::generic_id();
    event.type      = Event::Type::single;
    event.func      = func;
    event.duratoin  = duration;
    event.iter_func = iter_func;
    _events[event.id] = event;
    _events[event.id].start();
    return event.id;
  }

  void process_events() noexcept
  {
    for (auto it = _events.begin(); it != _events.end();)
    {
      if (it->second.process())
        it = _events.erase(it);
      else
        ++it;
    }
  }

  auto get_progress(uint32_t id) const noexcept
  {
    err_if(!_events.contains(id), "time event {} is not exist!", id);
    return _events.at(id).get_progress();
  }

  auto set_progress(uint32_t id, float progress) noexcept
  {
    err_if(!_events.contains(id), "time event {} is not exist!", id);
    return _events.at(id).set_progress(progress);
  }

  auto is_finished(uint32_t id) const noexcept
  {
    return get_progress(id) == 1.f;
  }

private:
  std::unordered_map<uint32_t, Event> _events;
};

}
