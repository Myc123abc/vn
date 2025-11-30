#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <dxgi1_6.h>

#include <stdint.h>

namespace vn { namespace renderer {

class Core
{
private:
  Core()                       = default;
  ~Core()                      = default;
public:
  Core(Core const&)            = delete;
  Core(Core&&)                 = delete;
  Core& operator=(Core const&) = delete;
  Core& operator=(Core&&)      = delete;

  static auto const instance() noexcept
  {
    static Core instance;
    return &instance;
  }

  void init() noexcept;
  void destroy() const noexcept;

  auto submit(ID3D12GraphicsCommandList1* cmd) noexcept -> uint64_t;

  void wait_gpu_complete()  noexcept;
  
  auto factory()       const noexcept { return _factory.Get();       }
  auto device()        const noexcept { return _device.Get();        }
  auto command_queue() const noexcept { return _command_queue.Get(); }
  auto cmd()           const noexcept { return _cmd.Get();           }
  auto fence()         const noexcept { return _fence.Get();         }
  auto fence_event()   const noexcept { return _fence_event;         }
  auto fence_value()   const noexcept { return _fence_value;         }

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6>              _factory;
  Microsoft::WRL::ComPtr<ID3D12Device2>              _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>         _command_queue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     _cmd_alloc;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> _cmd;
  Microsoft::WRL::ComPtr<ID3D12Fence>                _fence;
  HANDLE                                             _fence_event;
  uint64_t                                           _fence_value{};
};

}}
