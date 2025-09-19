#pragma once

#include "window_resource.hpp"
#include "memory_allocator.hpp"

#include <d3d11.h>

#include <glm/glm.hpp>

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <semaphore>
#include <array>

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

  void init_pipeline_resources() noexcept;

  void run() noexcept;

  void render() noexcept;

  void wait_gpu_complete() noexcept;
  
  void acquire_render() noexcept { _render_acquire.release(); }

  void create_window_resources(HWND handle) noexcept;
  auto add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>;
  void set_window_minimized(HWND handle) noexcept;
  void window_resize(HWND handle) noexcept;

  void capture_backdrop() noexcept;

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6>             _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>              _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>        _command_queue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _command_allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list;
  
  std::thread                                       _thread;
  std::atomic_bool                                  _exit{ false };
  std::binary_semaphore                             _render_acquire{ 0 };

  std::vector<WindowResource>                       _window_resources;

  std::array<uint64_t, Frame_Count>                 _fence_values;
  uint32_t                                          _frame_index{};
  Microsoft::WRL::ComPtr<ID3D12Fence>               _fence;
  HANDLE                                            _fence_event;

////////////////////////////////////////////////////////////////////////////////
///                          Pipeline Resources
////////////////////////////////////////////////////////////////////////////////

  FrameBuffer                                    _frame_buffer;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>    _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>    _root_signature;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>   _srv_heap;
  Microsoft::WRL::ComPtr<ID3D12Resource>         _backdrop_image;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> _desk_dup;
};

}}