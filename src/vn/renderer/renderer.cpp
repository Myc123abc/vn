#include "renderer.hpp"
#include "core.hpp"
#include "error_handling.hpp"
#include "message_queue.hpp"
#include "../ui/ui_context.hpp"
#include "compiler.hpp"
#include "window_manager.hpp"
#include "../util.hpp"

#include <algorithm>
#include <ranges>

using namespace vn;
using namespace Microsoft::WRL;

namespace {

struct Bitmap
{
  std::vector<std::byte> pixels;
  uint32_t               width{};
  uint32_t               height{};
  uint32_t               pixel_size{};
  glm::vec2              pos{};

  auto data()            noexcept { return pixels.data();               }
  auto byte_size() const noexcept { return width * height * pixel_size; }
  auto row_pitch() const noexcept { return width * pixel_size;          }

  void init(uint32_t width, uint32_t height, uint32_t pixel_size) noexcept
  {
    this->width      = width;
    this->height     = height;
    this->pixel_size = pixel_size;
    pixels.resize(byte_size());
  }
};

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
  GetBitmapBits(info.hbmColor, cursor_bitmap.byte_size(), cursor_bitmap.data());

  err_if(!DeleteObject(info.hbmColor), "failed to delete cursor information object");
  err_if(!DeleteObject(info.hbmMask), "failed to delete cursor information object");

  // get cursor position of bitmap
  auto min_x = uint32_t{};
  auto min_y = uint32_t{};
  auto max_x = uint32_t{};
  auto max_y = uint32_t{};
  auto p     = reinterpret_cast<uint8_t*>(cursor_bitmap.data());
  assert(bitmap.bmWidthBytes / bitmap.bmWidth == 4);
  for (int y = 0; y < cursor_bitmap.height; ++y)
  {
    for (int x = 0; x < cursor_bitmap.width; ++x)
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

  cursor_bitmap.pos.x = static_cast<float>(max_x - min_x) / 2 + min_x;
  cursor_bitmap.pos.y = static_cast<float>(max_y - min_y) / 2 + min_y;

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

  _cbv_srv_uav_heap.init(DescriptorHeapType::cbv_srv_uav, static_cast<uint32_t>(CursorType::Number) + Frame_Count + 2);

  load_cursor_images();

  // create buffers
  _cbv_srv_uav_heap.add_tag("framebuffer");
  for (auto& buf : _frame_buffers) buf.init(_cbv_srv_uav_heap.pop_handle());

  create_pipeline_resource();
}

void Renderer::destroy() noexcept
{
  Core::instance()->wait_gpu_complete();
  Core::instance()->destroy();
}

void Renderer::create_pipeline_resource() noexcept
{
  _pipeline.init_graphics("assets/shader.hlsl", "vs", "ps", "assets", ImageFormat::rgba8_unorm, true);
  
  _window_shadow_pipeline.init_compute("assets/window_shadow.hlsl", "main");
  _window_mask_pipeline.init_compute("assets/window_mask.hlsl", "main");

  auto size = get_screen_size();
  _uav_clear_heap.init(DescriptorHeapType::cbv_srv_uav, 1, true);
  _window_mask_image.init(ImageType::uav, ImageFormat::r8_unorm, size.x, size.y)
                    .create_descriptor(_uav_clear_heap.pop_handle("window mask image"));
  _cbv_srv_uav_heap.add_tag("window mask image", 1);
  
  _window_shadow_image.init(ImageType::uav, ImageFormat::rgba8_unorm, size.x, size.y)
                      .create_descriptor(_cbv_srv_uav_heap.pop_handle("window shadow image"));
}

void Renderer::load_cursor_images() noexcept
{
  auto core = Core::instance();

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
    _cursors[cursor_type].image.init(ImageType::srv, ImageFormat::rgba8_unorm, bitmap.width, bitmap.height);
    _cursors[cursor_type].pos = bitmap.pos;
  }

  // create upload heap
  auto upload_heap      = ComPtr<ID3D12Resource>{};
  auto heap_properties  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto upload_heap_desc = CD3DX12_RESOURCE_DESC::Buffer(std::ranges::fold_left(_cursors | std::views::values, 0,
    [](uint32_t sum, auto const& cursor) { return sum + align(GetRequiredIntermediateSize(cursor.image.handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT); }));
  err_if(core->device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &upload_heap_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap)),
          "failed to create upload heap");

  // upload bitmap to cursor tesxtures
  auto offset = uint32_t{};
  _cbv_srv_uav_heap.add_tag("cursors");
  for (auto const& [index, pair] : std::views::enumerate(bitmaps))
  {
    auto& [cursor_type, bitmap] = pair;
    auto& cursor_image          = _cursors[cursor_type].image;

    // upload bitmap
    auto texture_data = D3D12_SUBRESOURCE_DATA{};
    texture_data.pData      = bitmap.data();
    texture_data.RowPitch   = bitmap.width * bitmap.pixel_size;
    texture_data.SlicePitch = texture_data.RowPitch * bitmap.height;
    copy(core->cmd(), cursor_image, upload_heap.Get(), offset, texture_data);

    // convert state to pixel shader resource
    cursor_image.set_state(core->cmd(), ImageState::pixel_shader_resource);

    // move to next upload heap position
    offset += align(GetRequiredIntermediateSize(cursor_image.handle(), 0, 1), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    // create cursor texture descriptor
    cursor_image.create_descriptor(_cbv_srv_uav_heap.pop_handle());
  }

  // TODO: move to global and upload heap should be global too
  // wait gpu resources prepare complete
  core->submit(core->cmd());
  core->wait_gpu_complete();
}

void Renderer::add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept
{
  _current_frame_render_finish_procs.emplace_back([func, last_fence_value = Core::instance()->get_last_fence_value()]()
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
}

void Renderer::render_begin() noexcept
{
  auto core = Core::instance();
  _frame_buffers[core->frame_index()].clear();
  core->frame_begin();
}

void Renderer::render_end() noexcept
{
  Core::instance()->frame_end();
}

void Renderer::render_window(HWND handle, ui::WindowRenderData const& data) noexcept
{
  err_if(!_window_resources.contains(handle), "unknow window resource window when rendering");
  _window_resources[handle].render(data.vertices, data.indices, data.shape_properties);
}

}}
