#pragma once

#include "image.hpp"
#include "shader_type.hpp"
#include "window.hpp"
#include "descriptor_heap.hpp"
#include "config.hpp"

#include <dcomp.h>

#include <array>
#include <span>

namespace vn { namespace renderer {

struct SwapchainResource
{
  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain;
  std::array<Image, Frame_Count>          swapchain_images;
  DescriptorHeap                          rtv_heap;
  DescriptorHeap                          dsv_heap;
  Image                                   dsv_image;
  CD3DX12_VIEWPORT                        viewport;
  CD3DX12_RECT                            scissor;
  bool                                    is_transparent{};

  Image                                   window_shadow_image;
  DescriptorHeap                          uav_heap;

private:
  Microsoft::WRL::ComPtr<IDCompositionDevice> _comp_device;
  Microsoft::WRL::ComPtr<IDCompositionTarget> _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual> _comp_visual;

public:

  void init(HWND handle, uint32_t width, uint32_t height, bool is_transparent = false) noexcept;

  auto current_image() noexcept { return &swapchain_images[swapchain->GetCurrentBackBufferIndex()]; }

  auto rtv() const noexcept
  {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE{
      rtv_heap.cpu_handle(),
      static_cast<int>(swapchain->GetCurrentBackBufferIndex()),
      RTV_Size};
  }

  void resize(uint32_t width, uint32_t height) noexcept;
};

struct FrameResource
{
  std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, Frame_Count> command_allocators;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> cmd;

  void init() noexcept;
};

struct WindowResource
{
  Window            window;
  SwapchainResource swapchain_resource;
  FrameResource     frame_resource;

  void init(Window const& window, bool transparent) noexcept;

  void render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept;
  void window_shadow_render(ID3D12GraphicsCommandList1* cmd, SwapchainResource& current_swapchain_resource) const noexcept;
};

}}
