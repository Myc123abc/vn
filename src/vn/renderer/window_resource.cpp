#include "window_resource.hpp"
#include "renderer.hpp"

#include <algorithm>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void SwapchainResource::init(HWND handle, uint32_t width, uint32_t height, bool transparent) noexcept
{
  this->transparent = transparent;
  
  auto core = Core::instance();

  // set viewport and scissor
  viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };
  scissor  = CD3DX12_RECT{     0,   0,   static_cast<LONG>(width),  static_cast<LONG>(height)  };

  // create swapchain
  ComPtr<IDXGISwapChain1> swapchain;
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = width;
  swapchain_desc.Height           = height;
  swapchain_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
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
    err_if(core->factory()->CreateSwapChainForHwnd(core->command_queue(), handle, &swapchain_desc, nullptr, nullptr, &swapchain),
            "failed to create swapchain");
  err_if(swapchain.As(&this->swapchain), "failed to get swapchain4");

  // disable alt-enter fullscreen
  err_if(core->factory()->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER), "failed to disable alt-enter");

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  err_if(core->device()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)),
    "failed to create render target view descriptor heap");

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i < Frame_Count; ++i)
  {
    // get swapchain image
    swapchain_images[i].init(swapchain.Get(), i)
                       .create_descriptor(rtv_handle);
    // offset next swapchain image
    rtv_handle.Offset(1, RTV_Size);
  }
}

void SwapchainResource::resize(uint32_t width, uint32_t height) noexcept
{
  auto core = Core::instance();

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

  // reget swapchain images
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i < Frame_Count; ++i)
  {
    // get swapchain image
    swapchain_images[i].init(swapchain.Get(), i)
                       .create_descriptor(rtv_handle);
    // offset next swapchain image
    rtv_handle.Offset(1, RTV_Size);
  }
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

  // init frame buffer
  frame_buffer.init(1024);
}

void WindowResource::init(Window const& window) noexcept
{
  this->window = window;
  swapchain_resource.init(window.handle, window.width, window.height);
  frame_resource.init();

  // first frame rendered then display
  Renderer::instance()->add_current_frame_render_finish_proc([handle = window.handle]
  { 
    ShowWindow(handle, SW_SHOW);
    UpdateWindow(handle);
  });
}

void WindowResource::render() noexcept
{
  auto core      = Core::instance();
  auto renderer  = Renderer::instance();
  auto cmd_alloc = frame_resource.command_allocators[core->frame_index()].Get();
  auto cmd       = frame_resource.cmd.Get();

  auto& swapchain_resource = (window.moving || window.resizing) ? renderer->_fullscreen_swapchain_resource : this->swapchain_resource;
  auto  swapchain_image    = swapchain_resource.current_image();
  auto  rtv_handle         = swapchain_resource.rtv();
  
  // reset command allocator and command list
  err_if(cmd_alloc->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(cmd_alloc, nullptr), "failed to reset command list");

  // set pipeline state
  cmd->SetPipelineState(renderer->_pipeline_state.Get());

  // set root signature
  cmd->SetGraphicsRootSignature(renderer->_root_signature.Get());

  // set viewport and scissor rectangle
  cmd->RSSetViewports(1, &swapchain_resource.viewport);
  cmd->RSSetScissorRects(1, &swapchain_resource.scissor);

  // convert render target view from present type to render target type
  swapchain_image->set_state<ImageState::render_target>(cmd);

  // set render target view
  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  float constexpr clear_color[4]{};
  cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // clear frame buffer
  frame_resource.frame_buffer.clear();

  // upload vertices and indices
  frame_resource.frame_buffer.upload(cmd, frame_resource.vertices, frame_resource.indices);

  // set constant
  auto constants = Constants{};
  constants.window_extent = swapchain_image->extent();
  if (window.moving || window.resizing)
    constants.window_pos = window.pos();
  constants.cursor_index = 3;
  cmd->SetGraphicsRoot32BitConstants(0, sizeof(Constants), &constants, 0);

  // set cursor texture
  auto descriptor_heaps = std::array<ID3D12DescriptorHeap*, 1>{ renderer->_srv_heap.handle() };
  cmd->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
  cmd->SetGraphicsRootDescriptorTable(1, renderer->_srv_heap.gpu_handle());

  // draw
  cmd->DrawIndexedInstanced(frame_resource.indices.size(), 1, 0, 0, 0);

  // clear vertices and indices
  frame_resource.vertices.clear();
  frame_resource.indices.clear();
  frame_resource.idx_beg = {};

  // record finish, change render target view type to present
  swapchain_image->set_state<ImageState::present>(cmd);

  // submit command
  core->submit(cmd);

  // present
  err_if(swapchain_resource.swapchain->Present(1, 0), "failed to present swapchain");
}

}}