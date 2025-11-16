#pragma once

#include "timer.hpp"

namespace vn { namespace ui {

class LerpAnimation
{
public:
  enum class State
  {
    idle,
    running,
    finished,
  };

  void init(Timer* timer, uint32_t duration) noexcept
  {
    _timer    = timer;
    _duration = duration;
  }

  void start() noexcept
  {
    err_if(_state == State::running, "cannot start lerp animation in running");
    _state = State::running;
    _event = _timer->add_single_event(_duration, [&]
    {
      _state = State::finished;
    });
  }

  auto get_lerp() const noexcept
  {
    auto value = 0.f;
    if (_state == State::idle)
      value = 0.f;
    else if (_state == State::running)
      value = _timer->get_progress(_event);
    else if (_state == State::finished)
      value = 1.f;
    return _is_reversed ? 1.f - value : value;
  }

  void reverse() noexcept
  {
    err_if(_state == State::idle, "I don't know when should use reverse on idle");
    if (_state == State::running)
      _timer->set_progress(_event, 1.f - _timer->get_progress(_event));
    else if (_state == State::finished)
      start();
    _is_reversed = !_is_reversed;
  }

  auto state() const noexcept { return _state; }

  auto is_reversed() const noexcept { return _is_reversed; }

  auto update(bool b) noexcept -> LerpAnimation&
  {
    auto is_running = _state != State::idle;
    if (b)
    {
      if (!is_running)
        start();
      else if (_is_reversed)
        reverse();
    }
    else
    {
      if (is_running && !_is_reversed)
        reverse();
    }
    return *this;
  }

  auto update(std::function<bool()> func) noexcept
  {
    auto res = func();
    update(res);
    return res;
  }

private:
  Timer*   _timer{};
  uint32_t _event{};
  State    _state{};
  uint32_t _duration{};
  bool     _is_reversed{};
};

}}
