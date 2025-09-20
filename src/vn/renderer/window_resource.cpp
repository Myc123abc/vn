#include "window_resource.hpp"
#include "../util.hpp"
#include "../window/window_manager.hpp"
#include "core.hpp"
#include "renderer.hpp"

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

WindowResource::WindowResource(HWND handle) noexcept
{
  auto core = Core::instance();

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
  swapchain_desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
  swapchain_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  err_if(core->factory()->CreateSwapChainForComposition(core->command_queue(), &swapchain_desc, nullptr, &swapchain),
          "failed to create swapchain for composition");
  err_if(swapchain.As(&_swapchain), "failed to get swapchain4");

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

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  err_if(core->device()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)),
    "failed to create render target view descriptor heap");

  // create render target views
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i <Frame_Count; ++i)
  {
    _rtv_images[i].init(swapchain.Get(), i)
                  .create_descriptor(rtv_handle);
    rtv_handle.Offset(1, RTV_Size);
  }

  // create frame resources
  for (auto i = 0; i < Frame_Count; ++i)
    // create command allocator
    err_if(core->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocators[i])),
            "failed to create command allocator");
}

void WindowResource::render() noexcept
{
  auto core     = Core::instance();
  auto cmd      = core->cmd();
  auto renderer = Renderer::instance();
  
  // reset command allocator and command list
  err_if(_command_allocators[core->frame_index()]->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(_command_allocators[core->frame_index()].Get(), nullptr), "failed to reset command list");

  // get current render target view index
  auto rtv_idx = _swapchain->GetCurrentBackBufferIndex();

  // blur render
  cmd->SetComputeRootSignature(renderer->_blur_root_signature.Get());
  cmd->SetPipelineState(renderer->_blur_pipeline_state.Get());

  ID3D12DescriptorHeap* heaps[] = { renderer->_srv_uav_heap.Get() };
  cmd->SetDescriptorHeaps(_countof(heaps), heaps);
  auto heap_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE{ renderer->_srv_uav_heap->GetGPUDescriptorHandleForHeapStart() };
  cmd->SetComputeRootDescriptorTable(0, heap_handle);

  auto screen_size = WindowManager::instance()->screen_size();
  cmd->Dispatch((screen_size.x + 15) / 16, (screen_size.y + 15) / 16, 1);

#if 1
  // copy uav to rtv
  auto rect = _window.rect_coord();
  copy(cmd, renderer->_uav_image, { rect.left, rect.top, rect.right, rect.bottom }, _rtv_images[rtv_idx]);
  _rtv_images[rtv_idx].set_state<ImageState::present>(cmd);

#else
  // set pipeline state
  cmd->SetPipelineState(core->pipeline_state.Get());

  // set root signature
  cmd->SetGraphicsRootSignature(core->root_signature.Get());

  // set descriptors
  cmd->SetGraphicsRootDescriptorTable(1, heaps[0]->GetGPUDescriptorHandleForHeapStart());

  // set viewport and scissor rectangle
  cmd->RSSetViewports(1, &_viewport);
  cmd->RSSetScissorRects(1, &_scissor);

  // convert render target view from present type to render target type
  auto barrier_begin = CD3DX12_RESOURCE_BARRIER::Transition(_rtvs[rtv_idx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  cmd->ResourceBarrier(1, &barrier_begin);

  // set render target view
  auto rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), rtv_idx, RTV_Size);
  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  float constexpr clear_color[4] = { 0.f, 0.f, 0.f, .5f };
  cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // create vertices
  auto uvs = _window.uv_rect_coord();
  std::vector<Vertex> vertices
  {
    { { -1.f,  1.f }, uvs.left_top,     {1, 1, 1, 1} },
    { {  1.f,  1.f }, uvs.right_top,    {1, 1, 1, 1} },
    { { -1.f, -1.f }, uvs.left_bottom,  {1, 1, 1, 1} },
    { {  1.f, -1.f }, uvs.right_bottom, {1, 1, 1, 1} },
  };

  std::vector<uint16_t> indices
  {
    0, 1, 2,
    2, 1, 3,
  };

  // upload vertices
  Renderer::instance()->_frame_buffer.upload(cmd, vertices, indices);

  // set constant
  static auto beg = std::chrono::high_resolution_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - beg).count();
  float alpha = abs(sin(3.1415926 / 3000 * dur));
  cmd->SetGraphicsRoot32BitConstant(0, std::bit_cast<uint32_t>(alpha), 0);

  // draw
  cmd->DrawIndexedInstanced(indices.size(), 1, 0, 0, 0);

  // record finish, change render target view type to present
  auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(_rtvs[rtv_idx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  cmd->ResourceBarrier(1, &barrier_end);
#endif

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
    _rtv_images[i].destroy();
  _comp_visual->SetContent(nullptr);

  // resize swapchain
  err_if(_swapchain->ResizeBuffers(Frame_Count, wnd_size.x, wnd_size.y, DXGI_FORMAT_UNKNOWN, 0),
          "failed to resize swapchain");

  // rebind composition resources
  err_if(_comp_visual->SetContent(_swapchain.Get()),
          "failed to bind swapchain to composition visual");
  err_if(_comp_device->Commit(),
          "failed to commit composition device");

  // recreate render target views
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i <Frame_Count; ++i)
  {
    _rtv_images[i].init(_swapchain.Get(), i)
                  .create_descriptor(rtv_handle);
    rtv_handle.Offset(1, RTV_Size);
  }
}

}}