#include "window_resource.hpp"
#include "renderer.hpp"
#include "config.hpp"
#include "core.hpp"
#include "error_handling.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <ranges>

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
  swapchain_desc.Format           = dxgi_format(Image_Format);
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  swapchain_desc.Flags            = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  if (is_transparent)
  {
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    err_if(core->factory()->CreateSwapChainForComposition(core->command_queue(), &swapchain_desc, nullptr, &swapchain),
            "failed to create swapchain for composition");

    // create composition
    if (!_comp_device)
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
    err_if(core->factory()->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES), "failed to disable alt-enter");
  }
  err_if(swapchain.As(&this->swapchain), "failed to get swapchain4");
  this->swapchain->SetMaximumFrameLatency(Frame_Count);
  waitable_obj = this->swapchain->GetFrameLatencyWaitableObject();
  err_if(!waitable_obj, "failed to get waitable object from swapchain");

  for (auto [i, img] : swapchain_images | std::views::enumerate)
    img.init(swapchain.Get(), i);

  if (renderer->enable_depth_test)
    dsv_image.init(ImageType::dsv, ImageFormat::d32, width, height);
}

void SwapchainResource::destroy() noexcept
{
  CloseHandle(waitable_obj);
  std::ranges::for_each(swapchain_images, [](auto& img) { img.destroy(); });
  if (Renderer::instance()->enable_depth_test)
    dsv_image.destroy();
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
  err_if(swapchain->ResizeBuffers(Frame_Count, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING),
          "failed to resize swapchain");

  if (is_transparent)
  {
    // rebind composition resources
    err_if(_comp_visual->SetContent(swapchain.Get()),
            "failed to bind swapchain to composition visual");
    err_if(_comp_device->Commit(),
            "failed to commit composition device");
  }

  for (auto [i, img] : swapchain_images | std::views::enumerate)
    img.resize(swapchain.Get(), i);

  if (renderer->enable_depth_test)
    dsv_image.resize(width, height);
}

void WindowResource::init(Window const& window, bool transparent) noexcept
{
  auto core   = Core::instance();
  auto device = core->device();

  // initialize swapchain resource
  this->window = window;
  swapchain_resource.init(window.handle, window.real_width(), window.real_width(), transparent);

  // create frame resources
  for (auto& frame_resource : frame_resources)
  {
    // create command allocator
    err_if(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame_resource.cmd_alloc)),
            "failed to create command allocator");

    // initialize frame buffer
    frame_resource.buffer.init();
  }

  // create command list
  err_if(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_resources[0].cmd_alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)),
          "failed to create command list");
  err_if(cmd->Close(), "failed to close command list");
}

void WindowResource::destroy() noexcept
{
  swapchain_resource.destroy();
  std::ranges::for_each(frame_resources, [](auto& frame) { frame.buffer.destroy(); });
}

void WindowResource::wait_current_frame_render_finish() const noexcept
{
  auto        core           = Core::instance();
  auto const& frame_resource = frame_resources[frame_index];
  if (core->fence()->GetCompletedValue() < frame_resource.fence_value)
  {
    err_if(core->fence()->SetEventOnCompletion(frame_resource.fence_value, core->fence_event()), "failed to set event on completion");
    auto objs = std::array<HANDLE, 2>{ swapchain_resource.waitable_obj, core->fence_event() };
    WaitForMultipleObjects(objs.size(), objs.data(), true, INFINITE);
  }
  else
    WaitForSingleObjectEx(swapchain_resource.waitable_obj, INFINITE, false);
}

void WindowResource::clear_window() noexcept
{
  auto  core            = Core::instance();
  auto renderer         = Renderer::instance();
  auto& frame_resource  = frame_resources[frame_index];
  auto  swapchain_image = swapchain_resource.current_image();

  wait_current_frame_render_finish();

  // reset command
  err_if(frame_resource.cmd_alloc->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(frame_resource.cmd_alloc.Get(), nullptr), "failed to reset command list");

  auto rtv_handle = swapchain_image->cpu_handle();
  auto dsv_handle = D3D12_CPU_DESCRIPTOR_HANDLE{};
  if (renderer->enable_depth_test)
    dsv_handle = swapchain_resource.dsv_image.cpu_handle();

  // set render target view
  if (renderer->enable_depth_test)
    cmd->OMSetRenderTargets(1, &rtv_handle, false, &dsv_handle);
  else
    cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  swapchain_image->clear_render_target(cmd.Get());
  if (renderer->enable_depth_test)
  {
    cmd->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    // set depth range
    cmd->OMSetDepthBounds(0.f, 1.f);
  }

  // record finish, change render target view type to present
  swapchain_image->set_state(cmd.Get(), ImageState::present);

  // submit command
  frame_resource.fence_value = core->submit(cmd.Get());

  // move to next frame resource
  frame_index = (frame_index + 1) % Frame_Count;
}

void WindowResource::render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties, std::optional<Window> fullscreen_target_window) noexcept
{
  auto  core            = Core::instance();
  auto  renderer        = Renderer::instance();
  auto& frame_resource  = frame_resources[frame_index];
  auto  swapchain_image = swapchain_resource.current_image();

  wait_current_frame_render_finish();

  // reset command
  err_if(frame_resource.cmd_alloc->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(frame_resource.cmd_alloc.Get(), nullptr), "failed to reset command list");

  // set descriptor heaps
  DescriptorHeapManager::instance()->bind_heaps(cmd.Get());

  // render
  window_content_render(swapchain_image, vertices, indices, shape_properties, fullscreen_target_window);
  // window_shadow_render(cmd);

  // record finish, change render target view type to present
  swapchain_image->set_state(cmd.Get(), ImageState::present);

  // submit command
  frame_resource.fence_value = core->submit(cmd.Get());

  // move to next frame resource
  frame_index = (frame_index + 1) % Frame_Count;
}

void WindowResource::present(bool vsync) const noexcept
{
  vsync
    ? err_if(swapchain_resource.swapchain->Present(1, 0), "failed to present swapchain")
    : err_if(swapchain_resource.swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING), "failed to present swapchain");
}

void WindowResource::window_content_render(Image* render_target_image, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties, std::optional<Window> fullscreen_target_window) noexcept
{
  auto renderer   = Renderer::instance();
  auto rtv_handle = render_target_image->cpu_handle();
  auto dsv_handle = D3D12_CPU_DESCRIPTOR_HANDLE{};
  if (renderer->enable_depth_test)
    dsv_handle = swapchain_resource.dsv_image.cpu_handle();

  // set render target view
  if (renderer->enable_depth_test)
    cmd->OMSetRenderTargets(1, &rtv_handle, false, &dsv_handle);
  else
    cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  render_target_image->clear_render_target(cmd.Get());
  if (renderer->enable_depth_test)
  {
    cmd->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    // set depth range
    cmd->OMSetDepthBounds(0.f, 1.f);
  }

  // bind pipeline
  renderer->_sdf_pipeline.bind(cmd.Get());

  // set viewport
  cmd->RSSetViewports(1, &swapchain_resource.viewport);

  // upload data to buffer
  frame_resources[frame_index].buffer.clear().upload(cmd.Get(), vertices, indices, shape_properties);

  // set descriptors
  auto constants = Constants{};
  constants.window_extent = render_target_image->extent();
  constants.window_pos    = window.content_pos();
  if (fullscreen_target_window.has_value())
  {
    constants.window_pos   = fullscreen_target_window->pos();
    constants.cursor_index = g_image_pool[renderer->_cursors[fullscreen_target_window->cursor_type].handle].index();
  }
  renderer->_sdf_pipeline.set_descriptors(cmd.Get(), "constants", constants,
  {
    { "images", g_descriptor_heap_mgr.first_gpu_handle(DescriptorHeapType::cbv_srv_uav) },
    { "buffer", frame_resources[frame_index].buffer.gpu_handle()                        },
  });

  // draw
  if (fullscreen_target_window.has_value())
  {
    cmd->RSSetScissorRects(1, &fullscreen_target_window->rect);
    cmd->DrawIndexedInstanced(indices.size() - 6, 1, 0, 0, 0);
    auto rect = window.real_rect();
    cmd->RSSetScissorRects(1, &rect);
    cmd->DrawIndexedInstanced(6, 1, indices.size() - 6, 0, 0);
  }
  else
  {
    auto rect = RECT{};
    rect.left   = Window_Shadow_Thickness;
    rect.top    = Window_Shadow_Thickness;
    rect.right  = rect.left + window.width;
    rect.bottom = rect.top  + window.height;
    cmd->RSSetScissorRects(1, &rect);
    cmd->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);
  }
}

void WindowResource::window_shadow_render(ID3D12GraphicsCommandList1* cmd) const noexcept
{
#if 0
  auto renderer = Renderer::instance();

  //
  // first, generate window mask image
  //

  // clear mask image
  copy(renderer->_uav_clear_heap, "window mask image", renderer->_cbv_srv_uav_heap, "window mask image");
  renderer->_window_mask_image.clear(cmd, renderer->_uav_clear_heap.cpu_handle("window mask image"), renderer->_cbv_srv_uav_heap.gpu_handle("window mask image"));

  // bind window mask pipeline
  renderer->_window_mask_pipeline.bind(cmd);

  // set descriptors
  renderer->_window_mask_pipeline.set_descriptors(cmd,
    "constants", glm::vec4(window.rect.left, window.rect.top, window.rect.right, window.rect.bottom),
    {{ "window_mask_image", renderer->get_descriptor("window mask image") }});

  // run pipeline
  cmd->Dispatch((renderer->_window_mask_image.width() + 7) / 8, (renderer->_window_mask_image.height() + 7) / 8, 1);

  //
  // then, use window mask image as input of gaussian blur pipeline
  //

  return;

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
