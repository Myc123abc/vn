#pragma once

#include "descriptor_heap_manager.hpp"

#include <dxgi1_6.h>
#include <directx/d3dx12.h>

#include <glm/glm.hpp>

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

struct BitmapView
{
  void*    data{};
  uint32_t width{};
  uint32_t height{};
  uint32_t channel{};
  uint32_t row_pitch{};
  uint32_t size{};
  uint32_t x{};
  uint32_t y{};

  void init(uint32_t width, uint32_t height, uint32_t channel) noexcept
  {
    this->width   = width;
    this->height  = height;
    this->channel = channel;
    row_pitch     = width * channel;
    size          = row_pitch * height;
  }
};

class Bitmap
{
public:
  auto data()            noexcept { return _view.data;      }
  auto size()      const noexcept { return _view.size;      }
  auto row_pitch() const noexcept { return _view.row_pitch; }
  auto width()     const noexcept { return _view.width;     }
  auto height()    const noexcept { return _view.height;    }
  auto x()         const noexcept { return _view.x;         }
  auto y()         const noexcept { return _view.y;         }
  auto view()      const noexcept { return _view;           }

  void set_pos(uint32_t x, uint32_t y) noexcept
  {
    _view.x = x;
    _view.y = y;
  }

  void init(uint32_t width, uint32_t height, uint32_t channel) noexcept
  {
    _view.init(width, height, channel);
    _view.data = malloc(width * height * channel);
  }

  void destroy() noexcept;

  void init(std::string_view path) noexcept;

private:
  BitmapView _view;
  bool       _use_stb{};
};

struct Win32Bitmap
{
  HBITMAP    handle{};
  BitmapView view{};

  void init(uint32_t width, uint32_t height) noexcept;
  void destroy() const noexcept;
};

class Image
{
public:
  Image()                        = default;
  ~Image()                       = default;
  Image(Image const&)            = default;
  Image(Image&&)                 = delete;
  Image& operator=(Image const&) = default;
  Image& operator=(Image&&)      = delete;

  void init(ImageType type, ImageFormat format, uint32_t width , uint32_t height) noexcept;
  void init(IDXGISwapChain1* swapchain, uint32_t index)                           noexcept;
  void init(ImageType type, HANDLE handle, uint32_t width, uint32_t height)       noexcept;

  void destroy() noexcept
  {
    _handle.Reset();
    _descriptor_handle.release();
  }

  void set_state(ID3D12GraphicsCommandList1* cmd, ImageState state) noexcept;

  void resize(uint32_t width, uint32_t height)            noexcept { init(_type, _format, width, height); }
  void resize(IDXGISwapChain1* swapchain, uint32_t index) noexcept { init(swapchain, index);              }

  void clear(ID3D12GraphicsCommandList1* cmd, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) const noexcept;
  void clear_render_target(ID3D12GraphicsCommandList1* cmd) noexcept;

  auto handle() const noexcept { return _handle.Get();                            }
  auto format() const noexcept { return _format;                                  }
  auto width()  const noexcept { return _width;                                   }
  auto height() const noexcept { return _height;                                  }
  auto extent() const noexcept { return glm::vec<2, uint32_t>{ _width, _height }; }

  auto per_pixel_size() const noexcept -> uint32_t;

  auto readback(ID3D12GraphicsCommandList1* cmd, RECT const& rect) noexcept -> std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, BitmapView>;

  auto cpu_handle() const noexcept { return _descriptor_handle.cpu_handle(); }
  auto gpu_handle() const noexcept { return _descriptor_handle.gpu_handle(); }

private:
  void init(ImageType type, DXGI_FORMAT format, uint32_t width , uint32_t height) noexcept;
  void create_descriptor() noexcept;

private:
  ImageType                              _type{};
  DXGI_FORMAT                            _format{};
  Microsoft::WRL::ComPtr<ID3D12Resource> _handle;
  D3D12_RESOURCE_STATES                  _state{};
  uint32_t                               _width{};
  uint32_t                               _height{};
  DescriptorHandle                       _descriptor_handle;
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
) noexcept
{
  image.set_state(cmd, ImageState::copy_dst);
  UpdateSubresources(cmd, image.handle(), upload_heap, offset, 0, 1, &data);
}

void copy(
  ID3D12GraphicsCommandList1* cmd,
  Image&                      src,
  LONG                        left,
  LONG                        top,
  LONG                        right,
  LONG                        bottom,
  ID3D12Resource*             readback_buffer) noexcept;

void copy(BitmapView const& src, BitmapView const& dst) noexcept;

}}
