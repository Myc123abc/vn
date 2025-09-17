#include "renderer.hpp"
#include "message_queue.hpp"
#include "../util.hpp"
#include "../window/window_manager.hpp"
#include "desktop_duplication.hpp"

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
  _fence_event = CreateEvent(nullptr, false, false, nullptr);
  err_if(!_fence_event, "failed to create fence event");
  for (auto& fence_value : _fence_values)
    fence_value = 1;

  // create command allocator and list
  err_if(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocator)),
          "failed to create command allocator");
  err_if(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), _pipeline_state.Get(), IID_PPV_ARGS(&_command_list)),
          "failed to create command list");

  init_pipeline_resources();

  run();
}

auto constexpr TextureWidth     = 256;
auto constexpr TextureHeight    = 256;
auto constexpr TexturePixelSize = 4;

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> GenerateTextureData()
{
    const UINT rowPitch = TextureWidth * TexturePixelSize;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
    const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * TextureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}

void Renderer::init_pipeline_resources() noexcept
{
  // create descriptor heaps
  auto srv_heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{};
  srv_heap_desc.NumDescriptors = 1;
  srv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srv_heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  err_if(_device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&_srv_heap)), "failed to create srv heap");

  // create root signature
  auto root_parameters = std::array<CD3DX12_ROOT_PARAMETER1, 2>{};
  root_parameters[0].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  auto des_range = CD3DX12_DESCRIPTOR_RANGE1{};
  des_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  root_parameters[1].InitAsDescriptorTable(1, &des_range, D3D12_SHADER_VISIBILITY_PIXEL);
  
  auto sampler_desc = D3D12_STATIC_SAMPLER_DESC{};
  sampler_desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MaxLOD           = D3D12_FLOAT32_MAX;
  sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{ root_parameters.size(), root_parameters.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  err_if(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
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
    {  "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    { {  0.f,  .5f }, { 0.5f,  .0f }, { 1.f, 0.f, 0.f, 1.f } },
    { {  .5f, -.5f }, { 1.0f, 1.0f }, { 0.f, 1.f, 0.f, 1.f } },
    { { -.5f, -.5f }, {  .0f, 1.0f }, { 0.f, 0.f, 1.f, 1.f } },
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

  // create texture
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.MipLevels        = 1;
  texture_desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
  texture_desc.Width            = TextureWidth;
  texture_desc.Height           = TextureHeight;
  texture_desc.DepthOrArraySize = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
  err_if(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_texture)),
          "failed to create committed texture");

  // create upload heap
  ComPtr<ID3D12Resource> upload_heap;
  heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(_texture.Get(), 0, 1));
  err_if(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap)),
          "failed to create upload heap");
  upload_heap->SetName(L"upload heap");

  // upload
  auto data = GenerateTextureData();
  D3D12_SUBRESOURCE_DATA texture_data{};
  texture_data.pData      = data.data();
  texture_data.RowPitch   = TextureWidth * TexturePixelSize;
  texture_data.SlicePitch = texture_data.RowPitch + TextureHeight;
  UpdateSubresources(_command_list.Get(), _texture.Get(), upload_heap.Get(), 0, 0, 1, &texture_data);
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _command_list->ResourceBarrier(1, &barrier);

  // create srv
  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format                  = texture_desc.Format;
  srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MipLevels     = 1;
  _device->CreateShaderResourceView(_texture.Get(), &srv_desc, _srv_heap->GetCPUDescriptorHandleForHeapStart());  

  // commit command list
  _command_list->Close();
  ID3D12CommandList* cmds[] = { _command_list.Get() };
  _command_queue->ExecuteCommandLists(_countof(cmds), cmds);
  wait_gpu_complete();
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
  _window_resources.emplace_back(handle);
}

void Renderer::wait_gpu_complete() noexcept
{
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value), "failed to signal fence");

  if (_fence->GetCompletedValue() < fence_value)
  {
    // wait until frame is finished
    err_if(_fence->SetEventOnCompletion(fence_value, _fence_event), "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // advance frame
  _frame_index = ++_frame_index % Frame_Count;
  // advance fence
  _fence_values[_frame_index] = fence_value + 1;
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
      window_resource.render();

  // get current fence value
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value),
          "failed to signal fence");
      
  // move to next frame
  _frame_index = ++_frame_index % Frame_Count;

  // wait if next fence not ready
  if (_fence->GetCompletedValue() < _fence_values[_frame_index])
  {
    err_if(_fence->SetEventOnCompletion(_fence_values[_frame_index], _fence_event),
            "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // update the next frame fence value
  _fence_values[_frame_index] = fence_value + 1;
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