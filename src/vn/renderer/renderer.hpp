#pragma once

#include "image.hpp"
#include "memory_allocator.hpp"
#include "window_system.hpp"

#include <glm/glm.hpp>
#include <rigtorp/SPSCQueue.h>

#include <vector>
#include <thread>
#include <atomic>
#include <semaphore>
#include <functional>
#include <deque>
#include <variant>

namespace vn { 

void init()    noexcept;
void destroy() noexcept;
void render()  noexcept;
  
}

namespace vn { namespace renderer {

class Renderer
{
  friend void vn::init()    noexcept;
  friend void vn::destroy() noexcept;
  friend void vn::render()  noexcept;
  
  friend class FrameBuffer;
  
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

private:
  void init()    noexcept;
  void destroy() noexcept;

  void create_swapchain_resources() noexcept;
  void create_frame_resources() noexcept;

  void create_blur_pipeline() noexcept;
  void create_pipeline_resources() noexcept;

  void run() noexcept;

  void update() noexcept;
  void render() noexcept;
  
  void acquire_render() noexcept { _render_acquire.release(); }

  void create_desktop_duplication() noexcept;
  void capture_desktop_image() noexcept;

  void add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept;

////////////////////////////////////////////////////////////////////////////////
///                                  Misc
////////////////////////////////////////////////////////////////////////////////

  std::deque<std::function<bool()>> _current_frame_render_finish_procs;
  std::thread                       _thread;
  std::atomic_bool                  _exit{ false };
  std::binary_semaphore             _render_acquire{ 0 };
  WindowResources                   _window_resources{};

  bool                              _desktop_image_is_captured{};

////////////////////////////////////////////////////////////////////////////////
///                            Swaochain Resources
////////////////////////////////////////////////////////////////////////////////

  using SwapchainImageType = Image<ImageType::rtv, ImageFormat::bgra8_unorm>;

  Microsoft::WRL::ComPtr<IDXGISwapChain4>      _swapchain;
  std::array<SwapchainImageType, Frame_Count>  _swapchain_images;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtv_heap;
  CD3DX12_VIEWPORT                             _viewport;
  CD3DX12_RECT                                 _scissor;

////////////////////////////////////////////////////////////////////////////////
///                          Pipeline Resources
////////////////////////////////////////////////////////////////////////////////

  // sdf pipeline, for future
  Microsoft::WRL::ComPtr<ID3D12PipelineState>     _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>     _root_signature;

  // blur pipeline
  Image<ImageType::srv, ImageFormat::bgra8_unorm> _desktop_image;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication>  _desk_dup;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>     _blur_root_signature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>     _blur_pipeline_state;

////////////////////////////////////////////////////////////////////////////////
///                          Frame Resources
////////////////////////////////////////////////////////////////////////////////

  struct FrameResource
  {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  command_allocator;
    Image<ImageType::srv, ImageFormat::bgra8_unorm> backdrop_image;
    Image<ImageType::uav, ImageFormat::bgra8_unorm> blur_backdrop_image;
    DescriptorHeap<DescriptorHeapType::cbv_srv_uav> heap;
  };
  std::array<FrameResource, Frame_Count> _frames;
  FrameBuffer                            _frame_buffer;
  std::vector<Vertex>                    _vertices;
  std::vector<uint16_t>                  _indices;
  uint16_t                               _idx_beg{};

////////////////////////////////////////////////////////////////////////////////
///                          Message Process
////////////////////////////////////////////////////////////////////////////////

public:

  struct Message_Update_Window_Resource
  {
    WindowResources window_resources;
  };
  struct Message_Update_Fullscreen_Region
  {
    HRGN region;
  };
  struct Message_Desktop_Image_Copy
  {
    bool start;
  };

  using Message = std::variant<
    Message_Update_Window_Resource,
    Message_Update_Fullscreen_Region,
    Message_Desktop_Image_Copy
  >;

  void push_message(Message const& msg) noexcept { return _message_queue.push(msg); }

private:
  void process_messages() noexcept;

private:
  rigtorp::SPSCQueue<Message> _message_queue{ 10 };
  bool                        _need_copy_desktop_image{};
};

}}