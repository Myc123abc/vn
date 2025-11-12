#include "window_resource.hpp"
#include "renderer.hpp"
#include "config.hpp"
#include "core.hpp"
#include "error_handling.hpp"

#include <algorithm>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void SwapchainResource::init(HWND handle, uint32_t width, uint32_t height, bool is_transparent) noexcept
{
  this->is_transparent = is_transparent;

  auto core     = Core::instance();
  auto renderer = Renderer::instance();

  // set viewport and scissor
  viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };
  scissor  = CD3DX12_RECT{     0,   0,   static_cast<LONG>(width),  static_cast<LONG>(height)  };

  // create swapchain
  ComPtr<IDXGISwapChain1> swapchain;
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = width;
  swapchain_desc.Height           = height;
  swapchain_desc.Format           = dxgi_format(ImageFormat::rgba8_unorm);
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  if (is_transparent)
  {
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    err_if(core->factory()->CreateSwapChainForComposition(core->command_queue(), &swapchain_desc, nullptr, &swapchain),
            "failed to create swapchain for composition");

    // create composition
    err_if(DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&_comp_device)),
            "failed to create composition device");
    err_if(_comp_device->CreateTargetForHwnd(handle, true, &_comp_target),
            "failed to create composition target");
    err_if(_comp_device->CreateVisual(&_comp_visual),
            "failed to create composition visual");
    err_if(_comp_visual->SetContent(swapchain.Get()),
            "failed to bind swapchain to composition visual");
    err_if(_comp_target->SetRoot(_comp_visual.Get()),
            "failed to bind composition visual to target");
    err_if(_comp_device->Commit(),
            "failed to commit composition device");
  }
  else
  {
    err_if(core->factory()->CreateSwapChainForHwnd(core->command_queue(), handle, &swapchain_desc, nullptr, nullptr, &swapchain),
            "failed to create swapchain");

    // disable alt-enter fullscreen
    err_if(core->factory()->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER), "failed to disable alt-enter");
  }
  err_if(swapchain.As(&this->swapchain), "failed to get swapchain4");

  rtv_heap.init(DescriptorHeapType::rtv, Frame_Count);
  for (auto i = 0; i < Frame_Count; ++i)
    swapchain_images[i].init(swapchain.Get(), i).create_descriptor(rtv_heap.pop_handle());

  if (renderer->enable_depth_test)
  {
    dsv_heap.init(DescriptorHeapType::dsv);
    dsv_image.init(ImageType::dsv, ImageFormat::d32, width, height).create_descriptor(dsv_heap.pop_handle());
  }
}

void SwapchainResource::resize(uint32_t width, uint32_t height) noexcept
{
  auto core     = Core::instance();
  auto renderer = Renderer::instance();

  // set viewport and scissor
  viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };
  scissor  = CD3DX12_RECT{     0,   0,   static_cast<LONG>(width),  static_cast<LONG>(height)  };

  // wait gpu finish
  core->wait_gpu_complete();

  // reset swapchain relation resources
  std::ranges::for_each(swapchain_images, [](auto& image) { image.destroy(); });
  if (is_transparent)
    _comp_visual->SetContent(nullptr);

  // resize swapchain
  err_if(swapchain->ResizeBuffers(Frame_Count, width, height, DXGI_FORMAT_UNKNOWN, 0),
          "failed to resize swapchain");

  if (is_transparent)
  {
    // rebind composition resources
    err_if(_comp_visual->SetContent(swapchain.Get()),
            "failed to bind swapchain to composition visual");
    err_if(_comp_device->Commit(),
            "failed to commit composition device");
  }

  for (auto i = 0; i < Frame_Count; ++i)
    swapchain_images[i].resize(swapchain.Get(), i);

  if (renderer->enable_depth_test)
    dsv_image.resize(width, height);
}

void WindowResource::init(Window const& window, bool transparent) noexcept
{
  this->window = window;
  swapchain_resource.init(window.handle(), window.real_width(), window.real_height(), transparent);

  // first frame rendered then display
  Renderer::instance()->add_current_frame_render_finish_proc([handle = window.handle()] { ShowWindow(handle, SW_SHOW); });
}

void SwapchainResource::clear_rtv() noexcept
{
  auto  core            = Core::instance();
  auto  cmd             = core->cmd();
  auto  swapchain_image = current_image();
  auto  rtv_handle      = rtv();

  core->reset_cmd();

  swapchain_image->set_state(cmd, ImageState::render_target);

  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  float constexpr clear_color[4]{};
  cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // record finish, change render target view type to present
  swapchain_image->set_state(cmd, ImageState::present);

  // submit command
  core->submit(cmd);

  // present
  err_if(swapchain->Present(1, 0), "failed to present swapchain");
}

void WindowResource::render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto core     = Core::instance();
  auto renderer = Renderer::instance();

  if (clear_window_self_content)
  {
    swapchain_resource.clear_rtv();
    clear_window_self_content = {};
  }
  else if (clear_fullscreen_window_content)
  {
    renderer->_fullscreen_swapchain_resource.clear_rtv();
    clear_fullscreen_window_content = {};
  }

  auto& current_swapchain_resource = window.is_moving_or_resizing() ? renderer->_fullscreen_swapchain_resource : swapchain_resource;
  auto  cmd                        = core->cmd();
  auto  swapchain_image            = current_swapchain_resource.current_image();
  auto  rtv_handle                 = current_swapchain_resource.rtv();
  auto  dsv_handle                 = D3D12_CPU_DESCRIPTOR_HANDLE{};
  if (renderer->enable_depth_test)
    dsv_handle = current_swapchain_resource.dsv_heap.cpu_handle();

  core->reset_cmd();
  
  // set descriptor heaps
  auto descriptor_heaps = std::array<ID3D12DescriptorHeap*, 1>{ renderer->_cbv_srv_uav_heap.handle() };
  cmd->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());

  // convert render target view from present type to render target type
  swapchain_image->set_state(cmd, ImageState::render_target);

  // set render target view
  if (renderer->enable_depth_test)
    cmd->OMSetRenderTargets(1, &rtv_handle, false, &dsv_handle);
  else
    cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  float constexpr clear_color[4]{};
  cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
  if (renderer->enable_depth_test)
  {
    cmd->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    // set depth range
    cmd->OMSetDepthBounds(0.f, 1.f);
  }

  // window shadow render pass
  //window_shadow_render(cmd, current_swapchain_resource);

  // bind pipeline
  renderer->_pipeline.bind(cmd);

  // set viewport
  cmd->RSSetViewports(1, &current_swapchain_resource.viewport);

  // convert render target view from present type to render target type
  swapchain_image->set_state(cmd, ImageState::render_target);

  // upload data to buffer
  renderer->_frame_buffers[core->frame_index()].upload(cmd, vertices, indices, shape_properties);

  // set descriptors
  auto constants = Constants{};
  constants.window_extent = swapchain_image->extent();
  if (window.is_moving_or_resizing())
  {
    constants.window_pos   = window.real_pos();
    constants.cursor_index = static_cast<uint32_t>(window.cursor_type());
  }
  renderer->_pipeline.set_descriptors(cmd, "constants", constants,
  {
    { "cursor_textures", renderer->get_descriptor("cursors")                          },
    { "buffer",          renderer->get_descriptor("framebuffer", core->frame_index()) },
  });

  // draw
  if (window.is_moving_or_resizing())
  {
    auto rect = window.rect();
    cmd->RSSetScissorRects(1, &rect);
    cmd->DrawIndexedInstanced(indices.size() - 12, 1, 0, 0, 0);
    cmd->RSSetScissorRects(1, &current_swapchain_resource.scissor);
    cmd->DrawIndexedInstanced(12, 1, indices.size() - 12, 0, 0);
  }
  else
  {
    auto rect = current_swapchain_resource.scissor;
    rect.left   += Window::External_Thickness.left;
    rect.right  -= Window::External_Thickness.right;
    rect.top    += Window::External_Thickness.top;
    rect.bottom -= Window::External_Thickness.bottom;
    cmd->RSSetScissorRects(1, &rect);
    cmd->DrawIndexedInstanced(indices.size() - 6, 1, 0, 0, 0);
    cmd->RSSetScissorRects(1, &current_swapchain_resource.scissor);
    cmd->DrawIndexedInstanced(6, 1, indices.size() - 6, 0, 0);
  }

  // record finish, change render target view type to present
  swapchain_image->set_state(cmd, ImageState::present);

  // submit command
  core->submit(cmd);

  // present
  err_if(current_swapchain_resource.swapchain->Present(1, 0), "failed to present swapchain");
}

void WindowResource::window_shadow_render(ID3D12GraphicsCommandList1* cmd, SwapchainResource& current_swapchain_resource) const noexcept
{
  auto renderer        = Renderer::instance();
  auto swapchain_image = current_swapchain_resource.current_image();

  copy(renderer->_uav_clear_heap, "window mask image", renderer->_cbv_srv_uav_heap, "window mask image");
  renderer->_window_mask_image.clear(cmd, renderer->_uav_clear_heap.cpu_handle("window mask image"), renderer->_cbv_srv_uav_heap.gpu_handle("window mask image"));
  renderer->_window_mask_pipeline.bind(cmd);
  renderer->_window_mask_pipeline.set_descriptors(cmd,
    "constants", glm::vec4(window.rect().left, window.rect().top, window.rect().right, window.rect().bottom),
    {{ "window_mask_image", renderer->get_descriptor("window mask image") }});
  cmd->Dispatch((renderer->_window_mask_image.width() + 7) / 8, (renderer->_window_mask_image.height() + 7) / 8, 1);
  return;

#if 0
  renderer->_window_shadow_pipeline.bind(cmd);
  renderer->_window_shadow_pipeline.set_descriptors(cmd, "constants", glm::vec2(window.width, window.height), {{ "image", renderer->get_descriptor("window shadow image") }});
  cmd->Dispatch((renderer->_window_shadow_image.width() + 7) / 8, (renderer->_window_shadow_image.height() + 7) / 8, 1);
  if (window.is_moving_or_resizing())
  {
    auto x      = window.x;
    auto y      = window.y;
    auto left   = uint32_t{};
    auto top    = uint32_t{};
    auto right  = window.width;
    auto bottom = window.height;
    if (window.x < 0)
    {
      x    = 0;
      left = -window.x;
    }
    if (window.y < 0)
    {
      y   = 0;
      top = -window.y;
    }
    copy(cmd, renderer->_window_shadow_image, left, top,
      std::clamp(right,  0u, swapchain_image->width()  - window.x),
      std::clamp(bottom, 0u, swapchain_image->height() - window.y),
      *swapchain_image, x, y);
  }
  else
    copy(cmd, renderer->_window_shadow_image, *swapchain_image);
#endif
}

}}
