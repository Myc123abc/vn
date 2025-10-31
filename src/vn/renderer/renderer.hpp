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
  void render_begin() noexcept;
  void render_end() noexcept;

  void render_window(HWND handle, ui::WindowRenderData const& data) noexcept;

  static constexpr auto enable_depth_test{ false };

  auto get_descriptor(std::string_view tag, uint32_t offset = {}) const noexcept { return _cbv_srv_uav_heap.gpu_handle(tag, offset); }

private:
  void create_pipeline_resource() noexcept;

  void load_cursor_images() noexcept;

private:
  std::deque<std::function<bool()>> _current_frame_render_finish_procs;

  Pipeline _pipeline;
  Pipeline _window_shadow_pipeline;

  struct Cursor
  {
    Image<ImageType::srv, ImageFormat::rgba8_unorm> image;
    glm::vec2                                       pos;
  };
  std::unordered_map<CursorType, Cursor> _cursors;

  DescriptorHeap<
    DescriptorHeapType::cbv_srv_uav,
    static_cast<uint32_t>(CursorType::Number) + Frame_Count + 1> _cbv_srv_uav_heap;
  std::array<FrameBuffer, Frame_Count> _frame_buffers;

  std::unordered_map<HWND, WindowResource> _window_resources;
  SwapchainResource                        _fullscreen_swapchain_resource;
};

}}
