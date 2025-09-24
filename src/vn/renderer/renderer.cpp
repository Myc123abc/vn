#include "renderer.hpp"
#include "message_queue.hpp"
#include "../util.hpp"
#include "../window/window_manager.hpp"
#include "core.hpp"

#include <d3dcompiler.h>
#include <d3d11.h>

#include <algorithm>
#include <chrono>
#include <array>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  Core::instance()->init();

  capture_backdrop();

  init_pipeline_resources();
  init_blur_pipeline();

  run();
}

void Renderer::init_blur_pipeline() noexcept
{
  auto core = Core::instance();

  // set root parameters
  auto root_parameters = std::array<CD3DX12_ROOT_PARAMETER1, 1>{};
  auto des_ranges      = std::array<CD3DX12_DESCRIPTOR_RANGE1, 2>{};
  des_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  des_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
  root_parameters[0].InitAsDescriptorTable(2, des_ranges.data());

  // set root signature desc
  auto root_signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
  root_signature_desc.Init_1_1(root_parameters.size(), root_parameters.data());

  // serialize and create root signature
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  err_if(D3DX12SerializeVersionedRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(core->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_blur_root_signature)),
          "failed to create root signature");

  // create shader
  ComPtr<ID3DBlob> blur_shader;
  auto blur_shader_data = read_file("assets/blur.cso");
  D3DCreateBlob(blur_shader_data.size(), &blur_shader);
  memcpy(blur_shader->GetBufferPointer(), blur_shader_data.data(), blur_shader_data.size());

  // create pipeline state
  auto pipeline_state_desc = D3D12_COMPUTE_PIPELINE_STATE_DESC{};
  pipeline_state_desc.pRootSignature = _blur_root_signature.Get();
  pipeline_state_desc.CS = { blur_shader->GetBufferPointer(), blur_shader->GetBufferSize() };
  core->device()->CreateComputePipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_blur_pipeline_state));
}

void Renderer::init_pipeline_resources() noexcept
{
  auto core = Core::instance();

  // init frame buffer
  _frame_buffer.init(1024);

  // create root signature
  auto root_parameters = std::array<CD3DX12_ROOT_PARAMETER1, 1>{};
  root_parameters[0].InitAsConstants(sizeof(Constants), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  //auto des_range = CD3DX12_DESCRIPTOR_RANGE1{};
  //des_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  //root_parameters[1].InitAsDescriptorTable(1, &des_range, D3D12_SHADER_VISIBILITY_PIXEL);
  
  //auto sampler_desc = D3D12_STATIC_SAMPLER_DESC{};
  //sampler_desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  //sampler_desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  //sampler_desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  //sampler_desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  //sampler_desc.MaxLOD           = D3D12_FLOAT32_MAX;
  //sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  //auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{ root_parameters.size(), root_parameters.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
  auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{ root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  err_if(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(core->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
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
  pipeline_state_desc.RTVFormats[0]         = DXGI_FORMAT_B8G8R8A8_UNORM;
  pipeline_state_desc.SampleDesc.Count      = 1;
  err_if(core->device()->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipeline_state)),
          "failed to create pipeline state");
#if 0
  // create texture
  auto screen_size = WindowManager::screen_size();
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.MipLevels        = 1;
  texture_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
  texture_desc.Width            = screen_size.x;
  texture_desc.Height           = screen_size.y;
  texture_desc.DepthOrArraySize = 1;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };

  err_if(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_backdrop_image)),
          "failed to create committed texture");
  // create upload heap
  ComPtr<ID3D12Resource> upload_heap;
  heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD };
  resource_desc   = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(_backdrop_image.Get(), 0, 1));
  err_if(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap)),
          "failed to create upload heap");
  upload_heap->SetName(L"upload heap");

  // upload
  auto data = std::vector<std::byte>(texture_desc.Width * texture_desc.Height * 4);
  D3D12_SUBRESOURCE_DATA texture_data{};
  texture_data.pData      = data.data();
  texture_data.RowPitch   = texture_desc.Width * 4;
  texture_data.SlicePitch = texture_data.RowPitch + texture_desc.Height;
  UpdateSubresources(_command_list.Get(), _backdrop_image.Get(), upload_heap.Get(), 0, 0, 1, &texture_data);
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_backdrop_image.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _command_list->ResourceBarrier(1, &barrier);
  
  // commit command list
  _command_list->Close();
  ID3D12CommandList* cmds[] = { _command_list.Get() };
  _command_queue->ExecuteCommandLists(_countof(cmds), cmds);
  wait_gpu_complete();
#endif
}

void Renderer::destroy() noexcept
{
  _exit.store(true, std::memory_order_release);
  _render_acquire.release();
  _thread.join();
  Core::instance()->wait_gpu_complete();
  Core::instance()->destroy();
}

void Renderer::create_window_resources(HWND handle) noexcept
{
  _window_resources.emplace_back(handle);
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

      sort_windows_by_z_order();

      update();
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

void Renderer::update() noexcept
{
  if (_window_start_moving)
  {
    std::vector<uint16_t> indices;
    for (auto& window_resource : _window_resources)
    {
      if (!window_resource._is_minimized) [[unlikely]]
      {
        auto pos  = window_resource._window.position();
        auto size = window_resource._window.size();
        std::vector<Vertex> vertices
        {
          { pos + glm::vec<2, int32_t>{ size.x / 2, 0      }, {}, {1, 0, 0, 1} },
          { pos + glm::vec<2, int32_t>{ size.x,     size.y }, {}, {0, 1, 0, 1} },
          { pos + glm::vec<2, int32_t>{ 0,          size.y }, {}, {0, 0, 1, 1} },
        };
        auto idx = indices.size();
        indices.append_range(std::vector<uint16_t>
        {
          static_cast<uint16_t>(idx),
          static_cast<uint16_t>(idx + 1),
          static_cast<uint16_t>(idx + 2),
        });
        _fullscreen_window->push_vertices(vertices);
        _fullscreen_window->push_indices(indices);
      }
    }
  }
  else
  {
    for (auto& window_resource : _window_resources)
    {
      if (!window_resource._is_minimized) [[unlikely]]
      {
        auto size = window_resource._window.size();
        std::vector<Vertex> vertices
        {
          { { size.x / 2, 0      }, {}, {1, 0, 0, 1} },
          { { size.x,     size.y }, {}, {0, 1, 0, 1} },
          { { 0,          size.y }, {}, {0, 0, 1, 1} },
        };
        std::vector<uint16_t> indices
        {
          0, 1, 2,
        };
        window_resource.push_vertices(vertices);
        window_resource.push_indices(indices);
      }
    }
  }
}

void Renderer::render() noexcept
{
  // clear frame buffer
  _frame_buffer.clear();

  if (_window_start_moving)
  {
    _fullscreen_window->render();
  }
  else
  {
    // render per window
    for (auto& window_resource : _window_resources)
    {
      if (!window_resource._is_minimized) [[unlikely]]
      {
        window_resource.render();
      }
    }
  }
  
  Core::instance()->move_to_next_frame();
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
  auto func = [window_resource = *it, this, last_fence_value = Core::instance()->get_last_fence_value() ]
  {
    auto fence_value = Core::instance()->fence()->GetCompletedValue();
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

// TODO: resize and move to use a fake fullscreen transparent window to replace
void Renderer::window_resize(HWND handle) noexcept
{
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource._window.handle();
  });
  if (it == _window_resources.end()) return;
  it->resize();
}

void Renderer::capture_backdrop() noexcept
{
  // init d3d11 device
  ComPtr<ID3D11Device> device;  
  err_if(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr),
          "failed to create d3d11 device");

  // get dxgi device
  ComPtr<IDXGIDevice> dxgI_device;
  err_if(device.As(&dxgI_device), "failed to get dxgi device");

  // get adapter
  ComPtr<IDXGIAdapter> adapter;
  err_if(dxgI_device->GetParent(IID_PPV_ARGS(&adapter)), "failed to get adapter from dxgi device");

  // TODO: can get multiple monitors here
  // get dxgi ouput
  ComPtr<IDXGIOutput> output;
  err_if(adapter->EnumOutputs(0, &output), "failed to get dxgi output");

  // get dxgi output1
  ComPtr<IDXGIOutput1> output1;
  err_if(output.As(&output1), "failed to get dxgi output1");

  // create desktop duplicaiton
  err_if(output1->DuplicateOutput(device.Get(), &_desk_dup), "failed to get desktop duplication");

  // read first frame of d3d11 texture
  ComPtr<IDXGIResource>   desktop_resource;
  DXGI_OUTDUPL_FRAME_INFO frame_info{};
  ComPtr<ID3D11Texture2D> d3d11_texture;
  err_if(_desk_dup->AcquireNextFrame(0, &frame_info, &desktop_resource), "failed to read first frame dropback");
  err_if(_desk_dup->ReleaseFrame(), "failed to release capture backdrop");
  err_if(desktop_resource.As(&d3d11_texture), "failed to get d3d11 texture");

  // convert to dxgi resource
  ComPtr<IDXGIResource1> texture_dxgi_resource;
  err_if(d3d11_texture.As(&texture_dxgi_resource), "failed to conver to dxgi resource");

  // share handle
  HANDLE handle{};
  err_if(texture_dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &handle),
          "failed to create shared handle");

  // get description of texture
  D3D11_TEXTURE2D_DESC desc{};
  d3d11_texture->GetDesc(&desc);

  // share with d3d12 resource
  _desktop_image.init(handle, desc.Width, desc.Height);

  CloseHandle(handle);
}

void Renderer::sort_windows_by_z_order() noexcept
{
  std::ranges::sort(_window_resources, [](auto const& x, auto const& y)
  {
    auto handle = x._window.handle();
    while (handle)
    {
      handle = GetWindow(handle, GW_HWNDNEXT);
      if (handle == y._window.handle())
        return true;
    }
    return false;
  });
}

void Renderer::init_fullscreen_window(HWND handle) noexcept
{
  _fullscreen_window = std::make_unique<WindowResource>(handle);
}

void Renderer::window_move_start(HWND handle) noexcept
{
  _window_start_moving = true;
  auto it = std::ranges::find_if(_window_resources, [handle] (auto const& window_resource)
  {
    return handle == window_resource._window.handle();
  });
  err_if(it == _window_resources.end(), "faild to find moved window");
  _moved_window = &*it;
  //std::ranges::for_each(_window_resources, [](auto const& window) { ShowWindow(window._window.handle(), SW_HIDE); });
}

void Renderer::window_move_end() noexcept
{
  _window_start_moving = false;
  _moved_window        = nullptr;
}

}}