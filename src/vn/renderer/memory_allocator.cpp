#include "memory_allocator.hpp"
#include "core.hpp"
#include "../util.hpp"
#include "config.hpp"
#include "renderer.hpp"

#include <directx/d3dx12.h>

using namespace vn;

namespace {

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

void FrameBuffer::init(uint32_t per_frame_capacity, D3D12_CPU_DESCRIPTOR_HANDLE handle) noexcept
{
  _size               = {};
  _per_frame_capacity = align(per_frame_capacity, 8);
  _descriptor_handle  = handle;

  // create buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(_per_frame_capacity * Frame_Count);
  err_if(Core::instance()->device()->CreateCommittedResource(
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

  // create view
  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format                     = DXGI_FORMAT_R32_TYPELESS;
  srv_desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
  srv_desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_RAW;
  srv_desc.Buffer.NumElements         = _per_frame_capacity * Frame_Count / 4;
  Core::instance()->device()->CreateShaderResourceView(_buffer.Get(), &srv_desc, handle);
}

auto FrameBuffer::get_current_frame_buffer_pointer() const noexcept -> std::byte*
{
  return _pointer + Core::instance()->frame_index() * _per_frame_capacity;
}

auto FrameBuffer::get_current_frame_buffer_address() const noexcept -> D3D12_GPU_VIRTUAL_ADDRESS
{
  return _buffer->GetGPUVirtualAddress() + Core::instance()->frame_index() * _per_frame_capacity;
}

auto FrameBuffer::append(void const* data, uint32_t size) noexcept -> uint32_t
{
  // promise aligment
  size = align(size, 4);
  auto total_size = _size + size;
  if (total_size <= _per_frame_capacity)
  {
    memcpy(get_current_frame_buffer_pointer() + _size, data, size);
    _size = total_size;
  }
  else
  {
    // add old buffer for destroy
    Renderer::instance()->add_current_frame_render_finish_proc([buffer = _buffer] {});

    // temporary copy old data
    std::vector<std::byte> old_data(_size);
    memcpy(old_data.data(), get_current_frame_buffer_pointer(), _size);

    // create new bigger one
    // FIXME: every frame use them own descriptor handle of frame buffer, promise when buffer change, the last frame use old buffer is valid
    //        so array<Buffer, Frame_Count>?
    init(calculate_capacity(_per_frame_capacity, total_size), _descriptor_handle);
    assert(true);

    // copy old data to new buffer
    append(old_data.data(), old_data.size());
    // now copy current data again
    append(data, size);
  }
  return size;
}

auto FrameBuffer::upload(ID3D12GraphicsCommandList* command_list, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept -> uint32_t
{
  auto vertices_offset = append_range(vertices);
  auto indices_offset  = append_range(indices);

  // get current buffer gpu address
  auto address = get_current_frame_buffer_address() + _window_offset;

  // set vertex buffer view
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
  vertex_buffer_view.BufferLocation = address;
  vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
  vertex_buffer_view.SizeInBytes    = vertices_offset;
  command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

  // add vertices offset
  address += vertices_offset;

  // set index buffer view
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  index_buffer_view.BufferLocation = address;
  index_buffer_view.SizeInBytes    = indices_offset;
  index_buffer_view.Format         = DXGI_FORMAT_R16_UINT;
  command_list->IASetIndexBuffer(&index_buffer_view);

  // TODO: add indices offset for other data like shape properties
  auto v = 42;
  append(&v, sizeof(v));

  auto other_data_offset = _per_frame_capacity * Core::instance()->frame_index() + _window_offset +  vertices_offset + indices_offset;

  _window_offset = _size;

  return other_data_offset;
}

}}