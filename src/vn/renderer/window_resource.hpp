#pragma once

#include "config.hpp"
#include "../window/window.hpp"
#include "image.hpp"
#include "memory_allocator.hpp"

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
  struct WindowBackdropInfo
  {
    Image<ImageType::rtv, ImageFormat::bgra8_unorm>* image{};
    struct CopyInfo
    {
      RECT                  region{};
      glm::vec<2, uint32_t> offset;
    };
    std::vector<CopyInfo> copy_infos;
  };
  auto get_other_window_backdrop_regions() const noexcept -> std::vector<WindowBackdropInfo>;
  void set_backdrop_image(ID3D12GraphicsCommandList* cmd) noexcept;

private:
  using SwapchainImageType = Image<ImageType::rtv, ImageFormat::bgra8_unorm>;

  bool                                         _is_minimized{};
  Window                                       _window;

  Microsoft::WRL::ComPtr<IDXGISwapChain4>      _swapchain;
  Microsoft::WRL::ComPtr<IDCompositionDevice>  _comp_device;
  Microsoft::WRL::ComPtr<IDCompositionTarget>  _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual>  _comp_visual;

  std::array<SwapchainImageType, Frame_Count>  _swapchain_images;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtv_heap;
  CD3DX12_VIEWPORT                             _viewport;
  CD3DX12_RECT                                 _scissor;

  struct FrameResource
  {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  command_allocator;
    Image<ImageType::srv, ImageFormat::bgra8_unorm> backdrop_image;
    Image<ImageType::uav, ImageFormat::bgra8_unorm> blur_backdrop_image;
    DescriptorHeap<DescriptorHeapType::cbv_srv_uav> heap;
  };
  std::array<FrameResource, Frame_Count> _frames;
};

}}