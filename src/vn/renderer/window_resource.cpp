#include "window_resource.hpp"
#include "../util.hpp"
#include "renderer.hpp"

#include <chrono>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

WindowResource::WindowResource(HWND handle) noexcept
{
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
  swapchain_desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
  swapchain_desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  err_if(renderer->_factory->CreateSwapChainForComposition(renderer->_command_queue.Get(), &swapchain_desc, nullptr, &swapchain),
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
  err_if(renderer->_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)),
    "failed to create render target view descriptor heap");

  // create render target views
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i <Frame_Count; ++i)
  {
    err_if(swapchain->GetBuffer(i, IID_PPV_ARGS(&_rtvs[i])),
            "failed to get descriptor");
    renderer->_device->CreateRenderTargetView(_rtvs[i].Get(), nullptr, rtv_handle);
    rtv_handle.Offset(1, Render_Target_View_Descriptor_Size);
  }

  // create frame resources
  for (auto i = 0; i < Frame_Count; ++i)
    // create command allocator
    err_if(renderer->_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocators[i])),
            "failed to create command allocator");
}

void WindowResource::render() noexcept
{
  auto renderer = Renderer::instance();
  auto command_list = renderer->_command_list.Get();
  
  // reset command allocator and command list
  err_if(_command_allocators[renderer->_frame_index]->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(command_list->Reset(_command_allocators[renderer->_frame_index].Get(), renderer->_pipeline_state.Get()), "failed to reset command list");

  // set root signature
  command_list->SetGraphicsRootSignature(renderer->_root_signature.Get());

  // set descriptors
  ID3D12DescriptorHeap* heaps[] = { Renderer::instance()->_srv_heap.Get() };
  command_list->SetDescriptorHeaps(_countof(heaps), heaps);
  command_list->SetGraphicsRootDescriptorTable(1, heaps[0]->GetGPUDescriptorHandleForHeapStart());

  // set viewport and scissor rectangle
  command_list->RSSetViewports(1, &_viewport);
  command_list->RSSetScissorRects(1, &_scissor);

  // get current render target view index
  auto rtv_idx = _swapchain->GetCurrentBackBufferIndex();

  // convert render target view from present type to render target type
  auto barrier_begin = CD3DX12_RESOURCE_BARRIER::Transition(_rtvs[rtv_idx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  command_list->ResourceBarrier(1, &barrier_begin);

  // set render target view
  auto rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), rtv_idx, Render_Target_View_Descriptor_Size);
  command_list->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  float constexpr clear_color[4] = { 0.f, 0.f, 0.f, .5f };
  command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // set vertex buffer
  command_list->IASetVertexBuffers(0, 1, &renderer->_vertex_buffer_view);

  // set constant
  static auto beg = std::chrono::high_resolution_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - beg).count();
  float alpha = abs(sin(3.1415926 / 3000 * dur));
  command_list->SetGraphicsRoot32BitConstant(0, std::bit_cast<uint32_t>(alpha), 0);

  // draw
  command_list->DrawInstanced(6, 1, 0, 0);

  // record finish, change render target view type to present
  auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(_rtvs[rtv_idx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  command_list->ResourceBarrier(1, &barrier_end);

  // close command list
  err_if(command_list->Close(), "failed to close command list");

  // execute command list
  ID3D12CommandList* command_lists[] = { command_list };
  renderer->_command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

  // present swapchain
  err_if(_swapchain->Present(1, 0), "failed to present swapchain");
}

void WindowResource::resize(ID3D12Device* device) noexcept
{
 // reset minimized
  _is_minimized = false;

  // set viewport and scissor rectangle
  auto wnd_size = _window.size();
  _viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(wnd_size.x), static_cast<float>(wnd_size.y) };
  _scissor  = CD3DX12_RECT{ 0, 0, static_cast<LONG>(wnd_size.x), static_cast<LONG>(wnd_size.y) };

  // wait gpu finish
  Renderer::instance()->wait_gpu_complete();
  
  // reset swapchain relation resources
  for (auto i = 0; i < Frame_Count; ++i)
    _rtvs[i].Reset();
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
    err_if(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_rtvs[i])),
            "failed to get descriptor");
    device->CreateRenderTargetView(_rtvs[i].Get(), nullptr, rtv_handle);
    rtv_handle.Offset(1, Render_Target_View_Descriptor_Size);
  }
}

}}