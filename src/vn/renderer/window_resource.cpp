#include "window_resource.hpp"
#include "renderer.hpp"
#include "config.hpp"

#include <algorithm>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void SwapchainResource::init(HWND handle, uint32_t width, uint32_t height, bool transparent) noexcept
{
  this->transparent = transparent;

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
  swapchain_desc.Format           = swapchain_images[0].format();
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  if (transparent)
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

  rtv_heap.init();
  for (auto i = 0; i < Frame_Count; ++i)
    swapchain_images[i].init(swapchain.Get(), i).create_descriptor(rtv_heap.pop_handle());

  if (renderer->enable_depth_test)
  {
    dsv_heap.init();
    dsv_image.init(width, height).create_descriptor(dsv_heap.pop_handle());
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
  if (transparent)
    _comp_visual->SetContent(nullptr);

  // resize swapchain
  err_if(swapchain->ResizeBuffers(Frame_Count, width, height, DXGI_FORMAT_UNKNOWN, 0),
          "failed to resize swapchain");

  if (transparent)
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

void FrameResource::init() noexcept
{
  auto core = Core::instance();

  for (auto i = 0; i < Frame_Count; ++i)
  {
    // get command allocator
    err_if(core->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])),
        "failed to create command allocator");
  }

  // create command list
  err_if(core->device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0].Get(), nullptr, IID_PPV_ARGS(&cmd)),
          "failed to create command list");
  err_if(cmd->Close(), "failed to close command list");
}

void WindowResource::init(Window const& window, bool transparent) noexcept
{
  this->window = window;
  swapchain_resource.init(window.handle, window.width, window.height, transparent);
  frame_resource.init();

  // first frame rendered then display
  Renderer::instance()->add_current_frame_render_finish_proc([handle = window.handle]
  {
    ShowWindow(handle, SW_SHOW);
    UpdateWindow(handle);
  });
}

void WindowResource::render(std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto core      = Core::instance();
  auto renderer  = Renderer::instance();
  auto cmd_alloc = frame_resource.command_allocators[core->frame_index()].Get();
  auto cmd       = frame_resource.cmd.Get();

  auto& swapchain_resource = (window.moving || window.resizing) ? renderer->_fullscreen_swapchain_resource : this->swapchain_resource;
  auto  swapchain_image    = swapchain_resource.current_image();
  auto  rtv_handle         = swapchain_resource.rtv();
  auto  dsv_handle         = D3D12_CPU_DESCRIPTOR_HANDLE{};
  if (renderer->enable_depth_test)
    dsv_handle = swapchain_resource.dsv_heap.cpu_handle();

  // reset command allocator and command list
  err_if(cmd_alloc->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(cmd_alloc, nullptr), "failed to reset command list");

  // bind pipeline
  renderer->_pipeline.bind(cmd);

  // set viewport and scissor rectangle
  cmd->RSSetViewports(1, &swapchain_resource.viewport);
  if (window.moving || window.resizing)
    cmd->RSSetScissorRects(1, &window.rect);
  else
    cmd->RSSetScissorRects(1, &swapchain_resource.scissor);

  // convert render target view from present type to render target type
  swapchain_image->set_state<ImageState::render_target>(cmd);

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

  // set primitive topology
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // set descriptor heaps
  auto descriptor_heaps = std::array<ID3D12DescriptorHeap*, 1>{ renderer->_cbv_srv_uav_heap.handle() };
  cmd->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());

  // upload data to buffer
  renderer->_frame_buffers[core->frame_index()].upload(cmd, vertices, indices, shape_properties);

  // set descriptors
  auto constants = Constants{};
  constants.window_extent = swapchain_image->extent();
  if (window.moving || window.resizing)
  {
    constants.window_pos   = window.pos();
    constants.cursor_index = static_cast<uint32_t>(window.cursor_type);
  }
  renderer->_pipeline.set_descriptors(cmd, constants,
  {
    (window.moving || window.resizing) ? renderer->get_descriptor("cursors") : D3D12_GPU_DESCRIPTOR_HANDLE{},
    renderer->get_descriptor("framebuffer", core->frame_index()),
  });

  // draw
  if (window.moving || window.resizing)
  {
    cmd->DrawIndexedInstanced(indices.size() - 6, 1, 0, 0, 0);
    cmd->RSSetScissorRects(1, &swapchain_resource.scissor);
    cmd->DrawIndexedInstanced(6, 1, indices.size() - 6, 0, 0);
  }
  else
    cmd->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);

  // record finish, change render target view type to present
  swapchain_image->set_state<ImageState::present>(cmd);

  // submit command
  core->submit(cmd);

  // present
  err_if(swapchain_resource.swapchain->Present(1, 0), "failed to present swapchain");
}

}}
