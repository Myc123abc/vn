#include "image.hpp"
#include "core.hpp"
#include "error_handling.hpp"
#include "../util.hpp"
#include "renderer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace vn;
using namespace vn::renderer;
using namespace Microsoft::WRL;

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
    { DXGI_FORMAT_B8G8R8A8_UNORM, 4 },
  };
  err_if(!map.contains(format), "unsupport dxgi format for byte size now");
  return map.at(format);
}

}

namespace vn { namespace renderer {

////////////////////////////////////////////////////////////////////////////////
///                               Bitmap
////////////////////////////////////////////////////////////////////////////////

void Win32Bitmap::init(uint32_t width, uint32_t height) noexcept
{
  view.init(width, height, 4);

  auto hdc_mem = CreateCompatibleDC(nullptr);
  err_if(!hdc_mem, "failed to create compatible DC");
  auto info = BITMAPINFO{};
  info.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth    = width;
  info.bmiHeader.biHeight   = -height;
  info.bmiHeader.biPlanes   = 1;
  info.bmiHeader.biBitCount = 32;
  handle = CreateDIBSection(hdc_mem, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&view.data), nullptr, 0);
  err_if(!handle, "failed to create DIBSection");
  DeleteDC(hdc_mem);
}
  
void Win32Bitmap::destroy() const noexcept
{
  err_if(!DeleteObject(handle), "failed to destroy win32 bitmap");
}

void Bitmap::init(std::string_view filename) noexcept
{
  _use_stb = true;
  _view.data = stbi_load(filename.data(), reinterpret_cast<int*>(&_view.width), reinterpret_cast<int*>(&_view.height), reinterpret_cast<int*>(&_view.channel), 4);
  err_if(!_view.data, "failed to load image {}", filename);
  _view.channel = 4;
  _view.init(_view.width, _view.height, _view.channel);
}

void Bitmap::destroy() noexcept
{
  _use_stb
    ? stbi_image_free(_view.data)
    : free(_view.data);
  _use_stb = false;
}

////////////////////////////////////////////////////////////////////////////////
///                                 Image
////////////////////////////////////////////////////////////////////////////////

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

void Image::init(ImageType type, DXGI_FORMAT format, uint32_t width , uint32_t height) noexcept
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
  
  create_descriptor();
}  

void Image::init(ImageType type, ImageFormat format, uint32_t width , uint32_t height) noexcept
{
  init(type, dxgi_format(format), width, height);
}

void Image::init(IDXGISwapChain1* swapchain, uint32_t index) noexcept
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
  create_descriptor();
}

void Image::init(ImageType type, HANDLE handle, uint32_t width, uint32_t height) noexcept
{
  _state  = dx12_resource_state(type);
  _width  = width;
  _height = height;
  err_if(Core::instance()->device()->OpenSharedHandle(handle, IID_PPV_ARGS(_handle.ReleaseAndGetAddressOf())), "failed to share d3d11 texture");
  create_descriptor();
}

void Image::set_state(ID3D12GraphicsCommandList1* cmd, ImageState state) noexcept
{
  auto transition_state = dx12_resource_state(state);
  if (_state == transition_state) return;
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_handle.Get(), _state, transition_state);
  cmd->ResourceBarrier(1, &barrier);
  _state = transition_state;
}
  
void Image::create_descriptor() noexcept
{
  static auto device = Core::instance()->device();
  auto        mgr    = DescriptorHeapManager::instance();

  auto create_unordered_access_view = [&]
  {
    auto uav_desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{};
    uav_desc.Format        = _format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(_handle.Get(), nullptr, &uav_desc, _descriptor_handle.cpu_handle());
  };
  auto create_render_target_view = [&]
  {
    device->CreateRenderTargetView(_handle.Get(), nullptr, _descriptor_handle.cpu_handle());
  };
  auto create_shader_resource_view = [&]
  {
    auto srv_desc = D3D12_SHADER_RESOURCE_VIEW_DESC{};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format                  = _format;
    srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels     = 1;
    device->CreateShaderResourceView(_handle.Get(), &srv_desc, _descriptor_handle.cpu_handle());
  };
  auto create_depth_stencil_view = [&]
  {
    auto dsv_desc = D3D12_DEPTH_STENCIL_VIEW_DESC{};
    dsv_desc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(_handle.Get(), &dsv_desc, _descriptor_handle.cpu_handle());
  };

  // first initialize image, get descriptor handle
  if (!_descriptor_handle.is_valid())
  {
    if (_type == ImageType::uav)
      _descriptor_handle = mgr->pop_handle(DescriptorHeapType::cbv_srv_uav, create_unordered_access_view);
    else if (_type == ImageType::rtv)
      _descriptor_handle = mgr->pop_handle(DescriptorHeapType::rtv, create_render_target_view);
    else if (_type == ImageType::srv)
      _descriptor_handle = mgr->pop_handle(DescriptorHeapType::cbv_srv_uav, create_shader_resource_view);
    else if (_type == ImageType::dsv)
      _descriptor_handle = mgr->pop_handle(DescriptorHeapType::dsv, create_depth_stencil_view);
    else
      std::unreachable();
  }

  // create descriptor
  if (_type == ImageType::uav)
    create_unordered_access_view();
  else if (_type == ImageType::rtv)
    create_render_target_view();
  else if (_type == ImageType::srv)
    create_shader_resource_view(); 
  else if (_type == ImageType::dsv)
    create_depth_stencil_view();
  else
    std::unreachable();
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

void Image::clear_render_target(ID3D12GraphicsCommandList1* cmd) noexcept
{
  err_if(_type != ImageType::rtv, "clear render target only use on rtv");
  set_state(cmd, ImageState::render_target);
  float constexpr clear_color[4]{};
  cmd->ClearRenderTargetView(cpu_handle(), clear_color, 0, nullptr);
}

auto Image::per_pixel_size() const noexcept -> uint32_t
{
  return byte_size_of(_format);
}

auto Image::readback(ID3D12GraphicsCommandList1* cmd, RECT const& rect) noexcept -> std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, BitmapView>
{
  err_if(per_pixel_size() != 4, "readback only support rgba image now");

  auto left = std::max(rect.left, 0l);
  auto top  = std::max(rect.top, 0l);

  // create bitmap view
  auto view = BitmapView{};
  view.x      = left;
  view.y      = top;
  view.width  = rect.right  - view.x;
  view.height = rect.bottom - view.y;

  // create readback buffer
  auto readback_buffer = ComPtr<ID3D12Resource>{};
  auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  view.row_pitch       = align(view.width * per_pixel_size(), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  auto heap_desc       = CD3DX12_RESOURCE_DESC::Buffer(align(view.row_pitch * view.height, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
  err_if(Core::instance()->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &heap_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_buffer)),
          "failed to create readback buffer");

  // get pointer of readback buffer
  auto range = D3D12_RANGE{ 0, heap_desc.Width };
  err_if(readback_buffer->Map(0, &range, reinterpret_cast<void**>(&view.data)), "failed to map readback buffer to pointer");

  // copy data from gpu to cpu
  copy(cmd, *this, view.x, view.y, rect.right, rect.bottom, readback_buffer.Get());

  return { readback_buffer, view };
}

////////////////////////////////////////////////////////////////////////////////
///                             Copy Operations
////////////////////////////////////////////////////////////////////////////////

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

void copy(BitmapView const& src, BitmapView const& dst) noexcept
{
  auto src_data = reinterpret_cast<std::byte*>(src.data);
  auto dst_data = reinterpret_cast<std::byte*>(dst.data);
  for (auto i = 0; i < dst.height; ++i)
  {
    memcpy(dst_data, src_data, src.width * 4);
    src_data += src.row_pitch;
    dst_data += dst.row_pitch;
  }
}

////////////////////////////////////////////////////////////////////////////////
///                             Upload Buffer
////////////////////////////////////////////////////////////////////////////////

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
  // calculate required intermediate sizes
  auto intermediate_sizes = std::vector<uint32_t>{};
  intermediate_sizes.reserve(_infos.size());
  for (auto const& info : _infos)
    intermediate_sizes.emplace_back(
      align(GetRequiredIntermediateSize(g_image_pool[info.handle].handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));

  // initialize buffer
  auto size = std::ranges::fold_left(intermediate_sizes, 0, std::plus<>{});
  if (_buffer.capacity() < size)
    _buffer.init(size, false);

  // copy bitmap data to image by upload buffer
  auto offset = uint32_t{};
  for (auto i = 0; i < _infos.size(); ++i)
  {
    copy(cmd, g_image_pool[_infos[i].handle], _buffer.handle(), offset, _infos[i].data);
    offset += intermediate_sizes[i];
  }

  _infos.clear();
}

////////////////////////////////////////////////////////////////////////////////
///                        External Image Loaderer
////////////////////////////////////////////////////////////////////////////////

void ExternalImageLoader::load(std::string_view filename) noexcept
{
  err_if(_datas.contains(filename.data()), "Failed to load {}. It's already loaded", filename);
  _datas[filename.data()].init(filename);
}

void ExternalImageLoader::remove(std::string_view filename) noexcept
{
  err_if(!_datas.contains(filename.data()), "Failed to remove {}. It's not exist", filename);
  auto& data = _datas[filename.data()];
  if (data.state == State::unuploaded) data.bitmap.destroy();
  g_renderer.add_current_frame_render_finish_proc([handle = data.handle] mutable { g_image_pool.free(handle); });
  _datas.erase(filename.data());
}

void ExternalImageLoader::Data::init(std::string_view filename) noexcept
{
  bitmap.init(filename);
  handle = g_image_pool.alloc();
  g_image_pool[handle].init(ImageType::srv, ImageFormat::rgba8_unorm, bitmap.width(), bitmap.height());
}

void ExternalImageLoader::upload(ID3D12GraphicsCommandList1* cmd) noexcept
{
  auto unuploaded_datas = _datas
    | std::views::values
    | std::views::filter([](auto const& data) { return data.state == State::unuploaded; });
  auto handles = unuploaded_datas
    | std::views::transform([](auto const& data) { return data.handle; })
    | std::ranges::to<std::vector<ImageHandle>>();
  auto views = unuploaded_datas
    | std::views::transform([](auto const& data) { return data.bitmap.view(); })
    | std::ranges::to<std::vector<BitmapView>>();

  _upload_buffer.add_images(handles, views);
  _upload_buffer.upload(cmd);
  
  auto unuploaded_data_filenames = _datas
    | std::views::filter([](auto const& pair) { return pair.second.state == State::unuploaded; })
    | std::views::keys
    | std::ranges::to<std::vector<std::string>>();
  g_renderer.add_current_frame_render_finish_proc([unuploaded_data_filenames]
  {
    std::ranges::for_each(unuploaded_data_filenames, [](auto& filename)
    {
      g_external_image_loader.upload_finish(filename);
    });
  });

  std::ranges::for_each(unuploaded_datas, [](auto& data)
  {
    data.bitmap.destroy();
    data.state = State::uploading;
  });
}

void ExternalImageLoader::destroy() noexcept
{
  std::ranges::for_each(_datas | std::views::values, [](auto& data)
  {
    if (data.state == State::unuploaded) data.bitmap.destroy();
    g_image_pool.free(data.handle);
  });
  _datas.clear();
}

auto ExternalImageLoader::operator[](std::string_view filename) noexcept -> Image&
{
  err_if(!_datas.contains(filename.data()), "Failed to remove {}. It's not exist", filename);
	_datas[filename.data()].last_fence_value = Core::instance()->fence_value();
  return g_image_pool[_datas[filename.data()].handle];
}

void ExternalImageLoader::upload_finish(std::string_view filename) noexcept
{
  err_if(!_datas.contains(filename.data()), "Failed to remove {}. It's not exist", filename);
  _datas[filename.data()].state = State::uploaded;
}

auto ExternalImageLoader::is_uploaded(std::string_view filename) const noexcept -> bool
{
  err_if(!_datas.contains(filename.data()), "Failed to remove {}. It's not exist", filename);
  return _datas.at(filename.data()).state == State::uploaded;
}

}}
