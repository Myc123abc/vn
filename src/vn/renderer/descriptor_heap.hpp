#pragma once

#include "core.hpp"
#include "util.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <directx/d3dx12.h>

namespace vn { namespace renderer {

enum class DescriptorHeapType
{
  cbv_srv_uav,
};

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_heap_type()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  else
    static_assert(false, "unsupport descriptor heap type now");
}

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_heap_flags()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  else
    static_assert(false, "unsupport descriptor heap flags now");
}

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_size()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return CBV_SRV_UAV_Size;
  else
    static_assert(false, "unsupport descriptor size now");
}

template <DescriptorHeapType Type, uint32_t Count>
class DescriptorHeap
{
public:
  DescriptorHeap()                                 = default;
  ~DescriptorHeap()                                = default;
  DescriptorHeap(DescriptorHeap const&)            = default;
  DescriptorHeap(DescriptorHeap&&)                 = delete;
  DescriptorHeap& operator=(DescriptorHeap const&) = default;
  DescriptorHeap& operator=(DescriptorHeap&&)      = delete;

  void init() noexcept
  {
    _size     = 0;
    _capacity = Count;

    auto heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{};
    heap_desc.NumDescriptors = Count;
    heap_desc.Type           = dx12_descriptor_heap_type<Type>();
    heap_desc.Flags          = dx12_descriptor_heap_flags<Type>();
    err_if(Core::instance()->device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_heap)), "failed to create descriptor heap");
    _current_handle = _heap->GetCPUDescriptorHandleForHeapStart();
  }

  auto pop_handle() noexcept
  {
    err_if(_size + 1 > _capacity, "TODO: dynamic expand heap");
    ++_size;
    auto handle = _current_handle;
    _current_handle.Offset(dx12_descriptor_size<Type>());
    return handle;
  }

  auto handle() const noexcept { return _heap.Get(); }

  auto gpu_handle() const noexcept { return _heap->GetGPUDescriptorHandleForHeapStart(); }

  auto capacity() const noexcept { return _capacity; }

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heap;
  uint32_t                                     _capacity{};
  uint32_t                                     _size{};
  CD3DX12_CPU_DESCRIPTOR_HANDLE                _current_handle{};
};

}}
