#pragma once

#include "image.hpp"
#include "shader_type.hpp"
#include "window.hpp"
#include "descriptor_heap.hpp"
#include "config.hpp"
#include "buffer.hpp"

#include <dcomp.h>

#include <array>
#include <span>
#include <optional>

namespace vn { namespace renderer {

struct SwapchainResource
{
  static constexpr auto Image_Format = ImageFormat::bgra8_unorm;

  HANDLE                                  waitable_obj{};
  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain;
  std::array<Image, Frame_Count>          swapchain_images;
  DescriptorHeap                          rtv_heap;
  DescriptorHeap                          dsv_heap;
  Image                                   dsv_image;
  CD3DX12_VIEWPORT                        viewport;
  CD3DX12_RECT                            scissor;
  bool                                    is_transparent{};

private:
  static inline Microsoft::WRL::ComPtr<IDCompositionDevice> _comp_device{};

  Microsoft::WRL::ComPtr<IDCompositionTarget> _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual> _comp_visual;

public:
  void init(HWND handle, uint32_t width, uint32_t height, bool is_transparent = false) noexcept;
  void destroy() const noexcept;

  auto current_image() noexcept { return &swapchain_images[swapchain->GetCurrentBackBufferIndex()]; }

  void resize(uint32_t width, uint32_t height) noexcept;
};

struct WindowResource
{
  struct FrameResource
  {
    FrameBuffer                                    buffer;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmd_alloc;
    uint64_t                                       fence_value{};
  };

  Window                                             window;
  SwapchainResource                                  swapchain_resource;
  DescriptorHeap                                     cbv_srv_uav_heap;
  uint32_t                                           frame_index{};
  std::array<FrameResource, Frame_Count>             frame_resources;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> cmd;

  void init(Window const& window, bool transparent) noexcept;
  void destroy() const noexcept;

  void wait_current_frame_render_finish() const noexcept;

  void clear_window() noexcept;
  void render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties, std::optional<Window> fullscreen_target_window = {}) noexcept;
  void present(bool vsync) const noexcept;

  void window_content_render(Image* render_target_image, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties, std::optional<Window> fullscreen_target_window) noexcept;
  void window_shadow_render(ID3D12GraphicsCommandList1* cmd) const noexcept;
};

}}
