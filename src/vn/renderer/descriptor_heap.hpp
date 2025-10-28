#pragma once

#include "core.hpp"
#include "error_handling.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <directx/d3dx12.h>

#include <unordered_map>

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

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_heap_type()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  else if constexpr (T == DescriptorHeapType::rtv)
    return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  else if constexpr (T == DescriptorHeapType::dsv)
    return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  else
    static_assert(false, "unsupport descriptor heap type now");
}

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_heap_flags()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  else if constexpr (T == DescriptorHeapType::rtv)
    return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  else if constexpr (T == DescriptorHeapType::dsv)
    return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  else
    static_assert(false, "unsupport descriptor heap flags now");
}

template <DescriptorHeapType T>
auto constexpr dx12_descriptor_size()
{
  if constexpr (T == DescriptorHeapType::cbv_srv_uav)
    return CBV_SRV_UAV_Size;
  else if constexpr (T == DescriptorHeapType::rtv)
    return RTV_Size;
  else if constexpr (T == DescriptorHeapType::dsv)
    return DSV_Size;
  else
    static_assert(false, "unsupport descriptor size now");
}

template <DescriptorHeapType Type, uint32_t Count>
class DescriptorHeap
{
  template <DescriptorHeapType SrcType, uint32_t SrcCount,
            DescriptorHeapType DstType, uint32_t DstCount>
  friend void copy(DescriptorHeap<SrcType, SrcCount> const& src, std::string_view src_tag, uint32_t src_offset,
                   DescriptorHeap<DstType, DstCount> const& dst, std::string_view dst_tag, uint32_t dst_offset) noexcept;
public:
  DescriptorHeap()                                 = default;
  ~DescriptorHeap()                                = default;
  DescriptorHeap(DescriptorHeap const&)            = default;
  DescriptorHeap(DescriptorHeap&&)                 = delete;
  DescriptorHeap& operator=(DescriptorHeap const&) = default;
  DescriptorHeap& operator=(DescriptorHeap&&)      = delete;

  void init(bool as_copy_src = false) noexcept
  {
    _size        = 0;
    _capacity    = Count;
    _as_copy_src = as_copy_src;

    auto heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{};
    heap_desc.NumDescriptors = Count;
    heap_desc.Type           = dx12_descriptor_heap_type<Type>();
    heap_desc.Flags          = _as_copy_src ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : dx12_descriptor_heap_flags<Type>();
    err_if(Core::instance()->device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_heap)), "failed to create descriptor heap");
    _current_handle = _heap->GetCPUDescriptorHandleForHeapStart();
  }

  void add_tag(std::string_view tag, uint32_t placeholder_num = {}) noexcept
  {
    err_if(tag.empty(), "cannot mark empty tag");
    err_if(_tags.contains(tag.data()), "duplicate tag of descriptor : {}", tag);
    _tags[tag.data()] = _size;

    if (placeholder_num)
    {
      err_if(_size + placeholder_num > _capacity, "TODO: dynamic expand heap");
      _size += placeholder_num;
    }
  }

  auto pop_handle(std::string_view tag = {}) noexcept
  {
    err_if(_size + 1 > _capacity, "TODO: dynamic expand heap");
    if (!tag.empty()) add_tag(tag);
    ++_size;
    auto handle = _current_handle;
    _current_handle.Offset(dx12_descriptor_size<Type>());
    return handle;
  }

  auto handle() const noexcept { return _heap.Get(); }

  auto cpu_handle(std::string_view tag = {}, uint32_t offset = {}) const noexcept
  {
    auto handle = _heap->GetCPUDescriptorHandleForHeapStart();
    if (tag.empty()) return handle;
    err_if(!_tags.contains(tag.data()), "unexist tag of descriptor : {}", tag);
    handle.ptr += dx12_descriptor_size<Type>() * (_tags.at(tag.data()) + offset);
    return handle;
  }

  auto gpu_handle(std::string_view tag = {}, uint32_t offset = {}) const noexcept
  {
    auto handle = _heap->GetGPUDescriptorHandleForHeapStart();
    if (tag.empty()) return handle;
    err_if(!_tags.contains(tag.data()), "unexist tag of descriptor : {}", tag);
    handle.ptr += dx12_descriptor_size<Type>() * (_tags.at(tag.data()) + offset);
    return handle;
  }

  auto capacity() const noexcept { return _capacity; }

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heap;
  uint32_t                                     _capacity{};
  uint32_t                                     _size{};
  CD3DX12_CPU_DESCRIPTOR_HANDLE                _current_handle{};
  std::unordered_map<std::string, uint32_t>    _tags;
  bool                                         _as_copy_src{};
};

template <DescriptorHeapType SrcType, uint32_t SrcCount,
          DescriptorHeapType DstType, uint32_t DstCount>
void copy(DescriptorHeap<SrcType, SrcCount> const& src, std::string_view src_tag, uint32_t src_offset,
          DescriptorHeap<DstType, DstCount> const& dst, std::string_view dst_tag, uint32_t dst_offset) noexcept
{
  static_assert(SrcType == DstType, "different descriptor heaps cannot copy each other");
  err_if(!src._as_copy_src, "copy source descriptor heap must initialize as copy source");
  Core::instance()->device()->CopyDescriptorsSimple(1, dst.cpu_handle(dst_tag, dst_offset), src.cpu_handle(src_tag, src_offset), dx12_descriptor_heap_type<SrcType>());
}

template <DescriptorHeapType SrcType, uint32_t SrcCount,
          DescriptorHeapType DstType, uint32_t DstCount>
void copy(DescriptorHeap<SrcType, SrcCount> const& src, std::string_view src_tag,
          DescriptorHeap<DstType, DstCount> const& dst, std::string_view dst_tag) noexcept
{
  copy(src, src_tag, 0, dst, dst_tag, 0);
}

}}
