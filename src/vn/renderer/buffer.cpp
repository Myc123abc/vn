#include "buffer.hpp"
#include "error_handling.hpp"
#include "core.hpp"
#include "renderer.hpp"
#include "../util.hpp"

#include <directx/d3dx12.h>

#include <algorithm>

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

void Buffer::init(uint32_t size, bool use_descriptor) noexcept
{
  _size     = {};
  _capacity = align(size, 8);

  auto create_descriptor = [this]
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format                  = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.Flags            = D3D12_BUFFER_SRV_FLAG_RAW;
    srv_desc.Buffer.NumElements      = _capacity / 4;
    Core::instance()->device()->CreateShaderResourceView(_handle.Get(), &srv_desc, _descriptor_handle.cpu_handle());
  };

  if (use_descriptor && !_handle)
    _descriptor_handle = DescriptorHeapManager::instance()->pop_handle(DescriptorHeapType::cbv_srv_uav, create_descriptor);
  
  // create buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(_capacity);
  err_if(Core::instance()->device()->CreateCommittedResource(
    &heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())),
    "failed to create vertex buffer");

  // get pointer of buffer
  auto range = CD3DX12_RANGE{};
  err_if(_handle->Map(0, &range, reinterpret_cast<void**>(&_data)), "failed to map pointer from buffer");

  if (use_descriptor)
    create_descriptor();
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
    Renderer::instance()->add_current_frame_render_finish_proc([_ = _handle] {});

    // temporary copy old data
    auto old_data = std::vector<std::byte>(_size);
    memcpy(old_data.data(), _data, _size);

    // create new bigger one
    init(calculate_capacity(_capacity, total_size), _descriptor_handle.is_valid());

    // copy old data to new buffer
    append(old_data.data(), old_data.size());
    // now copy current data again
    append(data, size);
  }
  return size;
}

void FrameBuffer::init() noexcept
{
  _vertices_indices_buffer.init(Vertices_Indices_Buffer_Size, false);
  _shape_properties_buffer.init(Shape_Properties_Buffer_Size, true);
}

void FrameBuffer::upload(ID3D12GraphicsCommandList1* cmd, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept
{
  auto vertices_offset = _vertices_indices_buffer.append_range(vertices);
  auto indices_offset  = _vertices_indices_buffer.append_range(indices);

  // get current buffer gpu address
  auto address = _vertices_indices_buffer.gpu_address();

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

  // shape properties
  for (auto const& shape_property : shape_properties)
    _shape_properties_buffer.append(shape_property.data(), shape_property.byte_size());
}

void UploadBuffer::add_images(std::vector<ImageHandle> const& image_handles, std::vector<BitmapView> const& bitmaps) noexcept
{
  assert(image_handles.size() == bitmaps.size());

  _infos.reserve(_infos.size() + image_handles.size());
  for (auto i = 0; i < image_handles.size(); ++i)
  {
    auto info = Info{};
    info.handle          = image_handles[i];
    info.data.pData      = bitmaps[i].data;
    info.data.RowPitch   = bitmaps[i].row_pitch;
    info.data.SlicePitch = bitmaps[i].row_pitch * bitmaps[i].height;
    _infos.emplace_back(std::move(info));
  }
}

void UploadBuffer::upload(ID3D12GraphicsCommandList1* cmd) noexcept
{
  auto mem_pool = MemoryPool::instance();

  // calculate required intermediate sizes
  auto intermediate_sizes = std::vector<uint32_t>{};
  intermediate_sizes.reserve(_infos.size());
  for (auto const& info : _infos)
    intermediate_sizes.emplace_back(
      align(GetRequiredIntermediateSize(mem_pool->get(info.handle)->handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));

  // initialize buffer
  _buffer.init(std::ranges::fold_left(intermediate_sizes, 0, std::plus<>{}), false);

  // copy bitmap data to image by upload buffer
  auto offset = uint32_t{};
  for (auto i = 0; i < _infos.size(); ++i)
  {
    copy(cmd, *mem_pool->get(_infos[i].handle), _buffer.handle(), offset, _infos[i].data);
    offset += intermediate_sizes[i];
  }

  _infos.clear();
}

}}
