#include "image_manager.hpp"
#include "error_handling.hpp"
#include "renderer.hpp"

#include <ranges>
#include <algorithm>

namespace vn { namespace renderer {

void ImageManager::add_image(std::string_view path) noexcept
{
  err_if(_datas.contains(path.data()), "image {} is already load", path);
  _datas[path.data()].init(path);
}

void ImageManager::remove_image(std::string_view path) noexcept
{
  err_if(!_datas.contains(path.data()), "image {} is not loaded, cannot be removed", path);

  auto& data = _datas[path.data()];

  Renderer::instance()->add_current_frame_render_finish_proc([image = data.image] mutable { image.destroy(); });

  if (!data.uploaded)
    data.bitmap.destroy();

  _datas.erase(path.data());
}

void ImageManager::upload(ID3D12GraphicsCommandList1* cmd) noexcept
{
  // get unuploaded images
  auto datas = _datas
    | std::views::values
    | std::views::filter([](auto& data) { return !data.uploaded; });

  // upload by buffer
  _upload_buffer.add_images(
    datas
      | std::views::transform([](auto& data) { return &data.image; })
      | std::ranges::to<std::vector<Image*>>(),
    datas
      | std::views::transform([](auto& data) { return data.bitmap.view(); })
      | std::ranges::to<std::vector<BitmapView>>());
  _upload_buffer.upload(cmd);

  // destroy bitmaps
  std::ranges::for_each(datas, [](auto& data)
  {
    data.uploaded = true;
    data.bitmap.destroy();
  });
}

}}
