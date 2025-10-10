#include "core.hpp"
#include "../util.hpp"

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

  // create fence resources
  err_if(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)),
          "failed to create fence");
  _fence_event = CreateEvent(nullptr, false, false, nullptr);
  err_if(!_fence_event, "failed to create fence event");
  for (auto& fence_value : _fence_values)
    fence_value = 1;

  // create command allocator and list
  err_if(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocator)),
          "failed to create command allocator");
  err_if(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(&_command_list)),
          "failed to create command list");
  err_if(_command_list->Close(), "failed to close command list");
}

void Core::destroy() const noexcept
{
  CloseHandle(_fence_event);
}

void Core::wait_gpu_complete() noexcept
{
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value), "failed to signal fence");

  // move to next frame
  _frame_index = ++_frame_index % Frame_Count;

  if (_fence->GetCompletedValue() < fence_value)
  {
    // wait until frame is finished
    err_if(_fence->SetEventOnCompletion(fence_value, _fence_event), "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // advance fence
  _fence_values[_frame_index] = fence_value + 1;
}

void Core::move_to_next_frame() noexcept
{
  // get current fence value
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value), "failed to signal fence");
      
  // move to next frame
  _frame_index = ++_frame_index % Frame_Count;

  // wait if next fence not ready
  if (_fence->GetCompletedValue() < _fence_values[_frame_index])
  {
    err_if(_fence->SetEventOnCompletion(_fence_values[_frame_index], _fence_event), "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // update the next frame fence value
  _fence_values[_frame_index] = fence_value + 1;
}

}}