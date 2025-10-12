#pragma once

#include "config.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <dxgi1_6.h>

#include <array>

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

  void submit(ID3D12GraphicsCommandList * cmd) const noexcept;

  void wait_gpu_complete()  noexcept;
  void move_to_next_frame() noexcept;

  auto get_last_fence_value() const noexcept { return _fence_values[_frame_index]; }

  auto factory()       const noexcept { return _factory.Get();       }
  auto device()        const noexcept { return _device.Get();        }
  auto command_queue() const noexcept { return _command_queue.Get(); }
  auto cmd()           const noexcept { return _command_list.Get();  }
  auto fence()         const noexcept { return _fence.Get();         }
  auto frame_index()   const noexcept { return _frame_index;         }

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6>             _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>              _device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>        _command_queue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _command_allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list;
  uint32_t                                          _frame_index{};
  std::array<uint64_t, Frame_Count>                 _fence_values;
  Microsoft::WRL::ComPtr<ID3D12Fence>               _fence;
  HANDLE                                            _fence_event;
};
  
}}