#pragma once

#include "window_resource.hpp"

#include <DirectXMath.h>

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

  void run() noexcept;

  void render() noexcept;

  void wait_gpu_complete() noexcept;
  
  void acquire_render() noexcept { _render_acquire.release(); }

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6>             _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>              _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>        _command_queue;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>       _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>       _root_signature; // TODO: impl sdf

  std::thread                                       _thread;
  std::atomic_bool                                  _exit{ false };
  std::binary_semaphore                             _render_acquire{ 0 };

////////////////////////////////////////////////////////////////////////////////
///                            Window Resources 
////////////////////////////////////////////////////////////////////////////////
  
  friend class MessageQueue;

private:
  void create_window_resources(HWND handle) noexcept;
  auto add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>;
  void set_window_minimized(HWND handle) noexcept;
  void window_resize(HWND handle) noexcept;

private:
  std::vector<WindowResource> _window_resources;

  uint64_t                            _fence_values[Frame_Count]{};
  uint32_t                            _frame_index{};
  Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
  HANDLE                              _fence_event;

  struct Vertex
  {
    DirectX::XMFLOAT3 position;
  	DirectX::XMFLOAT4 color;
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> _vertex_buffer;
  D3D12_VERTEX_BUFFER_VIEW               _vertex_buffer_view;
};

}}