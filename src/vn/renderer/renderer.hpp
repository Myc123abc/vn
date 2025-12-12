#pragma once

#include "window_resource.hpp"
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

  void resize_window(HWND handle, uint32_t width, uint32_t height) noexcept { return _window_resources[handle].resize(width, height); }

  void add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept;

  void message_process() noexcept;

  void render(HWND handle, ui::WindowRenderData const& data) noexcept;
  void render_fullscreen(HWND handle, ui::WindowRenderData const& data) noexcept;
  void present(HWND handle, bool vsync = false) const noexcept;
  void present_fullscreen(bool vsync = false) const noexcept { _fullscreen_resource.present(vsync); }
	void clear_window(HWND handle) noexcept { _window_resources.at(handle).clear_window(); }
  void clear_fullscreen() noexcept { _fullscreen_resource.clear_window(); }

  static constexpr auto enable_depth_test{ false };

  // auto get_descriptor(std::string_view tag, uint32_t offset = {}) const noexcept { return _cbv_srv_uav_heap.gpu_handle(tag, offset); }

private:
  void create_pipeline_resource() noexcept;

  void load_cursor_images() noexcept;

private:
  WindowResource                           _fullscreen_resource;
  std::unordered_map<HWND, WindowResource> _window_resources;
  std::deque<std::function<bool()>>        _current_frame_render_finish_procs;
  Pipeline                                 _sdf_pipeline;

  // Pipeline _window_mask_pipeline;
  // Image    _window_mask_image;
  // Pipeline _window_shadow_pipeline;
  // Image    _window_shadow_image;

  struct Cursor
  {
    Image     image;
    glm::vec2 pos;
  };
  std::unordered_map<CursorType, Cursor> _cursors;
};

}}
