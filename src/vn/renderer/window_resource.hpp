#pragma once

#include "config.hpp"
#include "../window/window.hpp"
#include "image.hpp"

#include <dxgi1_6.h>
#include <dcomp.h>
#include <d3d12.h>
#include <directx/d3dx12.h>
#include <wrl/client.h>

#include <array>

namespace vn { namespace renderer {

// TODO: use global buffer
// |            frame 0            |            frame 1            |
// | window 0 data | window 1 data | window 0 data | window 1 data |
// window datas is compact data sets
class WindowResource
{
  friend class Renderer;

public:
  WindowResource(HWND handle) noexcept;

  void render() noexcept;

  void resize() noexcept;

private:
  using RTVImageType = Image<ImageType::rtv, ImageFormat::bgra8_unorm>;

  bool                                           _is_minimized{};
  Window                                         _window;

  Microsoft::WRL::ComPtr<IDXGISwapChain4>        _swapchain;
  Microsoft::WRL::ComPtr<IDCompositionDevice>    _comp_device;
  Microsoft::WRL::ComPtr<IDCompositionTarget>    _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual>    _comp_visual;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>   _rtv_heap;
  std::array<RTVImageType, Frame_Count>          _rtv_images;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _command_allocators[Frame_Count];
  CD3DX12_VIEWPORT                               _viewport;
  CD3DX12_RECT                                   _scissor;
};

}}