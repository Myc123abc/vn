#include "descriptor_heap_manager.hpp"
#include "error_handling.hpp"
#include "core.hpp"
#include "renderer.hpp"

#include <algorithm>
#include <ranges>
#include <unordered_map>

using namespace vn;
using namespace vn::renderer;
using namespace Microsoft::WRL;

namespace {

auto dx12_descriptor_heap_type(DescriptorHeapType type) noexcept
{
  using enum DescriptorHeapType;
  auto static const map = std::unordered_map<DescriptorHeapType, D3D12_DESCRIPTOR_HEAP_TYPE>
  {
    { cbv_srv_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV },
    { rtv,         D3D12_DESCRIPTOR_HEAP_TYPE_RTV         },
    { dsv,         D3D12_DESCRIPTOR_HEAP_TYPE_DSV         },
  };
  err_if(!map.contains(type), "unsupport descriptor heap type now");
  return map.at(type);
}

auto dx12_descriptor_size(DescriptorHeapType type) noexcept
{
  using enum DescriptorHeapType;
  auto static const map = std::unordered_map<DescriptorHeapType, uint32_t>
  {
    { cbv_srv_uav, CBV_SRV_UAV_Size },
    { rtv,         RTV_Size         },
    { dsv,         DSV_Size         },
  };
  err_if(!map.contains(type), "unsupport descriptor size now");
  return map.at(type);
}

}

namespace vn { namespace renderer {

void DescriptorHandle::release() noexcept
{
  if (is_valid())
  {
    DescriptorHeapManager::instance()->_heaps[_type]._handles[_index].first = false;
    _index = -1;
  }
}

auto DescriptorHandle::cpu_handle() const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE
{
  auto handle = DescriptorHeapManager::instance()->_heaps[_type]._heap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += dx12_descriptor_size(_type) * _index;
  return handle;
}

auto DescriptorHandle::gpu_handle() const noexcept -> D3D12_GPU_DESCRIPTOR_HANDLE
{
  auto handle = DescriptorHeapManager::instance()->_heaps[_type]._heap->GetGPUDescriptorHandleForHeapStart();
  handle.ptr += dx12_descriptor_size(_type) * _index;
  return handle;
}

void DescriptorHeapManager::DescriptorHeap::init(DescriptorHeapType type, uint32_t capacity) noexcept
{
  _type = type;

  // create descriptor heap
  auto heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{};
  heap_desc.NumDescriptors = capacity;
  heap_desc.Type           = dx12_descriptor_heap_type(type);
  heap_desc.Flags          = type == DescriptorHeapType::cbv_srv_uav ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  err_if(Core::instance()->device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_heap)), "failed to create descriptor heap");

  // initialize handles
  _handles.resize(capacity);
}

auto DescriptorHeapManager::DescriptorHeap::pop_handle(std::function<void()> recreate_descriptor_func) noexcept -> DescriptorHandle
{
  // find a not used handle
  auto it = std::ranges::find_if_not(_handles, [](auto const& handle) { return handle.first; });

  // if not found, the heap is full, expand it
  if (it == _handles.end())
  {
    reserve(_handles.size() * 2);
    return pop_handle(recreate_descriptor_func);
  }

  // find a useful handle
  it->first                            = true;
  it->second._type                     = _type;
  it->second._index                    = it - _handles.begin();
  it->second._recreate_descriptor_func = recreate_descriptor_func;
  return it->second;
}

void DescriptorHeapManager::DescriptorHeap::reserve(uint32_t capacity) noexcept
{
  if (capacity > _handles.size())
  {
    // destroy old heap
    Renderer::instance()->add_current_frame_render_finish_proc([_ = _heap] {});

    // create new bigger one
    auto size = _handles.size();
    init(_type, capacity);

    // recreate descriptors
    std::ranges::for_each(_handles | std::views::take(size) | std::views::elements<1>,
      [](auto const& handle) { if (handle._recreate_descriptor_func) handle._recreate_descriptor_func(); });
  }
}

auto DescriptorHeapManager::DescriptorHeap::usable_handle_count() const noexcept -> uint32_t
{
  return std::ranges::count_if(_handles | std::views::elements<0>, [](auto usable) { return usable; });
}

void DescriptorHeapManager::init() noexcept
{
  using enum DescriptorHeapType;
  _heaps[cbv_srv_uav].init(cbv_srv_uav, CBV_SRV_UAV_Heap_Size);
  _heaps[rtv].init(rtv, RTV_Heap_Size);
  if (Renderer::instance()->enable_depth_test)
    _heaps[dsv].init(dsv, DSV_Size);
}

void DescriptorHeapManager::bind_heaps(ID3D12GraphicsCommandList1* cmd) noexcept
{
  auto descriptor_heaps = std::array<ID3D12DescriptorHeap*, 1>{ _heaps[DescriptorHeapType::cbv_srv_uav]._heap.Get() };
  cmd->SetDescriptorHeaps(descriptor_heaps.size(), descriptor_heaps.data());
}

}}
