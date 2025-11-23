#include "image.hpp"
#include "core.hpp"
#include "error_handling.hpp"
#include "../util.hpp"

using namespace vn;
using namespace vn::renderer;

namespace {

auto dx12_resource_flag(ImageType type) noexcept
{
  using enum ImageType;
  auto static const map = std::unordered_map<ImageType, D3D12_RESOURCE_FLAGS>
  {
    { uav, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS },
    { rtv, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET    },
    { srv, D3D12_RESOURCE_FLAG_NONE                   },
    { dsv, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL    },
  };
  err_if(!map.contains(type), "unsupport image type now");
  return map.at(type);
}

auto dx12_resource_state(ImageType type) noexcept
{
  using enum ImageType;
  auto static const map = std::unordered_map<ImageType, D3D12_RESOURCE_STATES>
  {
    { uav, D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
    { rtv, D3D12_RESOURCE_STATE_PRESENT          },
    { srv, D3D12_RESOURCE_STATE_COMMON           },
    { dsv, D3D12_RESOURCE_STATE_DEPTH_WRITE      },
  };
  err_if(!map.contains(type), "unsupport image type now");
  return map.at(type);
}

auto dx12_resource_state(ImageState state) noexcept
{
  using enum ImageState;
  auto static const map = std::unordered_map<ImageState, D3D12_RESOURCE_STATES>
  {
    { copy_dst,              D3D12_RESOURCE_STATE_COPY_DEST             },
    { copy_src,              D3D12_RESOURCE_STATE_COPY_SOURCE           },
    { present,               D3D12_RESOURCE_STATE_PRESENT               },
    { unorder_access,        D3D12_RESOURCE_STATE_UNORDERED_ACCESS      },
    { common,                D3D12_RESOURCE_STATE_COMMON                },
    { render_target,         D3D12_RESOURCE_STATE_RENDER_TARGET         },
    { pixel_shader_resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
  };
  err_if(!map.contains(state), "unsupport image state now");
  return map.at(state);
}

auto byte_size_of(DXGI_FORMAT format) noexcept
{
  auto static const map = std::unordered_map<DXGI_FORMAT, uint32_t>
  {
    { DXGI_FORMAT_R8G8B8A8_UNORM, 4 },
  };
  err_if(!map.contains(format), "unsupport dxgi format for byte size now");
  return map.at(format);
}

}

namespace vn { namespace renderer {

auto dxgi_format(ImageFormat format) noexcept -> DXGI_FORMAT
{
  using enum ImageFormat;
  auto static const map = std::unordered_map<ImageFormat, DXGI_FORMAT>
  {
    { r8_unorm,    DXGI_FORMAT_R8_UNORM       },
    { bgra8_unorm, DXGI_FORMAT_B8G8R8A8_UNORM },
    { rgba8_unorm, DXGI_FORMAT_R8G8B8A8_UNORM },
    { d32,         DXGI_FORMAT_D32_FLOAT      },
  };
  err_if(!map.contains(format), "unsupport image format now");
  return map.at(format);
}

auto Image::init(ImageType type, DXGI_FORMAT format, uint32_t width , uint32_t height) noexcept -> Image&
{
  _type   = type;
  _format = format;
  _state  = dx12_resource_state(type);
  _width  = width;
  _height = height;

  // create image
  auto texture_desc = D3D12_RESOURCE_DESC{};
  texture_desc.Format           = _format;
  texture_desc.Width            = width;
  texture_desc.Height           = height;
  texture_desc.DepthOrArraySize = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture_desc.Flags            = dx12_resource_flag(type);
  auto heap_properties          = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
    
  auto clear_value = D3D12_CLEAR_VALUE{};
  if (type == ImageType::dsv)
  {
    clear_value.Format             = DXGI_FORMAT_D32_FLOAT;
    clear_value.DepthStencil.Depth = 1.f;
    err_if(Core::instance()->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, _state, &clear_value, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())),
            "failed to create image");
  }
  else if (type == ImageType::rtv)
  {
    clear_value.Format = texture_desc.Format;
    err_if(Core::instance()->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, _state, &clear_value, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())),
            "failed to create image");
  }
  else
    err_if(Core::instance()->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, _state, nullptr, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())),
            "failed to create image");

  return *this;
}  

auto Image::init(ImageType type, ImageFormat format, uint32_t width , uint32_t height) noexcept -> Image&
{
  return init(type, dxgi_format(format), width, height);
}

auto Image::init(IDXGISwapChain1* swapchain, uint32_t index) noexcept -> Image&
{
  _state = dx12_resource_state(ImageType::rtv);
  err_if(swapchain->GetBuffer(index, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())),
         "failed to get descriptor");
  DXGI_SWAP_CHAIN_DESC1 desc{};
  err_if(swapchain->GetDesc1(&desc), "failed to get swapchain description");
  _type   = ImageType::rtv;
  _format = desc.Format;
  _width  = desc.Width;
  _height = desc.Height;
  return *this;
}

auto Image::init(ImageType type, HANDLE handle, uint32_t width, uint32_t height) noexcept -> Image&
{
  _state  = dx12_resource_state(type);
  _width  = width;
  _height = height;
  err_if(Core::instance()->device()->OpenSharedHandle(handle, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())), "failed to share d3d11 texture");
  return *this;
}

void Image::resize(uint32_t width, uint32_t height) noexcept
{
  init(_type, _format, width, height);
  for (auto const& [k, v] : _cpu_handles)
    create_descriptor(v);
}

void Image::resize(IDXGISwapChain1* swapchain, uint32_t index) noexcept
{
  init(swapchain, index);
  for (auto const& [k, v] : _cpu_handles)
    create_descriptor(v);
}

void Image::set_state(ID3D12GraphicsCommandList1* cmd, ImageState state) noexcept
{
  auto transition_state = dx12_resource_state(state);
  if (_state == transition_state) return;
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_handle.Get(), _state, transition_state);
  cmd->ResourceBarrier(1, &barrier);
  _state = transition_state;
}
  
void Image::create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, std::optional<ImageType> type) noexcept
{
  if (!_cpu_handles.contains(type.value_or(_type))) _cpu_handles.emplace(type.value_or(_type), handle);

  auto device = Core::instance()->device();
  if (_type == ImageType::uav)
  {
    auto uav_desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{};
    uav_desc.Format        = _format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(_handle.Get(), nullptr, &uav_desc, handle);
  }
  else if (_type == ImageType::rtv)
  {
    device->CreateRenderTargetView(_handle.Get(), nullptr, handle);
  }
  else if (_type == ImageType::srv)
  {
    auto srv_desc = D3D12_SHADER_RESOURCE_VIEW_DESC{};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format                  = _format;
    srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels     = 1;
    device->CreateShaderResourceView(_handle.Get(), &srv_desc, handle);
  }
  else if (_type == ImageType::dsv)
  {
    auto dsv_desc = D3D12_DEPTH_STENCIL_VIEW_DESC{};
    dsv_desc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(_handle.Get(), &dsv_desc, handle);
  }
  else
    err_if(true, "unsupport image type now");
}

void Image::clear(ID3D12GraphicsCommandList1* cmd, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) const noexcept
{
  err_if(_type != ImageType::uav, "clear operator only use on uav");
  float values[4]{};
  auto rect = D3D12_RECT{};
  rect.right  = _width;
  rect.bottom = _height;
  cmd->ClearUnorderedAccessViewFloat(gpu_handle, cpu_handle, _handle.Get(), values, 1, &rect);
}

auto Image::per_pixel_size() const noexcept -> uint32_t
{
  return byte_size_of(_format);
}

void copy(
  ID3D12GraphicsCommandList1* cmd,
  Image&                      src,
  LONG                        left,
  LONG                        top,
  LONG                        right,
  LONG                        bottom,
  Image&                      dst,
  uint32_t                    x,
  uint32_t                    y) noexcept
{
  src.set_state(cmd, ImageState::copy_src);
  dst.set_state(cmd, ImageState::copy_dst);
  auto src_loc = CD3DX12_TEXTURE_COPY_LOCATION{ src.handle() };
  auto dst_loc = CD3DX12_TEXTURE_COPY_LOCATION{ dst.handle() };
  auto region_box = CD3DX12_BOX{ left, top, right, bottom };
  cmd->CopyTextureRegion(&dst_loc, x, y, 0, &src_loc, &region_box);
}

void copy(
  ID3D12GraphicsCommandList1* cmd,
  Image&                      src,
  LONG                        left,
  LONG                        top,
  LONG                        right,
  LONG                        bottom,
  ID3D12Resource*             readback_buffer) noexcept
{
  src.set_state(cmd, ImageState::copy_src);
  auto src_loc = CD3DX12_TEXTURE_COPY_LOCATION{ src.handle()    };
  auto region_box = CD3DX12_BOX{ left, top, right, bottom };

  auto footprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{};
  footprint.Footprint.Width    = right - left;
  footprint.Footprint.Height   = bottom - top;
  footprint.Footprint.Depth    = 1;
  footprint.Footprint.RowPitch = align(src.per_pixel_size() * footprint.Footprint.Width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  footprint.Footprint.Format   = src.format();
  auto dst_loc = CD3DX12_TEXTURE_COPY_LOCATION{ readback_buffer, footprint };

  cmd->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, &region_box);
}

}}
