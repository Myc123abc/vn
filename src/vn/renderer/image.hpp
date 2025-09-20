#pragma once

#include "../util.hpp"
#include "core.hpp"

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <directx/d3dx12.h>

#include <glm/glm.hpp>

#include <cstdint>

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
};

enum class ImageState
{
  copy_src,
  copy_dst,
  present,
  unorder_access,
  common,
};

template <ImageFormat T>
auto constexpr dx12_image_format()
{
  if constexpr (T == ImageFormat::bgra8_unorm)
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  else
    static_assert(true, "unsupport image format now");
}

template <ImageType T>
auto constexpr dx12_image_flags()
{
  if constexpr (T == ImageType::uav)
    return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  else if constexpr (T == ImageType::rtv)
    static_assert(true, "rtv not need the image initialize flags");
  else if constexpr (T == ImageType::srv)
    return 0;
  else
    static_assert(true, "unsupport image type now");
}

template <ImageType T>
auto constexpr dx12_init_image_state()
{
  if constexpr (T == ImageType::uav)
    return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  else if constexpr (T == ImageType::rtv)
    return D3D12_RESOURCE_STATE_PRESENT;
  else if constexpr (T == ImageType::srv)
    return D3D12_RESOURCE_STATE_COMMON;
  else
    static_assert(true, "unsupport image state now");
}

template <ImageState T>
auto constexpr dx12_image_state()
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
  else
    static_assert(true, "unsupport image state now");
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
    _state = dx12_init_image_state<Type>();

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

  auto init(IDXGISwapChain1* swapchain, uint32_t index) -> Image&
  {
    _state  = dx12_init_image_state<Type>();
    err_if(Type != ImageType::rtv, "must be rtv");
    err_if(swapchain->GetBuffer(index, IID_PPV_ARGS(&_handle)),
           "failed to get descriptor");
    return *this;
  }

  void init(HANDLE handle)
  {
    _state  = dx12_init_image_state<Type>();
    err_if(Core::instance()->device()->OpenSharedHandle(handle, IID_PPV_ARGS(&_handle)), "failed to share d3d11 texture");
  }

  void destroy() noexcept { _handle.Reset(); }

  void create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, bool shared = false) const noexcept
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
      srv_desc.Shader4ComponentMapping = shared ? D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING : 0;
      srv_desc.Format                  = dx12_image_format<Format>();
      srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Texture2D.MipLevels     = 1;
      Core::instance()->device()->CreateShaderResourceView(_handle.Get(), &srv_desc, handle);
    }
    else
      static_assert(true, "unsupport image type now");
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

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> _handle;
  D3D12_RESOURCE_STATES                  _state{};
};

template <ImageType SrcType, ImageFormat SrcFormat,
          ImageType DstType, ImageFormat DstFormat>
inline void copy(
  ID3D12GraphicsCommandList* cmd,
  Image<SrcType, SrcFormat>& src,
  glm::vec4 region,
  Image<DstType, DstFormat>& dst,
  glm::vec2 offset = {})
{
  src.template set_state<ImageState::copy_src>(cmd);
  dst.template set_state<ImageState::copy_dst>(cmd);

  auto src_loc = CD3DX12_TEXTURE_COPY_LOCATION{ src.handle() };
  auto dst_loc = CD3DX12_TEXTURE_COPY_LOCATION{ dst.handle() };
  auto region_box = CD3DX12_BOX
  { 
    static_cast<LONG>(region.x), 
    static_cast<LONG>(region.y), 
    static_cast<LONG>(region.z),
    static_cast<LONG>(region.w)
  };
  cmd->CopyTextureRegion(&dst_loc, offset.x, offset.y, 0, &src_loc, &region_box);
}

}}