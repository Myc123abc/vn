#pragma once

#include "core.hpp"
#include "../util.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <directx/d3dx12.h>

#include <glm/glm.hpp>

#include <ranges>

namespace vn { namespace renderer {

struct alignas(8) Vertex
{
  glm::vec2 pos{};
  glm::vec2 uv{};
	uint32_t  color{};
  uint32_t  flags{};
};

struct Constants
{
  glm::vec<2, uint32_t> window_extent{};
  glm::vec<2, int32_t>  window_pos{};
  int                   cursor_index{};
  uint32_t              buffer_offset{};
};

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

class FrameBuffer
{
public:
  FrameBuffer()                              = default;
  ~FrameBuffer()                             = default;
  FrameBuffer(FrameBuffer const&)            = default;
  FrameBuffer(FrameBuffer&&)                 = delete;
  FrameBuffer& operator=(FrameBuffer const&) = default;
  FrameBuffer& operator=(FrameBuffer&&)      = delete;

  void init(uint32_t per_frame_capacity, D3D12_CPU_DESCRIPTOR_HANDLE handle) noexcept;

  void clear() noexcept { _size = {}; _window_offset = {}; }

  auto upload(ID3D12GraphicsCommandList* command_list, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept -> uint32_t;

private:
  auto append(void const* data, uint32_t size) noexcept -> uint32_t;

  template <std::ranges::range T>
  requires std::ranges::sized_range<T> && std::ranges::contiguous_range<T>
  auto append_range(T&& values) noexcept -> uint32_t
  {
    return append(std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<T>));
  }

  inline auto get_current_frame_buffer_pointer() const noexcept -> std::byte*;
  inline auto get_current_frame_buffer_address() const noexcept -> D3D12_GPU_VIRTUAL_ADDRESS;

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> _buffer;
  D3D12_CPU_DESCRIPTOR_HANDLE            _descriptor_handle{};
  std::byte*                             _pointer{};
  uint32_t                               _per_frame_capacity{};
  uint32_t                               _size{};
  uint32_t                               _window_offset{};
};

}}