#include "window_resource.hpp"
#include "../util.hpp"
#include "core.hpp"
#include "renderer.hpp"

#undef Use_Compositor

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

WindowResource::WindowResource(HWND handle) noexcept
{
  auto core     = Core::instance();
  auto renderer = Renderer::instance();

  _window.init(handle);

  // set viewport and scissor
  auto wnd_size = _window.size();
  _viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(wnd_size.x), static_cast<float>(wnd_size.y) };
  _scissor  = CD3DX12_RECT{ 0, 0, static_cast<LONG>(wnd_size.x), static_cast<LONG>(wnd_size.y) };

  // create swapchain
  ComPtr<IDXGISwapChain1> swapchain;
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = wnd_size.x;
  swapchain_desc.Height           = wnd_size.y;
#ifdef Use_Compositor
  swapchain_desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
#endif
  swapchain_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
#ifdef Use_Compositor
  err_if(core->factory()->CreateSwapChainForComposition(core->command_queue(), &swapchain_desc, nullptr, &swapchain),
          "failed to create swapchain for composition");
#else
  err_if(core->factory()->CreateSwapChainForHwnd(core->command_queue(), handle, &swapchain_desc, nullptr, nullptr, &swapchain),
          "failed to create swapchain");
#endif
  err_if(swapchain.As(&_swapchain), "failed to get swapchain4");

#ifdef Use_Compositor
  // create composition
  err_if(DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&_comp_device)),
          "failed to create composition device");
  err_if(_comp_device->CreateTargetForHwnd(_window.handle(), true, &_comp_target),
          "failed to create composition target");
  err_if(_comp_device->CreateVisual(&_comp_visual),
          "failed to create composition visual");
  err_if(_comp_visual->SetContent(swapchain.Get()),
          "failed to bind swapchain to composition visual");
  err_if(_comp_target->SetRoot(_comp_visual.Get()),
          "failed to bind composition visual to target");
  err_if(_comp_device->Commit(),
          "failed to commit composition device");
#endif

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  err_if(core->device()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)),
    "failed to create render target view descriptor heap");

  // create frame resources
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i < Frame_Count; ++i)
  {
    auto& frame = _frames[i];

    // get swapchain image
    _swapchain_images[i].init(swapchain.Get(), i)
                        .create_descriptor(rtv_handle);
    // offset next swapchain image
    rtv_handle.Offset(1, RTV_Size);

    // get command allocator
    err_if(core->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.command_allocator)),
        "failed to create command allocator");

    // create backdrop image and blur backdrop image
    frame.backdrop_image.init(wnd_size.x, wnd_size.y);
    frame.blur_backdrop_image.init(wnd_size.x, wnd_size.y);

    // create descriptors
    frame.heap.init(2);
    frame.backdrop_image.create_descriptor(frame.heap.pop_handle());
    frame.blur_backdrop_image.create_descriptor(frame.heap.pop_handle());
  }
}

#undef USE_BLUR

void WindowResource::render() noexcept
{
  auto  core     = Core::instance();
  auto  cmd      = core->cmd();
  auto  renderer = Renderer::instance();
  auto& frame    = _frames[core->frame_index()];
  
  // reset command allocator and command list
  err_if(frame.command_allocator->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(frame.command_allocator.Get(), nullptr), "failed to reset command list");

  // get current swapchain
  auto& swapchain_image = _swapchain_images[_swapchain->GetCurrentBackBufferIndex()];

#ifdef USE_BLUR
  // blur render
  cmd->SetComputeRootSignature(renderer->_blur_root_signature.Get());
  cmd->SetPipelineState(renderer->_blur_pipeline_state.Get());

  auto heap = frame.heap.handle();
  cmd->SetDescriptorHeaps(1, &heap);
  cmd->SetComputeRootDescriptorTable(0, frame.heap.gpu_handle());
#endif
  // get backdrop image
  set_backdrop_image(cmd);

#ifdef USE_BLUR
  auto screen_size = get_screen_size();
  cmd->Dispatch((screen_size.x + 15) / 16, (screen_size.y + 15) / 16, 1);

  // copy blur backdrop image to swapchain image
  copy(cmd, frame.blur_backdrop_image, swapchain_image);
#else
  copy(cmd, frame.backdrop_image, swapchain_image);
#endif
  // set pipeline state
  cmd->SetPipelineState(renderer->_pipeline_state.Get());

  // set root signature
  cmd->SetGraphicsRootSignature(renderer->_root_signature.Get());

  // set descriptors
  //cmd->SetGraphicsRootDescriptorTable(1, heaps[0]->GetGPUDescriptorHandleForHeapStart());

  // set viewport and scissor rectangle
  cmd->RSSetViewports(1, &_viewport);
  cmd->RSSetScissorRects(1, &_scissor);

  // convert render target view from present type to render target type
  swapchain_image.set_state<ImageState::render_target>(cmd);

  // set render target view
  auto rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), _swapchain->GetCurrentBackBufferIndex(), RTV_Size);
  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  //float constexpr clear_color[4] = { 0.f, 0.f, 0.f, .5f };
  //cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // create vertices
  std::vector<Vertex> vertices
  {
    { {  0.f,  1.f }, {}, {1, 0, 0, 1} },
    { {  1.f, -1.f }, {}, {0, 1, 0, 1} },
    { { -1.f, -1.f }, {}, {0, 0, 1, 1} },
  };

  std::vector<uint16_t> indices
  {
    0, 1, 2,
  };

  // upload vertices
  Renderer::instance()->_frame_buffer.upload(cmd, vertices, indices);

  // set constant
  cmd->SetGraphicsRoot32BitConstant(0, std::bit_cast<uint32_t>(1), 0);

  // draw
  cmd->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);

  // record finish, change render target view type to present
  swapchain_image.set_state<ImageState::present>(cmd);

  // close command list
  err_if(cmd->Close(), "failed to close command list");

  // execute command list
  ID3D12CommandList* cmds[] = { cmd };
  core->command_queue()->ExecuteCommandLists(_countof(cmds), cmds);

  // present swapchain
  err_if(_swapchain->Present(1, 0), "failed to present swapchain");
}

void WindowResource::resize() noexcept
{
  auto core = Core::instance();

  // reset minimized
  _is_minimized = false;

  // set viewport and scissor rectangle
  auto wnd_size = _window.size();
  _viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(wnd_size.x), static_cast<float>(wnd_size.y) };
  _scissor  = CD3DX12_RECT{ 0, 0, static_cast<LONG>(wnd_size.x), static_cast<LONG>(wnd_size.y) };

  // wait gpu finish
  core->wait_gpu_complete();
  
  // reset swapchain relation resources
  for (auto i = 0; i < Frame_Count; ++i)
    _swapchain_images[i].destroy();
  _comp_visual->SetContent(nullptr);

  // resize swapchain
  err_if(_swapchain->ResizeBuffers(Frame_Count, wnd_size.x, wnd_size.y, DXGI_FORMAT_UNKNOWN, 0),
          "failed to resize swapchain");

#ifdef Use_Compositor
  // rebind composition resources
  err_if(_comp_visual->SetContent(_swapchain.Get()),
          "failed to bind swapchain to composition visual");
  err_if(_comp_device->Commit(),
          "failed to commit composition device");
#endif

  // recreate images
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i <Frame_Count; ++i)
  {
    _swapchain_images[i].init(_swapchain.Get(), i)
                  .create_descriptor(rtv_handle);
    rtv_handle.Offset(1, RTV_Size);
    _frames[i].backdrop_image.init(wnd_size.x, wnd_size.y);
    _frames[i].blur_backdrop_image.init(wnd_size.x, wnd_size.y);
  }
}

void WindowResource::set_backdrop_image(ID3D12GraphicsCommandList* cmd) noexcept
{
  auto  renderer = Renderer::instance();
  auto& frame    = _frames[Core::instance()->frame_index()];

  auto rect = _window.rect();
  auto desk_rect = RECT{ 0, 0, static_cast<LONG>(renderer->_desktop_image.width()), static_cast<LONG>(renderer->_desktop_image.height()) };
  if (is_rect_intersect(desk_rect, rect))
  {
    uint32_t x{}, y{};

    if (rect.left < 0)
    {
      x -= rect.left;
      rect.left = 0;
    }
    if (rect.right > desk_rect.right)
    {
      rect.right = desk_rect.right;
    }
    if (rect.top < 0)
    {
      y -= rect.top;
      rect.top = 0;
    }
    if (rect.bottom > desk_rect.bottom)
    {
      rect.bottom = desk_rect.bottom;
    }

    copy(cmd, renderer->_desktop_image, rect.left, rect.top, rect.right, rect.bottom, frame.backdrop_image, x, y);

    for (auto& [image, copy_infos] : get_other_window_backdrop_regions())
    {
      for (auto const& [rect, offset] : copy_infos)
      {
        copy(cmd, *image, rect.left, rect.top, rect.right, rect.bottom, frame.backdrop_image, offset.x, offset.y);
      }
    }
  }
}

auto WindowResource::get_other_window_backdrop_regions() const noexcept -> std::vector<WindowBackdropInfo>
{
  auto backdrop_infos = std::vector<WindowBackdropInfo>{};
  auto renderer       = Renderer::instance();
  auto core           = Core::instance();

  auto middle_window_rects = std::vector<RECT>{};

  auto handle = _window.handle();
  while (true)
  {
    // get backdrop window
    handle = GetWindow(handle, GW_HWNDNEXT);
    if (!handle) break;

    // whether visible and intersect
    if (IsWindowVisible(handle))
    {
      middle_window_rects.emplace_back(window_rect(handle));
    }

    // whether created window
    auto it = std::ranges::find_if(renderer->_window_resources, [this, handle](auto const& window_resource)
    {
      return window_resource._window.handle() == handle;
    });
    if (it == renderer->_window_resources.end())
      continue;

    // get intersection region
    if (auto res = _window.intersect_region(it->_window); res)
    {
      auto const& intersection_region = res.value();
      
      auto info = WindowBackdropInfo{};

      // get split visible regions between middle windows
      for (auto const& rect : rect_difference(intersection_region, { middle_window_rects.begin(), middle_window_rects.end() - 1 }))
      {
        info.copy_infos.emplace_back
        (
          it->_window.convert_to_window_coordinate(rect),
          _window.convert_to_window_coordinate(rect.left, rect.top)
        );
      }

      // set copy image
      info.image = &it->_swapchain_images[core->frame_index()];

      backdrop_infos.emplace_back(info);      
    }
  }

  return backdrop_infos;
}

}}