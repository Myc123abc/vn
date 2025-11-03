#include "descriptor_heap.hpp"
#include "error_handling.hpp"
#include "core.hpp"

using namespace vn;
using namespace vn::renderer;

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

auto dx12_descriptor_heap_flags(DescriptorHeapType type) noexcept
{
  using enum DescriptorHeapType;
  auto static const map = std::unordered_map<DescriptorHeapType, D3D12_DESCRIPTOR_HEAP_FLAGS>
  {
    { cbv_srv_uav, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },
    { rtv,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE           },
    { dsv,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE           },
  };
  err_if(!map.contains(type), "unsupport descriptor heap flags now");
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

auto DescriptorHeap::init(DescriptorHeapType type, uint32_t capacity, bool as_copy_src) noexcept -> DescriptorHeap&
{
  _type        = type;
  _size        = {};
  _capacity    = capacity;
  _as_copy_src = as_copy_src;

  auto heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{};
  heap_desc.NumDescriptors = capacity;
  heap_desc.Type           = dx12_descriptor_heap_type(type);
  heap_desc.Flags          = _as_copy_src ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : dx12_descriptor_heap_flags(type);
  err_if(Core::instance()->device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_heap)), "failed to create descriptor heap");

  return *this;
}

void DescriptorHeap::add_tag(std::string_view tag, uint32_t placeholder_num) noexcept
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

auto DescriptorHeap::pop_handle(std::string_view tag) noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE
{
  err_if(_size + 1 > _capacity, "TODO: dynamic expand heap");
  if (!tag.empty()) add_tag(tag);
  auto cpu_handle = _heap->GetCPUDescriptorHandleForHeapStart();
  cpu_handle.ptr += dx12_descriptor_size(_type) * _size++;
  return cpu_handle;
}

auto DescriptorHeap::cpu_handle(std::string_view tag, uint32_t offset) const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE
{
  auto handle = _heap->GetCPUDescriptorHandleForHeapStart();
  if (tag.empty()) return handle;
  err_if(!_tags.contains(tag.data()), "unexist tag of descriptor : {}", tag);
  handle.ptr += dx12_descriptor_size(_type) * (_tags.at(tag.data()) + offset);
  return handle;
}

auto DescriptorHeap::gpu_handle(std::string_view tag, uint32_t offset) const noexcept -> D3D12_GPU_DESCRIPTOR_HANDLE
{
  auto handle = _heap->GetGPUDescriptorHandleForHeapStart();
  if (tag.empty()) return handle;
  err_if(!_tags.contains(tag.data()), "unexist tag of descriptor : {}", tag);
  handle.ptr += dx12_descriptor_size(_type) * (_tags.at(tag.data()) + offset);
  return handle;
}

void copy(DescriptorHeap const& src, std::string_view src_tag, uint32_t src_offset,
          DescriptorHeap const& dst, std::string_view dst_tag, uint32_t dst_offset) noexcept
{
  err_if(src._type != dst._type, "different descriptor heaps cannot copy each other");
  err_if(!src._as_copy_src, "copy source descriptor heap must initialize as copy source");
  Core::instance()->device()->CopyDescriptorsSimple(1, dst.cpu_handle(dst_tag, dst_offset), src.cpu_handle(src_tag, src_offset), dx12_descriptor_heap_type(src._type));
}

}}
