#pragma once

#include "buffer.hpp"

namespace vn { namespace renderer {

class ImageManager
{
private:
  ImageManager()                               = default;
  ~ImageManager()                              = default;
public:
  ImageManager(ImageManager const&)            = delete;
  ImageManager(ImageManager&&)                 = delete;
  ImageManager& operator=(ImageManager const&) = delete;
  ImageManager& operator=(ImageManager&&)      = delete;

  static auto const instance() noexcept
  {
    static ImageManager instance;
    return &instance;
  }

  void add_image(std::string_view path) noexcept;
  void upload(ID3D12GraphicsCommandList1* cmd) noexcept;
  void remove_image(std::string_view path) noexcept;

private:
  UploadBuffer _upload_buffer;
  struct Data
  {
    Bitmap bitmap;
    Image  image;
    bool   uploaded{};

    void init(std::string_view path) noexcept
    {
      assert(!uploaded);
      bitmap.init(path);
      // TODO: assume channel always 4
      assert(bitmap.view().channel == 4);
      image.init(ImageType::srv, ImageFormat::rgba8_unorm, bitmap.width(), bitmap.height());
    }
  };
  std::unordered_map<std::string, Data> _datas;
};

}}
