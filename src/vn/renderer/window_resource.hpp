#pragma once

#include "image.hpp"
#include "shader_type.hpp"
#include "window.hpp"

#include <dcomp.h>

#include <span>

namespace vn { namespace renderer {

struct SwapchainResource
{
  using SwapchainImageType = Image<ImageType::rtv, ImageFormat::bgra8_unorm>;

  Microsoft::WRL::ComPtr<IDXGISwapChain4>      swapchain;
  std::array<SwapchainImageType, Frame_Count>  swapchain_images;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
  CD3DX12_VIEWPORT                             viewport;
  CD3DX12_RECT                                 scissor;
  bool                                         transparent;

private:
  Microsoft::WRL::ComPtr<IDCompositionDevice>  _comp_device;
  Microsoft::WRL::ComPtr<IDCompositionTarget>  _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual>  _comp_visual;

public:

  void init(HWND handle, uint32_t width, uint32_t height, bool transparent = false) noexcept;

  auto current_image() noexcept { return &swapchain_images[swapchain->GetCurrentBackBufferIndex()]; }

  auto rtv() const noexcept
  {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE{
      rtv_heap->GetCPUDescriptorHandleForHeapStart(),
      static_cast<int>(swapchain->GetCurrentBackBufferIndex()),
      RTV_Size};
  }

  void resize(uint32_t width, uint32_t height) noexcept;
};

struct FrameResource
{
  std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, Frame_Count> command_allocators;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd;

  void init() noexcept;
};

struct WindowResource
{
  Window            window;
  SwapchainResource swapchain_resource;
  FrameResource     frame_resource;

  void init(Window const& window) noexcept;

  void render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept;
};

}}
