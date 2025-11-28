#pragma once

#include "window_resource.hpp"
#include "buffer.hpp"
#include "descriptor_heap.hpp"
#include "pipeline.hpp"

#include <functional>
#include <deque>

namespace vn { namespace ui {

struct UIContext;
struct WindowRenderData;

}}

namespace vn { namespace renderer {

class Renderer
{
  friend class MessageQueue;
  friend class WindowResource;
  friend class ui::UIContext;

private:
  Renderer()                           = default;
  ~Renderer()                          = default;
public:
  Renderer(Renderer const&)            = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer const&) = delete;
  Renderer& operator=(Renderer&&)      = delete;

  static auto const instance() noexcept
  {
    static Renderer instance;
    return &instance;
  }

  void init()    noexcept;
  void destroy() noexcept;

  void add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept;

  void message_process() noexcept;
  void render_begin()    noexcept;
  void render_end()      noexcept;

  void render_window(HWND handle, ui::WindowRenderData const& data) noexcept;

  static constexpr auto enable_depth_test{ false };

  auto get_descriptor(std::string_view tag, uint32_t offset = {}) const noexcept { return _cbv_srv_uav_heap.gpu_handle(tag, offset); }

private:
  void create_pipeline_resource() noexcept;

  void load_cursor_images() noexcept;

private:
  std::deque<std::function<bool()>> _current_frame_render_finish_procs;

  Pipeline _pipeline;

  Pipeline _window_mask_pipeline;
  Image    _window_mask_image;
  Pipeline _window_shadow_pipeline;
  Image    _window_shadow_image;

  Pipeline                       _window_thumbnail_pipeline;
  // FIXME: thumbnail_images need be move in every window resource
  DescriptorHeap                 _rtv_heap;
  std::array<Image, Frame_Count> _thumbnail_images;

  DescriptorHeap _uav_clear_heap;

  struct Cursor
  {
    Image     image;
    glm::vec2 pos;
  };
  std::unordered_map<CursorType, Cursor> _cursors;

  DescriptorHeap                       _cbv_srv_uav_heap;
  std::array<FrameBuffer, Frame_Count> _frame_buffers;

  std::unordered_map<HWND, WindowResource> _window_resources;
};

}}
