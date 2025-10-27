#include "buffer.hpp"
#include "error_handling.hpp"
#include "core.hpp"
#include "renderer.hpp"

#include <directx/d3dx12.h>

using namespace vn;
using namespace vn::renderer;

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

void Buffer::init(uint32_t size, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle) noexcept
{
  _size              = {};
  _capacity          = align(size, 8);
  _descriptor_handle = descriptor_handle;

  // create buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(_capacity * Frame_Count);
  err_if(Core::instance()->device()->CreateCommittedResource(
    &heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&_handle)),
    "failed to create vertex buffer");

  // get pointer of buffer
  auto range = CD3DX12_RANGE{};
  err_if(_handle->Map(0, &range, reinterpret_cast<void**>(&_data)), "failed to map pointer from buffer");

  if (descriptor_handle.ptr)
  {
    // create view
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format                     = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_RAW;
    srv_desc.Buffer.NumElements         = _capacity * Frame_Count / 4;
    Core::instance()->device()->CreateShaderResourceView(_handle.Get(), &srv_desc, descriptor_handle);
  }
}

auto Buffer::append(void const* data, uint32_t size) noexcept -> uint32_t
{
  // promise aligment
  size = align(size, 4);
  auto total_size = _size + size;
  if (total_size <= _capacity)
  {
    memcpy(_data + _size, data, size);
    _size = total_size;
  }
  else
  {
    // add old buffer for destroy
    Renderer::instance()->add_current_frame_render_finish_proc([handle = _handle] {});

    // temporary copy old data
    auto old_data = std::vector<std::byte>(_size);
    memcpy(old_data.data(), _data, _size);

    // create new bigger one
    init(calculate_capacity(_capacity, total_size), _descriptor_handle);

    // copy old data to new buffer
    append(old_data.data(), old_data.size());
    // now copy current data again
    append(data, size);
  }
  return size;
}

void FrameBuffer::init(D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle) noexcept
{
  _vertices_indices_buffer.init(1024);
  _shape_properties_buffer.init(1024, descriptor_handle);
}

void FrameBuffer::upload(ID3D12GraphicsCommandList1* cmd, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto vertices_offset = _vertices_indices_buffer.append_range(vertices);
  auto indices_offset  = _vertices_indices_buffer.append_range(indices);

  // get current buffer gpu address
  auto address = _vertices_indices_buffer.gpu_address() + _window_offset;

  // set vertex buffer view
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
  vertex_buffer_view.BufferLocation = address;
  vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
  vertex_buffer_view.SizeInBytes    = vertices_offset;
  cmd->IASetVertexBuffers(0, 1, &vertex_buffer_view);

  // add vertices offset
  address += vertices_offset;

  // set index buffer view
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  index_buffer_view.BufferLocation = address;
  index_buffer_view.SizeInBytes    = indices_offset;
  index_buffer_view.Format         = DXGI_FORMAT_R16_UINT;
  cmd->IASetIndexBuffer(&index_buffer_view);

  _window_offset = _vertices_indices_buffer.size();

  // shape properties
  for (auto const& shape_property : shape_properties)
    _shape_properties_buffer.append(shape_property.data(), shape_property.byte_size());
}

}}
