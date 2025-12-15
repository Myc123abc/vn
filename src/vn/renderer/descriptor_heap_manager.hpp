#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <unordered_map>
#include <functional>

namespace vn { namespace renderer {

inline auto RTV_Size         = 0u;
inline auto CBV_SRV_UAV_Size = 0u;
inline auto DSV_Size         = 0u;

enum class DescriptorHeapType
{
  cbv_srv_uav,
  rtv,
  dsv,
};

class DescriptorHandle
{
  friend class DescriptorHeapManager;
public:
  auto cpu_handle() const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE;
  auto gpu_handle() const noexcept -> D3D12_GPU_DESCRIPTOR_HANDLE;

  void release() noexcept;

  auto is_valid() const noexcept { return _index >= 0; }

  auto index() const noexcept { return _index; }

private:
  int                   _index{ -1 };
  DescriptorHeapType    _type;
  std::function<void()> _recreate_descriptor_func;
};

class DescriptorHeapManager
{
  friend class DescriptorHandle;

  class DescriptorHeap
  {
    friend class DescriptorHandle;
    friend class DescriptorHeapManager;
  public:
    DescriptorHeap()                                 = default;
    ~DescriptorHeap()                                = default;
    DescriptorHeap(DescriptorHeap const&)            = delete;
    DescriptorHeap(DescriptorHeap&&)                 = delete;
    DescriptorHeap& operator=(DescriptorHeap const&) = delete;
    DescriptorHeap& operator=(DescriptorHeap&&)      = delete;

    void init(DescriptorHeapType type, uint32_t capacity) noexcept;
    auto pop_handle(std::function<void()> recreate_descriptor_func) noexcept -> DescriptorHandle;
    void reserve(uint32_t capacity) noexcept;
    auto usable_handle_count() const noexcept -> uint32_t;

  private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>   _heap;
    std::vector<std::pair<bool, DescriptorHandle>> _handles;
    DescriptorHeapType                             _type;
  };

private:
  DescriptorHeapManager()                                        = default;
  ~DescriptorHeapManager()                                       = default;
public:
  DescriptorHeapManager(DescriptorHeapManager const&)            = delete;
  DescriptorHeapManager(DescriptorHeapManager&&)                 = delete;
  DescriptorHeapManager& operator=(DescriptorHeapManager const&) = delete;
  DescriptorHeapManager& operator=(DescriptorHeapManager&&)      = delete;

  static auto const instance() noexcept
  {
    static DescriptorHeapManager instance;
    return &instance;
  }

  void init() noexcept;
  auto pop_handle(DescriptorHeapType type, std::function<void()> recreate_descriptor_func) noexcept { return _heaps[type].pop_handle(recreate_descriptor_func); }
  void bind_heaps(ID3D12GraphicsCommandList1* cmd) noexcept;
  void reserve(DescriptorHeapType type, uint32_t capacity) noexcept { _heaps[type].reserve(capacity); }
  auto usable_handle_count(DescriptorHeapType type) const noexcept { return _heaps.at(type).usable_handle_count(); }

  auto first_gpu_handle(DescriptorHeapType type) const noexcept { return _heaps.at(type)._handles[0].second.gpu_handle(); }

private:
  std::unordered_map<DescriptorHeapType, DescriptorHeap> _heaps;
};

inline static auto& g_descriptor_heap_mgr{ *DescriptorHeapManager::instance() };

}}
