#include "renderer.hpp"
#include "core.hpp"
#include "error_handling.hpp"
#include "message_queue.hpp"
#include "../ui/ui_context.hpp"
#include "compiler.hpp"
#include "descriptor_heap_manager.hpp"
#include "image.hpp"

#include <algorithm>
#include <ranges>

using namespace vn;
using namespace vn::renderer;
using namespace Microsoft::WRL;

namespace {

auto load_cursor_bitmap(LPCSTR idc_cursor) noexcept
{
  auto cursor = LoadCursorA(nullptr, idc_cursor);
  err_if(!cursor, "failed to load cursor");

  auto info = ICONINFO{};
  err_if(!GetIconInfo(cursor, &info), "failed to get cursor information");

  auto bitmap = BITMAP{};
  err_if(!GetObjectA(info.hbmColor, sizeof(bitmap), &bitmap), "failed to get bitmap of cursor");

  auto cursor_bitmap = Bitmap{};
  cursor_bitmap.init(bitmap.bmWidth, bitmap.bmHeight, bitmap.bmWidthBytes / bitmap.bmWidth);
  GetBitmapBits(info.hbmColor, cursor_bitmap.size(), cursor_bitmap.data());

  err_if(!DeleteObject(info.hbmColor), "failed to delete cursor information object");
  err_if(!DeleteObject(info.hbmMask), "failed to delete cursor information object");

  // get cursor position of bitmap
  auto min_x = uint32_t{};
  auto min_y = uint32_t{};
  auto max_x = uint32_t{};
  auto max_y = uint32_t{};
  auto p     = reinterpret_cast<uint8_t*>(cursor_bitmap.data());
  assert(bitmap.bmWidthBytes / bitmap.bmWidth == 4);
  for (int y = 0; y < cursor_bitmap.height(); ++y)
  {
    for (int x = 0; x < cursor_bitmap.width(); ++x)
    {
      auto idx = y * cursor_bitmap.row_pitch() + x * 4;
      if (p[idx] != 0 || p[idx + 1] != 0 || p[idx + 2] != 0)
      {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x= x;
        if (y > max_y) max_y = y;
      }
    }
  }

  cursor_bitmap.set_pos((max_x - min_x) / 2 + min_x, (max_y - min_y) / 2 + min_y);

  return cursor_bitmap;
}

}

////////////////////////////////////////////////////////////////////////////////
///                             Renderer
////////////////////////////////////////////////////////////////////////////////

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  auto core = Core::instance();

  Compiler::instance()->init();

  core->init();
  DescriptorHeapManager::instance()->init();

  load_cursor_images();

  create_pipeline_resource();
}

void Renderer::destroy() noexcept
{
  Core::instance()->wait_gpu_complete();
  std::ranges::for_each(_window_resources | std::views::values, [](auto& wr) { wr.destroy(); });
  std::ranges::for_each(_cursors | std::views::values, [&](auto& cursor) { g_image_pool.free(cursor.handle); });
  g_external_image_loader.destroy();
  Core::instance()->destroy();
}

void Renderer::create_pipeline_resource() noexcept
{
  _sdf_pipeline.init_graphics("assets/shader.hlsl", "vs", "ps", "assets", SwapchainResource::Image_Format, true);

  // _window_shadow_pipeline.init_compute("assets/window_shadow.hlsl", "main");
  // _window_mask_pipeline.init_compute("assets/window_mask.hlsl", "main");

  // auto size = get_screen_size();
  // _uav_clear_heap.init(DescriptorHeapType::cbv_srv_uav, 1, true);
  // _window_mask_image.init(ImageType::uav, ImageFormat::r8_unorm, size.x, size.y)
  //                   .create_descriptor(_uav_clear_heap.pop_handle("window mask image"));
  // _cbv_srv_uav_heap.add_tag("window mask image", 1);

  // _window_shadow_image.init(ImageType::uav, ImageFormat::rgba8_unorm, size.x, size.y)
  //                     .create_descriptor(_cbv_srv_uav_heap.pop_handle("window shadow image"));
}

void Renderer::load_cursor_images() noexcept
{
  auto core = Core::instance();
  core->reset_cmd();

  // get bitmaps of all cursor types
  auto bitmaps = std::unordered_map<CursorType, Bitmap>{};
  using enum CursorType;
  bitmaps[arrow]         = load_cursor_bitmap(IDC_ARROW);
  bitmaps[up_down]       = load_cursor_bitmap(IDC_SIZENS);
  bitmaps[left_rigtht]   = load_cursor_bitmap(IDC_SIZEWE);
  bitmaps[diagonal]      = load_cursor_bitmap(IDC_SIZENESW);
  bitmaps[anti_diagonal] = load_cursor_bitmap(IDC_SIZENWSE);

  // create cursors
  for (auto& [cursor_type, bitmap] : bitmaps)
  {
    _cursors[cursor_type].handle = g_image_pool.alloc();
    g_image_pool[_cursors[cursor_type].handle].init(ImageType::srv, ImageFormat::rgba8_unorm, bitmap.width(), bitmap.height());
    _cursors[cursor_type].pos = { bitmap.x(), bitmap.y() };
  }

  // create upload buffer
  auto upload_buffer = UploadBuffer{};
  upload_buffer.add_images(
    _cursors
      | std::views::values
      | std::views::transform([](auto& image) { return image.handle; })
      | std::ranges::to<std::vector<ImageHandle>>(),
    bitmaps
      | std::views::values
      | std::views::transform([](auto& bitmap) { return bitmap.view(); })
      | std::ranges::to<std::vector<BitmapView>>());

  upload_buffer.upload(core->cmd());
  std::ranges::for_each(_cursors | std::views::values, [&](auto& cursor)
    { g_image_pool[cursor.handle].set_state(core->cmd(), ImageState::pixel_shader_resource); });

  // TODO: move to global and upload heap should be global too
  // wait gpu resources prepare complete
  core->submit(core->cmd());
  core->wait_gpu_complete();

  std::ranges::for_each(bitmaps | std::views::values, [](auto& image) { image.destroy(); });
}

void Renderer::add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept
{
  _current_frame_render_finish_procs.emplace_back([func, last_fence_value = Core::instance()->signal()]()
  {
    auto fence_value = Core::instance()->fence()->GetCompletedValue();
    err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    auto render_complete = fence_value >= last_fence_value;
    if (render_complete) func();
    return render_complete;
  });
}

void Renderer::message_process() noexcept
{
  // process last render finish processes
  for (auto it = _current_frame_render_finish_procs.begin(); it != _current_frame_render_finish_procs.end();)
    (*it)() ? it = _current_frame_render_finish_procs.erase(it) : ++it;

  MessageQueue::instance()->process_messages();

  // upload images
  if (g_external_image_loader.have_unuploaded_images())
  {
    auto core = Core::instance();
    core->reset_cmd();
    g_external_image_loader.upload(core->cmd());
    core->submit(core->cmd());
  }
}

void Renderer::render(HWND handle, ui::WindowRenderData const& data) noexcept
{
  err_if(!_window_resources.contains(handle), "unknow window resource window when rendering");
  _window_resources[handle].render(data.vertices, data.indices, data.shape_properties);
}

void Renderer::render_fullscreen(HWND handle, ui::WindowRenderData const& data) noexcept
{
  _fullscreen_resource.render(data.vertices, data.indices, data.shape_properties, _window_resources[handle].window);
}

void Renderer::present(HWND handle, bool vsync) const noexcept
{
  err_if(!_window_resources.contains(handle), "unknow window resource window when rendering");
  _window_resources.at(handle).present(vsync);
}

}}
