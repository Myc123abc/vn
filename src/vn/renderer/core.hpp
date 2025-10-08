#pragma once

#include "config.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <dxgi1_6.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

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

  void create_dx12_resource() noexcept;

  auto current_frame_available() const noexcept -> bool;
  void wait_gpu_complete()  noexcept;
  void move_to_next_frame() noexcept;

  auto get_last_fence_value() const noexcept { return _fence_values[_frame_index]; }
  auto queue_fence()          const noexcept { return _queue_fences[_frame_index]; }

  auto handle()          const noexcept { return _instance;            }
  auto physical_device() const noexcept { return _physical_device;     }
  auto device()          const noexcept { return _device;              }
  auto command_pool()    const noexcept { return _command_pool;        }
  auto vma_allocator()   const noexcept { return _vma_allocator;       }
  auto queue()           const noexcept { return _queue;               }
  auto factory()         const noexcept { return _factory.Get();       }
  auto command_queue()   const noexcept { return _command_queue.Get(); }
  auto dxgi_device()     const noexcept { return _dxgi_device.Get();   }
  auto frame_index()     const noexcept { return _frame_index;         }
  auto fence()           const noexcept { return _fence.Get();         }

private:
  VkInstance                                 _instance{};
  VkDebugUtilsMessengerEXT                   _debug_utils_messenger{};
  VkPhysicalDevice                           _physical_device{};
  VkDevice                                   _device{};
  VkQueue                                    _queue{};
  VkCommandPool                              _command_pool{};
  VmaAllocator                               _vma_allocator{};
  Microsoft::WRL::ComPtr<IDXGIFactory6>      _factory;
  Microsoft::WRL::ComPtr<ID3D12Device>       _dxgi_device;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> _command_queue;
  std::array<VkFence, Frame_Count>           _queue_fences{};
  uint32_t                                   _frame_index{};
  std::array<uint64_t, Frame_Count>          _fence_values;
  Microsoft::WRL::ComPtr<ID3D12Fence>        _fence;
  HANDLE                                     _fence_event;
};
  
}}