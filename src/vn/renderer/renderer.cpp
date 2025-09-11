#include "renderer.hpp"
#include "../util.hpp"
#include "../window/window.hpp"

#include <directx/d3dx12.h>
#include <d3dcompiler.h>

using namespace Microsoft::WRL;

namespace vn {

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

  // create command allocator
  ComPtr<ID3D12CommandAllocator> command_allocator;
  exit_if(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)),
          "failed to create command allocator");

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  exit_if(_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)),
          "failed to create render target view descriptor heap");
  Render_Target_View_Descriptor_Size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // create root signature
  ComPtr<ID3D12RootSignature> root_signature;
  CD3DX12_ROOT_SIGNATURE_DESC signature_desc{};
  signature_desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  exit_if(D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  exit_if(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)),
          "failed to create root signature");
  
  // create pipeline state
  ComPtr<ID3DBlob> vertex_shader;
  ComPtr<ID3DBlob> pixel_shader;
#ifndef NDEBUG
  auto compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  auto compile_flag = 0;
#endif
  exit_if(D3DCompileFromFile(L"assets/shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flag, 0, &vertex_shader, nullptr), "failed to compile vertex shader");
  exit_if(D3DCompileFromFile(L"assets/shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, &pixel_shader, nullptr), "failed to compile pixel shader");
}

void Renderer::create_window_resources(Window const& window) noexcept
{
  // disable some combination keys
  exit_if(_factory->MakeWindowAssociation(window.handle(), DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN),
          "failed to set window association");

  // create swapchain
  auto wnd_size = window.size();
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount = Frame_Count;
  swapchain_desc.Width = wnd_size.width;
  swapchain_desc.Height = wnd_size.height;
  swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  ComPtr<IDXGISwapChain1> swapchain;
  exit_if(_factory->CreateSwapChainForHwnd(_command_queue.Get(), window.handle(), &swapchain_desc, nullptr, nullptr, &swapchain),
          "failed to create swapchain");

  // create render target views
  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  ComPtr<ID3D12Resource> rtvs[Frame_Count];
  for (auto i = 0; i <Frame_Count; ++i)
  {
    exit_if(swapchain->GetBuffer(i, IID_PPV_ARGS(&_rtvs[i])),
            "failed to get descriptor");
    _device->CreateRenderTargetView(_rtvs[i].Get(), nullptr, rtv_handle);
    rtv_handle.Offset(1, Render_Target_View_Descriptor_Size);
  }
}

}