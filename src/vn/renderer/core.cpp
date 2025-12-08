#include "core.hpp"
#include "error_handling.hpp"
#include "descriptor_heap.hpp"

#include <array>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void Core::init() noexcept
{
  // init debug controller
#ifndef NDEBUG
  ComPtr<ID3D12Debug> debug_controller;
  err_if(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)),
          "failed to create d3d12 debug controller");
  debug_controller->EnableDebugLayer();
#endif

  // create factory
#ifndef NDEBUG
  err_if(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#else
  err_if(CreateDXGIFactory2(0, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#endif

  // create device
  ComPtr<IDXGIAdapter4> adapter;
  err_if(_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)),
          "failed to enum dxgi adapter");
  err_if(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device)),
          "failed to create d3d12 device");

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  err_if(_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&_command_queue)),
          "failed to create command queue");

  // get render target view descriptor size
  RTV_Size         = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  CBV_SRV_UAV_Size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  DSV_Size         = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

  // create fence resources
  err_if(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)),
          "failed to create fence");
  _fence_event = CreateEvent(nullptr, false, false, nullptr);
  err_if(!_fence_event, "failed to create fence event");

  // create command allocator and list
  err_if(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc)),
          "failed to create command allocator");
  err_if(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc.Get(), nullptr, IID_PPV_ARGS(&_cmd)),
          "failed to create command list");
  err_if(_cmd->Close(), "failed to close command list");
}

void Core::destroy() const noexcept
{
  CloseHandle(_fence_event);
}

void Core::reset_cmd() const noexcept
{
  err_if(_cmd_alloc->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(_cmd->Reset(_cmd_alloc.Get(), nullptr), "failed to reset command list");
}

auto Core::submit(ID3D12GraphicsCommandList1* cmd) noexcept -> uint64_t
{
  // close and execute command list
  err_if(cmd->Close(), "failed to close command list");
  auto cmds = std::array<ID3D12CommandList*, 1>{ cmd };
  _command_queue->ExecuteCommandLists(cmds.size(), cmds.data());

  // signal to command queue
  err_if(_command_queue->Signal(_fence.Get(), ++_fence_value), "failed to signal fence");

  return _fence_value;
}

void Core::wait_gpu_complete() noexcept
{
  err_if(_command_queue->Signal(_fence.Get(), _fence_value), "failed to signal fence");
  err_if(_fence->SetEventOnCompletion(_fence_value, _fence_event), "failed to set event on completion");
  WaitForSingleObjectEx(_fence_event, INFINITE, false);
}

auto Core::signal() noexcept -> uint64_t
{
  err_if(_command_queue->Signal(_fence.Get(), ++_fence_value), "failed to signal fence");
  return _fence_value;
}

}}
