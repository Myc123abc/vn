#pragma once

#include "util.hpp"
#include "core.hpp"

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <directx/d3dx12.h>

#include <glm/glm.hpp>

namespace vn { namespace renderer {

enum class ImageType
{
  uav,
  rtv,
  srv,
};

enum class ImageFormat
{
  bgra8_unorm,
  rgba8_unorm,
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

template <ImageFormat T>
auto constexpr dx12_image_format() noexcept
{
  if constexpr (T == ImageFormat::bgra8_unorm)
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  else if constexpr (T == ImageFormat::rgba8_unorm)
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  else
    static_assert(false, "unsupport image format now");
}

template <ImageType T>
auto constexpr dx12_image_flags() noexcept
{
  if constexpr (T == ImageType::uav)
    return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  else if constexpr (T == ImageType::rtv)
    static_assert(false, "rtv not need the image initialize flags");
  else if constexpr (T == ImageType::srv)
    return D3D12_RESOURCE_FLAG_NONE;
  else
    static_assert(false, "unsupport image type now");
}

template <ImageType T>
auto constexpr dx12_init_image_state() noexcept
{
  if constexpr (T == ImageType::uav)
    return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  else if constexpr (T == ImageType::rtv)
    return D3D12_RESOURCE_STATE_PRESENT;
  else if constexpr (T == ImageType::srv)
    return D3D12_RESOURCE_STATE_COMMON;
  else
    static_assert(false, "unsupport image state now");
}

template <ImageState T>
auto constexpr dx12_image_state() noexcept
{
  if constexpr (T == ImageState::copy_dst)
    return D3D12_RESOURCE_STATE_COPY_DEST;
  else if constexpr (T == ImageState::copy_src)
    return D3D12_RESOURCE_STATE_COPY_SOURCE;
  else if constexpr (T == ImageState::present)
    return D3D12_RESOURCE_STATE_PRESENT;
  else if constexpr (T == ImageState::unorder_access)
    return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  else if constexpr (T == ImageState::common)
    return D3D12_RESOURCE_STATE_COMMON;
  else if constexpr (T == ImageState::render_target)
    return D3D12_RESOURCE_STATE_RENDER_TARGET;
  else if constexpr (T == ImageState::pixel_shader_resource)
    return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  else
    static_assert(false, "unsupport image state now");
}

template <ImageType Type, ImageFormat Format>
class Image
{
public:
  Image()                        = default;
  ~Image()                       = default;
  Image(Image const&)            = default;
  Image(Image&&)                 = delete;
  Image& operator=(Image const&) = default;
  Image& operator=(Image&&)      = delete;

  void init(uint32_t width , uint32_t height) noexcept
  {
    _state  = dx12_init_image_state<Type>();
    _width  = width;
    _height = height;

    // create image
    D3D12_RESOURCE_DESC texture_desc{};
    texture_desc.MipLevels        = 1;
    texture_desc.Format           = dx12_image_format<Format>();
    texture_desc.Width            = width;
    texture_desc.Height           = height;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Flags            = dx12_image_flags<Type>();
    auto heap_properties          = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
    err_if(Core::instance()->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, _state, nullptr, IID_PPV_ARGS(&_handle)),
            "failed to create image");
  }

  auto init(IDXGISwapChain1* swapchain, uint32_t index) noexcept -> Image&
  {
    _state = dx12_init_image_state<Type>();
    err_if(Type != ImageType::rtv, "must be rtv");
    err_if(swapchain->GetBuffer(index, IID_PPV_ARGS(&_handle)),
           "failed to get descriptor");
    DXGI_SWAP_CHAIN_DESC1 desc{};
    err_if(swapchain->GetDesc1(&desc), "failed to get swapchain description");
    _width  = desc.Width;
    _height = desc.Height;
    return *this;
  }

  void init(HANDLE handle, uint32_t width, uint32_t height) noexcept
  {
    _state  = dx12_init_image_state<Type>();
    _width  = width;
    _height = height;
    err_if(Core::instance()->device()->OpenSharedHandle(handle, IID_PPV_ARGS(&_handle)), "failed to share d3d11 texture");
  }

  void destroy() noexcept { _handle.Reset(); }

  void create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle) const noexcept
  {
    if constexpr (Type == ImageType::uav)
    {
      auto uav_desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{};
      uav_desc.Format        = dx12_image_format<Format>();
      uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      Core::instance()->device()->CreateUnorderedAccessView(_handle.Get(), nullptr, &uav_desc, handle);
    }
    else if constexpr (Type == ImageType::rtv)
    {
      Core::instance()->device()->CreateRenderTargetView(_handle.Get(), nullptr, handle);
    }
    else if constexpr (Type == ImageType::srv)
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Format                  = dx12_image_format<Format>();
      srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Texture2D.MipLevels     = 1;
      Core::instance()->device()->CreateShaderResourceView(_handle.Get(), &srv_desc, handle);
    }
    else
      static_assert(false, "unsupport image type now");
  }

  template <ImageState state>
  void set_state(ID3D12GraphicsCommandList* cmd) noexcept
  {
    if (_state == dx12_image_state<state>()) return;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_handle.Get(), _state, dx12_image_state<state>());
    cmd->ResourceBarrier(1, &barrier);
    _state = dx12_image_state<state>();
  }

  auto handle() const noexcept { return _handle.Get(); }
  auto width()  const noexcept { return _width;        }
  auto height() const noexcept { return _height;       }
  auto extent() const noexcept { return glm::vec<2, uint32_t>{ _width, _height }; }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> _handle;
  D3D12_RESOURCE_STATES                  _state{};
  uint32_t                               _width{};
  uint32_t                               _height{};
};

template <ImageType SrcType, ImageFormat SrcFormat,
          ImageType DstType, ImageFormat DstFormat>
inline void copy(
  ID3D12GraphicsCommandList* cmd,
  Image<SrcType, SrcFormat>& src,
  LONG left,
  LONG top,
  LONG right,
  LONG bottom,
  Image<DstType, DstFormat>& dst,
  uint32_t x = 0,
  uint32_t y = 0) noexcept
{
  src.template set_state<ImageState::copy_src>(cmd);
  dst.template set_state<ImageState::copy_dst>(cmd);

  auto src_loc = CD3DX12_TEXTURE_COPY_LOCATION{ src.handle() };
  auto dst_loc = CD3DX12_TEXTURE_COPY_LOCATION{ dst.handle() };
  auto region_box = CD3DX12_BOX{ left, top, right, bottom };
  cmd->CopyTextureRegion(&dst_loc, x, y, 0, &src_loc, &region_box);
}

template <ImageType SrcType, ImageFormat SrcFormat,
          ImageType DstType, ImageFormat DstFormat>
inline void copy(
  ID3D12GraphicsCommandList* cmd,
  Image<SrcType, SrcFormat>& src,
  Image<DstType, DstFormat>& dst) noexcept
{
  copy(cmd, src, 0, 0, src.width(), src.height(), dst);
}

template <ImageType Type, ImageFormat Format>
inline void copy(
  ID3D12GraphicsCommandList* cmd,
  Image<Type, Format>&       image,
  ID3D12Resource*            upload_heap,
  uint32_t                   offset,
  D3D12_SUBRESOURCE_DATA&    data
)
{
  image.template set_state<ImageState::copy_dst>(cmd);
  UpdateSubresources(cmd, image.handle(), upload_heap, offset, 0, 1, &data);
}

}}
