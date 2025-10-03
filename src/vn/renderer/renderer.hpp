#pragma once

#include "window_resource.hpp"

#include <functional>
#include <thread>
#include <atomic>
#include <semaphore>
#include <deque>

namespace vn { namespace renderer {

class Renderer
{
  friend class MessageQueue;
  friend class WindowResource;

private:
  Renderer()                           = default;
  ~Renderer()                          = default;
public:
  Renderer(Renderer const&)            = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer const&) = delete;
  Renderer& operator=(Renderer&&)      = delete;

  static auto const instance() noexcept
  {
    static Renderer instance;
    return &instance;
  }

  void init() noexcept;
  void destroy() noexcept;

  void create_pipeline_resource() noexcept;

  void run() noexcept;

  void update() noexcept;
  void render() noexcept;

  void acquire_render() noexcept { _render_acquire.release(); }

  void add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept;

private:
  std::thread                                 _thread;
  std::atomic_bool                            _exit{ false };
  std::binary_semaphore                       _render_acquire{ 0 };
  std::deque<std::function<bool()>>           _current_frame_render_finish_procs;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

  std::unordered_map<HWND, WindowResource>    _window_resources;
  WindowResource                              _fullscreen_window_resource;
  HWND                                        _moving_window{};
};

}}