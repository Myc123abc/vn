#pragma once

#include <dxgi1_6.h>
#include <directx/d3dx12.h>

#include <glm/glm.hpp>

#include <optional>

namespace vn { namespace renderer {

enum class ImageType
{
  uav,
  rtv,
  srv,
  dsv,
};

enum class ImageFormat
{
  r8_unorm,
  bgra8_unorm,
  rgba8_unorm,
  d32,
};

enum class ImageState
{
  copy_src,
  copy_dst,
  present,
  unorder_access,
  common,
  render_target,
  pixel_shader_resource,
};

auto dxgi_format(ImageFormat format) noexcept -> DXGI_FORMAT;

class Image
{
public:
  Image()                        = default;
  ~Image()                       = default;
  Image(Image const&)            = default;
  Image(Image&&)                 = delete;
  Image& operator=(Image const&) = default;
  Image& operator=(Image&&)      = delete;

  auto init(ImageType type, ImageFormat format, uint32_t width , uint32_t height) noexcept -> Image&;
  auto init(IDXGISwapChain1* swapchain, uint32_t index)                           noexcept -> Image&;
  auto init(ImageType type, HANDLE handle, uint32_t width, uint32_t height)       noexcept -> Image&;

  void destroy() noexcept { _handle.Reset(); }

  void set_state(ID3D12GraphicsCommandList1* cmd, ImageState state) noexcept;

  void resize(uint32_t width, uint32_t height)            noexcept;
  void resize(IDXGISwapChain1* swapchain, uint32_t index) noexcept;

  void clear(ID3D12GraphicsCommandList1* cmd, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) const noexcept;

  auto handle()     const noexcept { return _handle.Get(); }
  auto width()      const noexcept { return _width;        }
  auto height()     const noexcept { return _height;       }
  auto extent()     const noexcept { return glm::vec<2, uint32_t>{ _width, _height }; }

  void create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::optional<ImageType> type = {}) noexcept;

private:
  auto init(ImageType type, DXGI_FORMAT format, uint32_t width , uint32_t height) noexcept -> Image&;

private:
  ImageType                              _type{};
  DXGI_FORMAT                            _format{};
  Microsoft::WRL::ComPtr<ID3D12Resource> _handle;
  D3D12_RESOURCE_STATES                  _state{};
  uint32_t                               _width{};
  uint32_t                               _height{};
  std::unordered_map<ImageType, D3D12_CPU_DESCRIPTOR_HANDLE> _cpu_handles;
};

void copy(
  ID3D12GraphicsCommandList1* cmd,
  Image&                      src,
  LONG                        left,
  LONG                        top,
  LONG                        right,
  LONG                        bottom,
  Image&                      dst,
  uint32_t                    x = 0,
  uint32_t                    y = 0) noexcept;

inline void copy(ID3D12GraphicsCommandList1* cmd, Image&  src, Image&  dst) noexcept
{
  copy(cmd, src, 0, 0, src.width(), src.height(), dst);
}

inline void copy(
  ID3D12GraphicsCommandList1* cmd,
  Image&                      image,
  ID3D12Resource*             upload_heap,
  uint32_t                    offset,
  D3D12_SUBRESOURCE_DATA&     data
)
{
  image.set_state(cmd, ImageState::copy_dst);
  UpdateSubresources(cmd, image.handle(), upload_heap, offset, 0, 1, &data);
}

}}
