#pragma once

#include "window_resource.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <semaphore>

namespace vn { 

void init()    noexcept;
void destroy() noexcept;
void render()  noexcept;
  
}

namespace vn { namespace renderer {

class Renderer
{
  friend void vn::init()    noexcept;
  friend void vn::destroy() noexcept;
  friend void vn::render()  noexcept;
  
  friend class WindowResource;
  friend class MessageQueue;
  friend class FrameBuffer;
  
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

private:
  void init()    noexcept;
  void destroy() noexcept;

  void init_blur_pipeline() noexcept;
  void init_pipeline_resources() noexcept;

  void run() noexcept;

  void render() noexcept;
  
  void acquire_render() noexcept { _render_acquire.release(); }

  void create_window_resources(HWND handle) noexcept;
  auto add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>;
  void set_window_minimized(HWND handle) noexcept;
  void window_resize(HWND handle) noexcept;

  void capture_backdrop() noexcept;

  void sort_windows_by_z_order() noexcept;

private:
  std::thread                                      _thread;
  std::atomic_bool                                 _exit{ false };
  std::binary_semaphore                            _render_acquire{ 0 };

  std::vector<WindowResource>                      _window_resources;

////////////////////////////////////////////////////////////////////////////////
///                          Pipeline Resources
////////////////////////////////////////////////////////////////////////////////

  FrameBuffer                                     _frame_buffer;

  // sdf pipeline, for future
  Microsoft::WRL::ComPtr<ID3D12PipelineState>     _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>     _root_signature;

  // blur pipeline
  Image<ImageType::srv, ImageFormat::bgra8_unorm> _desktop_image;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication>  _desk_dup;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>     _blur_root_signature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>     _blur_pipeline_state;
};

}}