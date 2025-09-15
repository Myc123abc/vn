#include "renderer.hpp"
#include "message_queue.hpp"
#include "../util.hpp"
#include "../window/window_manager.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <chrono>
#include <array>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  // init debug controller
#ifndef NDEBUG
  ComPtr<ID3D12Debug> debug_controller;
  err_if(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)),
          "failed to create d3d12 debug controller");
  debug_controller->EnableDebugLayer();
#endif

  // create factory
#ifndef NDEBUG
  err_if(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#else
  err_if(CreateDXGIFactory2(0, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#endif

  // create device
  ComPtr<IDXGIAdapter4> adapter;
  err_if(_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)),
          "failed to enum dxgi adapter");
  err_if(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device)),
          "failed to create d3d12 device");

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  err_if(_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&_command_queue)),
          "failed to create command queue");

  // get render target view descriptor size
  Render_Target_View_Descriptor_Size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // create fence resources
  err_if(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)),
          "failed to create fence");
  _fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  err_if(!_fence_event, "failed to create fence event");

  // create root signature
  auto root_parameters = std::array<CD3DX12_ROOT_PARAMETER, 1>{};
  root_parameters[0].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  auto signature_desc = CD3DX12_ROOT_SIGNATURE_DESC{ root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  err_if(D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
          "failed to create root signature");
  
  // create shaders
  ComPtr<ID3DBlob> vertex_shader;
  ComPtr<ID3DBlob> pixel_shader;
  auto vertex_data = read_file("assets/vertex.cso");
  auto pixel_data  = read_file("assets/pixel.cso");
  D3DCreateBlob(vertex_data.size(), &vertex_shader);
  D3DCreateBlob(pixel_data.size(), &pixel_shader);
  memcpy(vertex_shader->GetBufferPointer(), vertex_data.data(), vertex_data.size());
  memcpy(pixel_shader->GetBufferPointer(), pixel_data.data(), pixel_data.size());

  // create pipeline state
  D3D12_INPUT_ELEMENT_DESC layout[]
  {
    {  "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};
  pipeline_state_desc.InputLayout           = { layout, _countof(layout) };
  pipeline_state_desc.pRootSignature        = _root_signature.Get();
  pipeline_state_desc.VS                    = CD3DX12_SHADER_BYTECODE{ vertex_shader.Get() };
  pipeline_state_desc.PS                    = CD3DX12_SHADER_BYTECODE{ pixel_shader.Get() };
  pipeline_state_desc.RasterizerState       = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
  pipeline_state_desc.BlendState            = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
  pipeline_state_desc.SampleMask            = UINT_MAX;
  pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_desc.NumRenderTargets      = 1;
  pipeline_state_desc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count      = 1;
  err_if(_device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipeline_state)),
          "failed to create pipeline state");

  // create vertices
  Vertex vertices[]
  {
    { { 0.f,   .5f, 0.f }, { 1.f, 0.f, 0.f, 1.f } },
    { { .5f,  -.5f, 0.f }, { 0.f, 1.f, 0.f, 1.f } },
    { { -.5f, -.5f, 0.f }, { 0.f, 0.f, 1.f, 1.f } },
  };

  // FIXME: it's not to recommand use this, just like vulkan use device buffer to dynamic upload byte data every frame
  //        upload heap should only use to upload single data like static texture
  // create vertex buffer
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  auto resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
  err_if(_device->CreateCommittedResource(
    &heap_properties,
    D3D12_HEAP_FLAG_NONE,
    &resource_desc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&_vertex_buffer)),
    "failed to create vertex buffer");

  // copy data to vertex buffer
  uint8_t* p;
  auto range = CD3DX12_RANGE{};
  err_if(_vertex_buffer->Map(0, &range, reinterpret_cast<void**>(&p)),
          "failed to map pointer from vertex buffer");
  memcpy(p, vertices, sizeof(vertices));
  _vertex_buffer->Unmap(0, nullptr);

  // init vertex buffer view
  _vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
  _vertex_buffer_view.StrideInBytes  = sizeof(Vertex);
  _vertex_buffer_view.SizeInBytes    = sizeof(vertices);

  run();
}

void Renderer::destroy() noexcept
{
  _exit.store(true, std::memory_order_release);
  _render_acquire.release();
  _thread.join();
  wait_gpu_complete();
  CloseHandle(_fence_event);
}

void Renderer::create_window_resources(HWND handle) noexcept
{
  // create window resource
  auto window_resource = WindowResource{ handle, _factory.Get(), _command_queue.Get(), _device.Get() };

  // create command list
  if (!_command_list)
  {
    err_if(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, window_resource._command_allocators[_frame_index].Get(), _pipeline_state.Get(), IID_PPV_ARGS(&_command_list)),
            "failed to create command list");
    _command_list->Close();
  }

  // add window resources
  _window_resources.emplace_back(std::move(window_resource));
}

void Renderer::wait_gpu_complete() noexcept
{
  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), _fence_values[_frame_index]),
          "failed to signal fence");

  // wait until frame is finished
  err_if(_fence->SetEventOnCompletion(_fence_values[_frame_index], _fence_event),
          "failed to set event on completion");
  WaitForSingleObjectEx(_fence_event, INFINITE, FALSE);

  // update fence value
  ++_fence_values[_frame_index];
}

void Renderer::run() noexcept
{
  _thread = std::thread{[this] {
    auto beg = std::chrono::high_resolution_clock::now();
    uint32_t count{};
    while (true)
    {
      // wait render acquire
      _render_acquire.acquire();
      if (_exit.load(std::memory_order_acquire)) [[unlikely]]
        return;

      // process all message
      MessageQueue::instance()->pop_all();

      // render
      render();

      ++count;
      auto now = std::chrono::high_resolution_clock::now();
      auto dur = std::chrono::duration<float>(now - beg).count();
      if (dur >= 1.f)
      {
        info("[fps] {}", count / dur);
        count = 0;
        beg = now;
      }
    }
  }};
}

void Renderer::render() noexcept
{
  // render per window
  for (auto& window_resource : _window_resources)
    if (!window_resource._is_minimized) [[unlikely]]
      window_resource.render(_command_queue.Get(), _command_list.Get(), _pipeline_state.Get(), _root_signature.Get(), _vertex_buffer_view, _frame_index);

  // get current fence value
  auto current_fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), current_fence_value),
          "failed to signal fence");
      
  // move to next frame
  _frame_index = ++_frame_index % Frame_Count;

  // wait if next fence not ready
  if (_fence->GetCompletedValue() < _fence_values[_frame_index])
  {
    err_if(_fence->SetEventOnCompletion(_fence_values[_frame_index], _fence_event),
            "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, FALSE);
  }

  // update fence value
  _fence_values[_frame_index] = current_fence_value + 1;
}

auto Renderer::add_closed_window_resources(HWND handle) noexcept -> std::function<bool()>
{
  // find closed window resource
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource._window.handle();
  });
  err_if(it == _window_resources.end(), "failed to find closed window");

  // add to old resource, wait gpu finish using then destroy
  auto func = [window_resource = *it, this, last_fence_value = _fence_values[_frame_index]]
  {
    auto fence_value = _fence->GetCompletedValue();
    err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    return fence_value >= last_fence_value;
  };

  // remove old one
  _window_resources.erase(it);

  // destroy window
  vn::WindowManager::instance()->destroy_window(handle);

  return func;
}

void Renderer::set_window_minimized(HWND handle) noexcept
{
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource._window.handle();
  });
  if (it == _window_resources.end()) return;
  it->_is_minimized = true;
}

void Renderer::window_resize(HWND handle) noexcept
{
  // create new window resource and add old resource wait destroy, so need return func
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource._window.handle();
  });
  if (it == _window_resources.end()) return;

  // resize window resource
  it->resize(_device.Get());
}

}}