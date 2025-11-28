#include "window_resource.hpp"
#include "renderer.hpp"
#include "config.hpp"
#include "core.hpp"
#include "error_handling.hpp"
#include "window_manager.hpp"

#include <dwmapi.h>

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
  auto screen_size = get_screen_size();
  swapchain_resource.init(window.handle, screen_size.x, screen_size.y, transparent);

  // first frame rendered then display
  Renderer::instance()->add_current_frame_render_finish_proc([handle = window.handle] { ShowWindow(handle, SW_SHOW); });
}

void WindowResource::render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto core            = Core::instance();
  auto renderer        = Renderer::instance();
  auto cmd             = core->cmd();
  auto swapchain_image = swapchain_resource.current_image();

  // reset command
  core->reset_cmd();
  
  // set descriptor heaps
  auto descriptor_heaps = std::array<ID3D12DescriptorHeap*, 1>{ renderer->_cbv_srv_uav_heap.handle() };
  cmd->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());

  // render
  window_content_render(cmd, swapchain_image, vertices, indices, shape_properties);
  if (use_window_thumbnail_pipeline)
  {
    window_thumbnail_render(cmd, &renderer->_thumbnail_images[core->frame_index()]);
    use_window_thumbnail_pipeline = false;
  }
  window_shadow_render(cmd);

  // record finish, change render target view type to present
  swapchain_image->set_state(cmd, ImageState::present);

  // submit command
  core->submit(cmd);

  // present
  err_if(swapchain_resource.swapchain->Present(1, 0), "failed to present swapchain");
}

void WindowResource::window_content_render(ID3D12GraphicsCommandList1* cmd, Image* render_target_image, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto core       = Core::instance();
  auto renderer   = Renderer::instance();
  auto rtv_handle = render_target_image->cpu_handle();
  auto dsv_handle = D3D12_CPU_DESCRIPTOR_HANDLE{};
  if (renderer->enable_depth_test)
    dsv_handle = swapchain_resource.dsv_heap.cpu_handle();

  // set render target view
  if (renderer->enable_depth_test)
    cmd->OMSetRenderTargets(1, &rtv_handle, false, &dsv_handle);
  else
    cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  render_target_image->clear_render_target(cmd);
  if (renderer->enable_depth_test)
  {
    cmd->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    // set depth range
    cmd->OMSetDepthBounds(0.f, 1.f);
  }

  // bind pipeline
  renderer->_pipeline.bind(cmd);

  // set viewport
  cmd->RSSetViewports(1, &swapchain_resource.viewport);

  // upload data to buffer
  renderer->_frame_buffers[core->frame_index()].upload(cmd, vertices, indices, shape_properties);

  // set descriptors
  auto constants = Constants{};
  constants.window_extent = render_target_image->extent();
  constants.window_pos   = window.pos();
  constants.cursor_index = static_cast<uint32_t>(window.cursor_type);
  renderer->_pipeline.set_descriptors(cmd, "constants", constants,
  {
    { "cursor_textures", renderer->get_descriptor("cursors")                          },
    { "buffer",          renderer->get_descriptor("framebuffer", core->frame_index()) },
  });

  // draw
  cmd->RSSetScissorRects(1, &window.rect);
  cmd->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);
}

void WindowResource::window_thumbnail_render(ID3D12GraphicsCommandList1* cmd, Image* render_target_image) noexcept
{
  auto renderer   = Renderer::instance();
  auto rtv_handle = render_target_image->cpu_handle();

  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);
  render_target_image->clear_render_target(cmd);
}

void WindowResource::window_shadow_render(ID3D12GraphicsCommandList1* cmd) const noexcept
{
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

void WindowResource::capture_window(uint32_t max_width, uint32_t max_height) noexcept
{
  auto renderer = Renderer::instance();

  // get the window valid region
  auto screen_size = get_screen_size();
  auto rect        = RECT{};
  rect.left   = std::clamp(window.rect.left,   0l, static_cast<LONG>(screen_size.x));
  rect.top    = std::clamp(window.rect.top,    0l, static_cast<LONG>(screen_size.y));
  rect.right  = std::clamp(window.rect.right,  0l, static_cast<LONG>(screen_size.x));
  rect.bottom = std::clamp(window.rect.bottom, 0l, static_cast<LONG>(screen_size.y));

  // judge wheather need scale down (by window thumbnail pipeline)
  if (rect.right - rect.left > max_width || rect.bottom - rect.top > max_height)
  {
    use_window_thumbnail_pipeline = true;

    // initialize thumbnail image
    if (!thumbnail_width && !thumbnail_height)
      std::ranges::for_each(renderer->_thumbnail_images, [=](auto& image)
      {
        image.init(ImageType::rtv, ImageFormat::rgba8_unorm, max_width, max_height)
             .create_descriptor(renderer->_rtv_heap.pop_handle());
      });
    else
      err_if(max_width != thumbnail_width || max_height != thumbnail_height, "currently, not conside thumbnail extent change case");

    return;
  }

  // readback image
  auto core = Core::instance();
  auto cmd  = core->cmd();
  core->reset_cmd();
  auto [readback_buffer, src_bitmap_view] = swapchain_resource.current_image()->readback(cmd, rect);
  core->submit(cmd);
  core->wait_gpu_complete();

  // create bitmap
  auto dst_bitmap = Win32Bitmap{};
  dst_bitmap.init(max_width, max_height);

  // copy small image
  auto offset_x  = (max_width  - src_bitmap_view.width)  / 2;
  auto offset_y  = (max_height - src_bitmap_view.height) / 2;
  auto row_count = std::min(max_height, src_bitmap_view.height);

  dst_bitmap.view.data += (offset_y * max_width + offset_x) * 4;

  // copy data from readback buffer to bitmap
  for (auto i = 0; i < row_count; ++i)
  {
    memcpy(dst_bitmap.view.data, src_bitmap_view.data, src_bitmap_view.width * 4);
    src_bitmap_view.data += src_bitmap_view.row_byte_size;
    dst_bitmap.view.data += max_width * 4;
  }

  // set thumbnail
  err_if(DwmSetIconicThumbnail(window.handle, dst_bitmap.handle, 0), "failed to set thumbnail");

  dst_bitmap.destroy();
}

}}
