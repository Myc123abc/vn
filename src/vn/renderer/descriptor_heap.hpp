#pragma once

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

class DescriptorHeap
{
  friend void copy(DescriptorHeap const& src, std::string_view src_tag, uint32_t src_offset,
                   DescriptorHeap const& dst, std::string_view dst_tag, uint32_t dst_offset) noexcept;
public:
  DescriptorHeap()                                 = default;
  ~DescriptorHeap()                                = default;
  DescriptorHeap(DescriptorHeap const&)            = default;
  DescriptorHeap(DescriptorHeap&&)                 = delete;
  DescriptorHeap& operator=(DescriptorHeap const&) = default;
  DescriptorHeap& operator=(DescriptorHeap&&)      = delete;

  auto init(DescriptorHeapType type, uint32_t capacity = 1, bool as_copy_src = false) noexcept -> DescriptorHeap&;

  void add_tag(std::string_view tag, uint32_t placeholder_num = {}) noexcept;

  auto pop_handle(std::string_view tag = {}) noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE;

  auto handle() const noexcept { return _heap.Get(); }

  auto cpu_handle(std::string_view tag = {}, uint32_t offset = {}) const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE;

  auto gpu_handle(std::string_view tag = {}, uint32_t offset = {}) const noexcept -> D3D12_GPU_DESCRIPTOR_HANDLE;

  auto capacity() const noexcept { return _capacity; }

private:
  DescriptorHeapType                           _type{};
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heap;
  uint32_t                                     _capacity{};
  uint32_t                                     _size{};
  std::unordered_map<std::string, uint32_t>    _tags;
  bool                                         _as_copy_src{};
};

void copy(DescriptorHeap const& src, std::string_view src_tag, uint32_t src_offset,
          DescriptorHeap const& dst, std::string_view dst_tag, uint32_t dst_offset) noexcept;

inline void copy(DescriptorHeap const& src, std::string_view src_tag,
          DescriptorHeap const& dst, std::string_view dst_tag) noexcept
{
  copy(src, src_tag, 0, dst, dst_tag, 0);
}

}}
