#pragma once

#include "config.hpp"
#include "memory_allocator.hpp"
#include "window.hpp"

#include <wrl/client.h>
#include <dcomp.h>

#include <vulkan/vulkan.h>

#include <array>

namespace vn { namespace renderer {

struct SwapchainResource
{
  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain{};
  std::array<Image, Frame_Count>          images{};
  bool                                    transparent{};

private:
  Microsoft::WRL::ComPtr<IDCompositionDevice>  _comp_device;
  Microsoft::WRL::ComPtr<IDCompositionTarget>  _comp_target;
  Microsoft::WRL::ComPtr<IDCompositionVisual>  _comp_visual;

public:
  void init(HWND handle, uint32_t width, uint32_t height, bool transparent = false) noexcept;
  void destroy() noexcept;

  auto current_image() noexcept { return &images[swapchain->GetCurrentBackBufferIndex()]; }

  void resize(uint32_t width, uint32_t height) noexcept;
};

struct FrameResource
{
  std::array<VkCommandBuffer, Frame_Count> cmds;
  FrameBuffer                              buffer;
  std::vector<Vertex>                      vertices;
  std::vector<uint16_t>                    indices;
  uint16_t                                 idx_beg{};

  void init() noexcept;
  void destroy() const noexcept;
};

struct WindowResource
{
  Window            window;
  SwapchainResource swapchain_resource;
  FrameResource     frame_resource;

  void init(Window const& window) noexcept;

  void destroy() noexcept
  {
    swapchain_resource.destroy();
    frame_resource.destroy();
  }

  void render() noexcept;
};

}}