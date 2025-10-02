#pragma once

#include "image.hpp"
#include "memory_allocator.hpp"
#include "window_manager.hpp"

#include <windows.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>

#include <rigtorp/SPSCQueue.h>
#include <glm/glm.hpp>

#include <functional>
#include <thread>
#include <atomic>
#include <semaphore>
#include <deque>
#include <variant>

namespace vn { namespace renderer {

class Renderer
{
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

  void init() noexcept;
  void destroy() noexcept;

  void create_pipeline_resource() noexcept;

  void run() noexcept;

  void update() noexcept;
  void render() noexcept;

  void acquire_render() noexcept { _render_acquire.release(); }

  void add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept;

private:
  std::thread                       _thread;
  std::atomic_bool                  _exit{ false };
  std::binary_semaphore             _render_acquire{ 0 };
  std::deque<std::function<bool()>> _current_frame_render_finish_procs;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;

////////////////////////////////////////////////////////////////////////////////
///                          Window Resource
////////////////////////////////////////////////////////////////////////////////

private:
  using SwapchainImageType = Image<ImageType::rtv, ImageFormat::bgra8_unorm>;

  struct WindowResource
  {
    Window window;

    Microsoft::WRL::ComPtr<IDXGISwapChain4>      swapchain;
    std::array<SwapchainImageType, Frame_Count>  swapchain_images;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap;
    CD3DX12_VIEWPORT                             viewport;
    CD3DX12_RECT                                 scissor;

    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, Frame_Count> command_allocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd;

    FrameBuffer           frame_buffer;
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    uint16_t              idx_beg{};

    WindowResource() = default;
    WindowResource(Window const& window) noexcept;

    auto swapchain_image() noexcept { return &swapchain_images[swapchain->GetCurrentBackBufferIndex()]; }

    void render() noexcept;
  };

  std::vector<WindowResource> _window_resources;
  WindowResource              _fullscreen_window_resource;

////////////////////////////////////////////////////////////////////////////////
///                          Message Process
////////////////////////////////////////////////////////////////////////////////

public:

  struct Message_Create_Window_Render_Resource
  {
    Window window;
  };

  struct Message_Destroy_Window_Render_Resource
  {
    HWND handle;
  };

  struct Message_Create_Fullscreen_Window_Render_Resource
  {
    Window window;
  };

  struct Message_Update_Window
  {
    Window window;
  };

  struct Message_Resize_Window
  {
    Window window;
  };

  using Message = std::variant<
    Message_Create_Window_Render_Resource,
    Message_Destroy_Window_Render_Resource,
    Message_Create_Fullscreen_Window_Render_Resource,
    Message_Update_Window,
    Message_Resize_Window
  >;

  void send_message(Message const& msg) noexcept { return _message_queue.push(msg); }

private:
  void process_messages() noexcept;

private:
  rigtorp::SPSCQueue<Message> _message_queue{ 10 };
};

}}