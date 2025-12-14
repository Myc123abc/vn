#pragma once

#include "shader_type.hpp"
#include "image.hpp"
#include "memory_pool.hpp"

#include <d3d12.h>
#include <wrl/client.h>

#include <glm/glm.hpp>

#include <span>

namespace vn { namespace renderer {

class Buffer
{
public:
  void init(uint32_t size, bool use_descriptor) noexcept;
  void destroy() noexcept { _descriptor_handle.release(); }

  void clear() noexcept { _size = {}; }

  auto append(void const* data, uint32_t size) noexcept -> uint32_t;

  template <std::ranges::range T>
  requires std::ranges::sized_range<T> && std::ranges::contiguous_range<T>
  auto append_range(T&& values) noexcept -> uint32_t
  {
    return append(std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<T>));
  }

  auto gpu_address() const noexcept { return _handle->GetGPUVirtualAddress(); }
  auto size()        const noexcept { return _size;                           }
  auto gpu_handle()  const noexcept { return _descriptor_handle.gpu_handle(); }
  auto handle()      const noexcept { return _handle.Get();                   }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> _handle;
  DescriptorHandle                       _descriptor_handle;
  uint8_t*                               _data{};
  uint32_t                               _capacity{};
  uint32_t                               _size{};
};

class FrameBuffer
{
public:
  void init() noexcept;
  void destroy() noexcept
  {
    _vertices_indices_buffer.destroy();
    _shape_properties_buffer.destroy();
  }

  auto clear() noexcept -> FrameBuffer&
  {
    _vertices_indices_buffer.clear();
    _shape_properties_buffer.clear();
    return *this;
  }

  void upload(ID3D12GraphicsCommandList1* cmd, std::span<Vertex const> vertices, std::span<uint16_t const> indices, std::span<ShapeProperty const> shape_properties) noexcept;

  auto gpu_handle() const noexcept { return _shape_properties_buffer.gpu_handle(); }

private:
  Buffer _vertices_indices_buffer;
  Buffer _shape_properties_buffer;
};

class UploadBuffer
{
public:
  void add_images(std::vector<ImageHandle> const& image_handles, std::vector<BitmapView> const& bitmaps) noexcept;
  void upload(ID3D12GraphicsCommandList1* cmd) noexcept;

private:
  struct Info
  {
    D3D12_SUBRESOURCE_DATA data{};
    ImageHandle            handle;
  };

  Buffer            _buffer;
  std::vector<Info> _infos;
};

}}
