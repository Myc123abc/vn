#include "renderer.hpp"
#include "renderer_message_queue.hpp"
#include "../util.hpp"
#include "../window/window_manager.hpp"

#include <d3dcompiler.h>

#include <algorithm>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  // init debug controller
#ifndef NDEBUG
  ComPtr<ID3D12Debug> debug_controller;
  exit_if(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)),
          "failed to create d3d12 debug controller");
  debug_controller->EnableDebugLayer();
#endif

  // create factory
#ifndef NDEBUG
  exit_if(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#else
  exit_if(CreateDXGIFactory2(0, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#endif

  // create device
  ComPtr<IDXGIAdapter4> adapter;
  exit_if(_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)),
          "failed to enum dxgi adapter");
  exit_if(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device)),
          "failed to create d3d12 device");

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  exit_if(_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&_command_queue)),
          "failed to create command queue");

  // get render target view descriptor size
  Render_Target_View_Descriptor_Size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // create root signature
  CD3DX12_ROOT_SIGNATURE_DESC signature_desc{};
  signature_desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  exit_if(D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  exit_if(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
          "failed to create root signature");
  
  // create shaders
  ComPtr<ID3DBlob> vertex_shader;
  ComPtr<ID3DBlob> pixel_shader;
#ifndef NDEBUG
  auto compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  auto compile_flag = 0;
#endif
  exit_if(D3DCompileFromFile(L"assets/shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flag, 0, &vertex_shader, nullptr), "failed to compile vertex shader");
  exit_if(D3DCompileFromFile(L"assets/shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, &pixel_shader,  nullptr), "failed to compile pixel shader");

  // create pipeline state
  D3D12_INPUT_ELEMENT_DESC layout[]
  {
    {  "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.InputLayout           = { layout, _countof(layout) };
  pipeline_state_desc.pRootSignature        = _root_signature.Get();
  pipeline_state_desc.VS                    = CD3DX12_SHADER_BYTECODE{ vertex_shader.Get() };
  pipeline_state_desc.PS                    = CD3DX12_SHADER_BYTECODE{ pixel_shader.Get() };
  pipeline_state_desc.RasterizerState       = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
  pipeline_state_desc.BlendState            = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
  pipeline_state_desc.SampleMask            = UINT_MAX;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_desc.NumRenderTargets      = 1;
  pipeline_state_desc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count      = 1;
  exit_if(_device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipeline_state)),
          "failed to create pipeline state");

  // create vertices
  Vertex vertices[]
  {
    { { 0.f,   .5f, 0.f }, { 1.f, 0.f, 0.f, 1.f } },
    { { .5f,  -.5f, 0.f }, { 0.f, 1.f, 0.f, 1.f } },
    { { -.5f, -.5f, 0.f }, { 0.f, 0.f, 1.f, 1.f } },
  };

  // FIXME: it's not to recommand use this, just like vulkan use device buffer to dynamic upload byte data every frame
  //        upload heap should only use to upload single data like static texture
  // create vertex buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
  exit_if(_device->CreateCommittedResource(
    &heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&_vertex_buffer)),
    "failed to create vertex buffer");

  // copy data to vertex buffer
  uint8_t* p;
  auto range = CD3DX12_RANGE{};
  exit_if(_vertex_buffer->Map(0, &range, reinterpret_cast<void**>(&p)),
          "failed to map pointer from vertex buffer");
  memcpy(p, vertices, sizeof(vertices));
  _vertex_buffer->Unmap(0, nullptr);

  // init vertex buffer view
  _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
  _vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
  _vertex_buffer_view.SizeInBytes    = sizeof(vertices);

  run();
}

void Renderer::destroy() noexcept
{
  _exit.store(true, std::memory_order_release);
  _render_acquire.release();
  _thread.join();
  for (auto& window_resource : _window_resources)
    window_resource.wait_gpu_complete(_command_queue.Get(), _fence_event);
  CloseHandle(_fence_event);
}

void Renderer::create_window_resources(HWND handle) noexcept
{
  // create window resource
  WindowResource window_resource{};
  window_resource.window.init(handle);

  // set viewport and scissor
  auto wnd_size = window_resource.window.size();
  window_resource.viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(wnd_size.width), static_cast<float>(wnd_size.height) };
  window_resource.scissor  = CD3DX12_RECT{ 0, 0, static_cast<LONG>(wnd_size.width), static_cast<LONG>(wnd_size.height) };

  // create swapchain
  ComPtr<IDXGISwapChain1> swapchain;
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = wnd_size.width;
  swapchain_desc.Height           = wnd_size.height;
  swapchain_desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  exit_if(_factory->CreateSwapChainForHwnd(_command_queue.Get(), window_resource.window.handle(), &swapchain_desc, nullptr, nullptr, &swapchain),
          "failed to create swapchain");
  exit_if(swapchain.As(&window_resource.swapchain), "failed to get swapchain4");

  // disable some combination keys
  exit_if(_factory->MakeWindowAssociation(window_resource.window.handle(), DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN),
    "failed to set window association");

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  exit_if(_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&window_resource.rtv_heap)),
    "failed to create render target view descriptor heap");

  // create render target views
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ window_resource.rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i <Frame_Count; ++i)
  {
    exit_if(window_resource.swapchain->GetBuffer(i, IID_PPV_ARGS(&window_resource.rtvs[i])),
            "failed to get descriptor");
    _device->CreateRenderTargetView(window_resource.rtvs[i].Get(), nullptr, rtv_handle);
    rtv_handle.Offset(1, Render_Target_View_Descriptor_Size);
  }

  // create frame resources
  for (auto i = 0; i < Frame_Count; ++i)
  {
    // create command allocator
    exit_if(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&window_resource.frames[i].command_allocator)),
            "failed to create command allocator");
  }
  // create command list
  exit_if(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, window_resource.frames[window_resource.frame_index].command_allocator.Get(), _pipeline_state.Get(), IID_PPV_ARGS(&window_resource.command_list)),
          "failed to create command list");
  window_resource.command_list->Close();

  // create fence resources
  exit_if(_device->CreateFence(window_resource.frames[window_resource.frame_index].fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&window_resource.fence)),
          "failed to create fence");
  ++window_resource.frames[window_resource.frame_index].fence_value;
  _fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  exit_if(!_fence_event, "failed to create fence event");

  // add window resources
  _window_resources.emplace_back(std::move(window_resource));
}

void Renderer::WindowResource::wait_gpu_complete(ID3D12CommandQueue* command_queue, HANDLE fence_event) noexcept
{
  // signal fence
  exit_if(command_queue->Signal(fence.Get(), frames[frame_index].fence_value),
          "failed to signal fence");

  // wait until frame is finished
  exit_if(fence->SetEventOnCompletion(frames[frame_index].fence_value, fence_event),
          "failed to set event on completion");
  WaitForSingleObjectEx(fence_event, INFINITE, FALSE);

  // update fence value
  ++frames[frame_index].fence_value;
}

void Renderer::run() noexcept
{
  _thread = std::thread{[this] {
    while (true)
    {
      // wait render acquire
      _render_acquire.acquire();
      if (_exit.load(std::memory_order_acquire)) [[unlikely]]
        return;

      // process all message
      RendererMessageQueue::instance()->pop_all();

      // render
      for (auto& window_resource : _window_resources)
        if (!window_resource.is_minimized) [[unlikely]]
          window_resource.render(_command_queue.Get(), _pipeline_state.Get(), _root_signature.Get(), _vertex_buffer_view, _fence_event);
    }
  }};
}

void Renderer::WindowResource::render(
  ID3D12CommandQueue*      command_queue,
  ID3D12PipelineState*     pipeline_state,
  ID3D12RootSignature*     root_signature,
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view,
  HANDLE                   fence_event) noexcept
{
  // reset command allocator and command list
  exit_if(frames[frame_index].command_allocator->Reset() == E_FAIL, "failed to reset command allocator");
  exit_if(command_list->Reset(frames[frame_index].command_allocator.Get(), pipeline_state), "failed to reset command list");

  // set root signature
  command_list->SetGraphicsRootSignature(root_signature);

  // set viewport and scissor rectangle
  command_list->RSSetViewports(1, &viewport);
  command_list->RSSetScissorRects(1, &scissor);

  // convert render target view from present type to render target type
  auto barrier_begin = CD3DX12_RESOURCE_BARRIER::Transition(rtvs[frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  command_list->ResourceBarrier(1, &barrier_begin);

  // set render target view
  auto rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, Render_Target_View_Descriptor_Size);
  command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

  // clear color
  float constexpr clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };
  command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // set vertex buffer
  command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

  // draw
  command_list->DrawInstanced(3, 1, 0, 0);

  // record finish, change render target view type to present
  auto barrier_end = CD3DX12_RESOURCE_BARRIER::Transition(rtvs[frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  command_list->ResourceBarrier(1, &barrier_end);

  // close command list
  exit_if(command_list->Close(), "failed to close command list");

  // execute command list
  ID3D12CommandList* command_lists[] = { command_list.Get() };
  command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

  // present swapchain
  exit_if(swapchain->Present(1, 0), "failed to present swapchain");

  // get current fence value
  current_fence_value = frames[frame_index].fence_value;
      
  // signal fence
  exit_if(command_queue->Signal(fence.Get(), current_fence_value),
          "failed to signal fence");
      
  // move to next frame
  frame_index = swapchain->GetCurrentBackBufferIndex();

  // wait if next fence not ready
  if (fence->GetCompletedValue() < frames[frame_index].fence_value)
  {
    exit_if(fence->SetEventOnCompletion(frames[frame_index].fence_value, fence_event),
            "failed to set event on completion");
    WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
  }

  // update fence value
  frames[frame_index].fence_value = current_fence_value + 1;
}

auto Renderer::add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>
{
  // find closed window resource
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource.window.handle();
  });
  exit_if(it == _window_resources.end(), "failed to find closed window");

  // add to old resource, wait gpu finish using then destroy
  auto func = [window_resource = *it]
  {
    auto fence_value = window_resource.fence->GetCompletedValue();
    exit_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    return fence_value >= window_resource.current_fence_value;
  };

  // remove old one
  _window_resources.erase(it);

  // destroy window
  WindowManager::instance()->destroy_window(handle);

  return func;
}

void Renderer::set_window_minimized(HWND handle, bool is_minimized)
{
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource.window.handle();
  });
  exit_if(it == _window_resources.end(), "failed to find window");
  it->is_minimized = is_minimized;
}

}}