#pragma once

#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>

namespace vn {

class Window;

class Renderer
{
  friend void init()    noexcept;
  friend void destroy() noexcept;
  
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

  void create_window_resources(Window const& window) noexcept;

private:
  void init() noexcept;

private:
  static constexpr auto Frame_Count                        = 2;
  static inline    auto Render_Target_View_Descriptor_Size = 0;

  Microsoft::WRL::ComPtr<IDXGIFactory6>        _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>         _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>   _command_queue;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtv_heap;
  Microsoft::WRL::ComPtr<ID3D12Resource>       _rtvs[Frame_Count];
};

}