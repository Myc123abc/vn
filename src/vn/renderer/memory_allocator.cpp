#include "memory_allocator.hpp"
#include "renderer.hpp"
#include "../util.hpp"
#include "config.hpp"
#include "message_queue.hpp"

#include <directx/d3dx12.h>

namespace {

inline auto align(uint32_t value, uint32_t size)
{
  return (value + size - 1) & ~(size - 1);
}

auto calculate_capacity(uint32_t old_capacity, uint32_t need_capacity)
{
  auto factor = (old_capacity < 256 * 1024)      ? 2.0 :
                (old_capacity < 8 * 1024 * 1024) ? 1.5 : 1.25;

  auto capacity = static_cast<uint32_t>(old_capacity * factor);
  if (old_capacity < need_capacity) capacity = need_capacity;

  // Round up to 256 bytes
  capacity = align(capacity, 256);

  // Clamp to max budget (optional)
  constexpr size_t Max = 128ull * 1024 * 1024;
  if (capacity > Max) capacity = align(need_capacity, 256);

  return capacity;
}

}

namespace vn { namespace renderer {

void FrameBuffer::init(uint32_t per_frame_capacity) noexcept
{
  _size               = 0;
  _per_frame_capacity = align(per_frame_capacity, 256);

  // create buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(_per_frame_capacity * Frame_Count);
  err_if(Renderer::instance()->_device->CreateCommittedResource(
    &heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&_buffer)),
    "failed to create vertex buffer");
  
  // get pointer of buffer
  auto range = CD3DX12_RANGE{};
  err_if(_buffer->Map(0, &range, reinterpret_cast<void**>(&_pointer)),
          "failed to map pointer from buffer");
}

auto FrameBuffer::get_current_frame_buffer_pointer() const noexcept -> std::byte*
{
  return _pointer + Renderer::instance()->_frame_index * _per_frame_capacity;
}

auto FrameBuffer::get_current_frame_buffer_address() const noexcept -> D3D12_GPU_VIRTUAL_ADDRESS
{
  return _buffer->GetGPUVirtualAddress() + Renderer::instance()->_frame_index * _per_frame_capacity;
}

auto FrameBuffer::append(void const* data, uint32_t size) noexcept -> FrameBuffer&
{
  auto total_size = _size + size;
  if (total_size <= _per_frame_capacity)
  {
    memcpy(get_current_frame_buffer_pointer() + _size, data, size);
    _size = total_size;
  }
  else
  {
    // add old buffer for destroy
    auto renderer = Renderer::instance();
    auto func = [buffer = _buffer, last_fence_value = renderer->_fence_values[renderer->_frame_index]]
    {
      auto fence_value = Renderer::instance()->_fence->GetCompletedValue();
      err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
      return fence_value >= last_fence_value;
    };
    MessageQueue::instance()->push(FrameBufferDestroyInfo{ func });

    // temporary copy old data
    std::vector<std::byte> old_data(_size);
    memcpy(old_data.data(), get_current_frame_buffer_pointer(), _size);

    // create new bigger one
    init(calculate_capacity(_per_frame_capacity, total_size));

    // copy old data to new buffer
    append(old_data.data(), old_data.size());
    // now copy current data again
    append(data, size);
  }
  return *this;
}

void FrameBuffer::upload(ID3D12GraphicsCommandList* command_list, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept
{
  append_range(vertices);
  append_range(indices);

  // get current buffer gpu address
  auto address = get_current_frame_buffer_address() + _window_offset;

  // set vertex buffer view
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
  vertex_buffer_view.BufferLocation = address;
  vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
  vertex_buffer_view.SizeInBytes    = vertices.size_bytes();
  command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

  // add vertices offset
  address += vertices.size_bytes();

  // set index buffer view
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  index_buffer_view.BufferLocation = address;
  index_buffer_view.SizeInBytes    = indices.size_bytes();
  index_buffer_view.Format         = DXGI_FORMAT_R16_UINT;
  command_list->IASetIndexBuffer(&index_buffer_view);

  // TODO: add indices offset for other data like shape properties

  // set window offset
  _window_offset = _size;
}

}}