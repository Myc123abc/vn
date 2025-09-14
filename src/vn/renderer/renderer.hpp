#pragma once

#include "../window/window.hpp"

#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <directx/d3dx12.h>

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
  static inline auto Render_Target_View_Descriptor_Size = 0;

  Microsoft::WRL::ComPtr<IDXGIFactory6>             _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>              _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>        _command_queue;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>       _pipeline_state; // TODO: use single pipeline state for multiple swapchains
                                                                     //       and can dynamic change render targets number for increase swapchain of window
                                                                     //       check whether have a feature like vulkan's shader object use for dynamic pipeline
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>       _root_signature; // TODO: impl sdf

  std::thread                                       _thread;
  std::atomic_bool                                  _exit{ false };
  std::binary_semaphore                             _render_acquire{ 0 };

////////////////////////////////////////////////////////////////////////////////
///                            Window Resources 
////////////////////////////////////////////////////////////////////////////////
  
  friend class RendererMessageQueue;

private:
  void create_window_resources(HWND handle) noexcept;
  auto add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>;
  void set_window_minimized(HWND handle, bool is_minimized);

private:
  static constexpr auto Frame_Count = 2;

  struct FrameResource
  {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
  };

  // TODO: use global buffer
  // |            frame 0            |            frame 1            |
  // | window 0 data | window 1 data | window 0 data | window 1 data |
  // window datas is compact data sets
  struct WindowResource
  {
    bool                                         is_minimized{};
	  Window                                       window;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>      swapchain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap; // TODO: make single dynamic descriptor heap
    Microsoft::WRL::ComPtr<ID3D12Resource>       rtvs[Frame_Count];
    CD3DX12_VIEWPORT                             viewport;
    CD3DX12_RECT                                 scissor;

    FrameResource                                frames[Frame_Count];

    void render(
      ID3D12CommandQueue*        command_queue,
      ID3D12GraphicsCommandList* command_list,
      ID3D12PipelineState*       pipeline_state,
      ID3D12RootSignature*       root_signature,
      D3D12_VERTEX_BUFFER_VIEW   vertex_buffer_view,
      uint32_t                   frame_index) noexcept;
  };
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
  Microsoft::WRL::ComPtr<ID3D12Resource> _vertex_buffer; // TODO: single global buffer
  D3D12_VERTEX_BUFFER_VIEW               _vertex_buffer_view;
};

}}