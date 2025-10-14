#pragma once

#include "window_resource.hpp"

#include <functional>
#include <deque>

namespace vn { namespace renderer {

class Renderer
{
  friend class MessageQueue;
  friend class WindowResource;

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

  void run() noexcept;

private:
  void create_pipeline_resource() noexcept;

  void load_cursor_images() noexcept;

  void update() noexcept;
  void render() noexcept;

private:
  std::deque<std::function<bool()>> _current_frame_render_finish_procs;

  struct Cursor
  {
    Image<ImageType::srv, ImageFormat::rgba8_unorm> image;
    glm::vec2                                       pos;
  };
  std::unordered_map<CursorType, Cursor> _cursors;

  // descriptor heap
  // cursor textures + frame buffer
  DescriptorHeap<
    DescriptorHeapType::cbv_srv_uav,
    static_cast<uint32_t>(CursorType::Number) + 1> _srv_heap;
  
  FrameBuffer _buffer;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

  std::unordered_map<HWND, WindowResource> _window_resources;
  SwapchainResource                        _fullscreen_swapchain_resource;
};

}}