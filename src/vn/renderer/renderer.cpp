#include "renderer.hpp"

#include <dxcapi.h>
#include <d3d11.h>

#include <utf8.h>

#include <chrono>
#include <array>

using namespace Microsoft::WRL;

namespace
{

#ifdef _WIN32
auto to_wstring(std::string_view str) noexcept -> std::wstring
{
  auto u16str = utf8::utf8to16(str);
  return { reinterpret_cast<wchar_t*>(u16str.data()), u16str.size() };
}
#endif

auto compile_shader(IDxcCompiler3* compiler, IDxcUtils* utils, DxcBuffer& buffer, std::string_view main, std::string_view profile) noexcept
{
  auto args = ComPtr<IDxcCompilerArgs>{};
  vn::err_if(utils->BuildArguments(nullptr, to_wstring(main).data(), to_wstring(profile).data(), nullptr, 0, nullptr, 0, args.GetAddressOf()),
              "failed to create dxc args");
  
  auto result = ComPtr<IDxcResult>{};
  vn::err_if(compiler->Compile(&buffer, args->GetArguments(), args->GetCount(), nullptr, IID_PPV_ARGS(&result)),
              "failed to compile {}", main);
  
  auto hr = HRESULT{};
  result->GetStatus(&hr);
  if (FAILED(hr))
  {
    auto msg = ComPtr<IDxcBlobUtf8>{};
    vn::err_if(result->GetOutput(DXC_OUT_ERRORS,  IID_PPV_ARGS(&msg), nullptr), "failed to get error ouput of dxc");
    vn::err_if(true, "failed to compile {}\n{}", main, msg->GetStringPointer());
  }

  auto cso = ComPtr<IDxcBlob>{};
  vn::err_if(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&cso), nullptr), "failed to get compile result");

  return cso;
}

auto compile_vert_pixel(std::string_view path, std::string_view vert_main, std::string_view pixel_main) noexcept -> std::pair<ComPtr<IDxcBlob>, ComPtr<IDxcBlob>>
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = vn::read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");
  return { compile_shader(compiler.Get(), utils.Get(), buffer, vert_main, "vs_6_0"), compile_shader(compiler.Get(), utils.Get(), buffer, pixel_main, "ps_6_0") };
}

auto compile_comp(std::string_view path, std::string_view comp_main) noexcept
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = vn::read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  vn::err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");
  return compile_shader(compiler.Get(), utils.Get(), buffer, comp_main, "cs_6_0");
}

}

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  _thread = std::thread{[this]
  {
    Core::instance()->init();

    create_swapchain_resources();
    create_frame_resources();

    capture_backdrop();

    create_pipeline_resources();
    create_blur_pipeline();

    run();
  }};
}

void Renderer::create_swapchain_resources() noexcept
{
  auto core = Core::instance();
  auto ws   = WindowSystem::instance();

  auto size = ws->screen_size();

  // set viewport and scissor
  _viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(size.x), static_cast<float>(size.y) };
  _scissor  = CD3DX12_RECT{ 0, 0, static_cast<LONG>(size.x), static_cast<LONG>(size.y) };

  // create swapchain
  ComPtr<IDXGISwapChain1> swapchain;
  DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = size.x;
  swapchain_desc.Height           = size.y;
  swapchain_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  err_if(core->factory()->CreateSwapChainForHwnd(core->command_queue(), ws->handle(), &swapchain_desc, nullptr, nullptr, &swapchain),
        "failed to create swapchain for composition");
  err_if(swapchain.As(&_swapchain), "failed to get swapchain4");

  // create descriptor heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = Frame_Count;
  rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  err_if(core->device()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&_rtv_heap)),
    "failed to create render target view descriptor heap");
}

void Renderer::create_frame_resources() noexcept
{
  auto core = Core::instance();

  auto size = WindowSystem::instance()->screen_size();

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle{ _rtv_heap->GetCPUDescriptorHandleForHeapStart() };
  for (auto i = 0; i < Frame_Count; ++i)
  {
    auto& frame = _frames[i];

    // get swapchain image
    _swapchain_images[i].init(_swapchain.Get(), i)
                        .create_descriptor(rtv_handle);
    // offset next swapchain image
    rtv_handle.Offset(1, RTV_Size);

    // get command allocator
    err_if(core->device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.command_allocator)),
        "failed to create command allocator");

    // create backdrop image and blur backdrop image
    frame.backdrop_image.init(size.x, size.y);
    frame.blur_backdrop_image.init(size.x, size.y);

    // create descriptors
    frame.heap.init(2);
    frame.backdrop_image.create_descriptor(frame.heap.pop_handle());
    frame.blur_backdrop_image.create_descriptor(frame.heap.pop_handle());
  }
}

void Renderer::create_blur_pipeline() noexcept
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
  auto shader = compile_comp("assets/blur.hlsl", "compute_main");

  // create pipeline state
  auto pipeline_state_desc = D3D12_COMPUTE_PIPELINE_STATE_DESC{};
  pipeline_state_desc.pRootSignature = _blur_root_signature.Get();
  pipeline_state_desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
  core->device()->CreateComputePipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_blur_pipeline_state));
}

void Renderer::create_pipeline_resources() noexcept
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
  auto [vertex_shader, pixel_shader] = compile_vert_pixel("assets/shader.hlsl", "vs", "ps");

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
  pipeline_state_desc.VS                    = CD3DX12_SHADER_BYTECODE{ vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
  pipeline_state_desc.PS                    = CD3DX12_SHADER_BYTECODE{ pixel_shader->GetBufferPointer(),  pixel_shader->GetBufferSize()  };
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

void Renderer::run() noexcept
{
  auto beg = std::chrono::high_resolution_clock::now();
  uint32_t count{};
  while (true)
  {
    // wait render acquire
    _render_acquire.acquire();
    if (_exit.load(std::memory_order_acquire)) [[unlikely]]
      return;

    // destruct old resources
    for (auto it = _old_resource_destructor.begin(); it != _old_resource_destructor.end();)
      (*it)() ? it = _old_resource_destructor.erase(it) : ++it;

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
}

void Renderer::update() noexcept
{
  _window_resources = WindowSystem::instance()->updated_data();

  for (auto const& window : _window_resources->windows)
  {
    std::vector<Vertex> vertices
    {
      { { window.x + window.width / 2, window.y                 }, {}, {1, 0, 0, 1} },
      { { window.x + window.width,     window.y + window.height }, {}, {0, 1, 0, 1} },
      { { window.x,                    window.y + window.height }, {}, {0, 0, 1, 1} },
    };
    _vertices.append_range(vertices);

    _indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(_idx_beg + 0),
      static_cast<uint16_t>(_idx_beg + 1),
      static_cast<uint16_t>(_idx_beg + 2),
    });
    _idx_beg += 3;
  }
}

void Renderer::render() noexcept
{
  auto  core  = Core::instance();
  auto  cmd   = core->cmd();
  auto& frame = _frames[core->frame_index()];

  // reset command allocator and command list
  err_if(frame.command_allocator->Reset() == E_FAIL, "failed to reset command allocator");
  err_if(cmd->Reset(frame.command_allocator.Get(), nullptr), "failed to reset command list");

  // get current swapchain
  auto& swapchain_image = _swapchain_images[_swapchain->GetCurrentBackBufferIndex()];

  // copy backdrop image
  for (auto const& window : _window_resources->windows)
  {
    auto rect = window.rect();
    copy(cmd, _desktop_image, rect.left, rect.top, rect.right, rect.bottom, frame.backdrop_image, rect.left, rect.top);
    copy(cmd, frame.backdrop_image, swapchain_image);
  }

  // set pipeline state
  cmd->SetPipelineState(_pipeline_state.Get());

  // set root signature
  cmd->SetGraphicsRootSignature(_root_signature.Get());

  // set viewport and scissor rectangle
  cmd->RSSetViewports(1, &_viewport);
  cmd->RSSetScissorRects(1, &_scissor);

  // convert render target view from present type to render target type
  swapchain_image.set_state<ImageState::render_target>(cmd);

  // set render target view
  auto rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtv_heap->GetCPUDescriptorHandleForHeapStart(), _swapchain->GetCurrentBackBufferIndex(), RTV_Size);
  cmd->OMSetRenderTargets(1, &rtv_handle, false, nullptr);

  // clear color
  //float constexpr clear_color[4]{};
  //cmd->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

  // set primitive topology
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // clear frame buffer
  _frame_buffer.clear();

  // upload vertices and indices
  _frame_buffer.upload(cmd, _vertices, _indices);

  // set constant
  auto constants = Constants{};
  constants.window_extent = WindowSystem::instance()->screen_size();
  cmd->SetGraphicsRoot32BitConstants(0, sizeof(Constants), &constants, 0);

  // draw
  cmd->DrawIndexedInstanced(_indices.size(), 1, 0, 0, 0);

  // clear vertices and indices
  _vertices.clear();
  _indices.clear();
  _idx_beg = {};

  // record finish, change render target view type to present
  swapchain_image.set_state<ImageState::present>(cmd);

  // close command list
  err_if(cmd->Close(), "failed to close command list");

  // execute command list
  ID3D12CommandList* cmds[] = { cmd };
  core->command_queue()->ExecuteCommandLists(_countof(cmds), cmds);

  // present swapchain
  err_if(_swapchain->Present(1, 0), "failed to present swapchain");

  Core::instance()->move_to_next_frame();
}

// TODO: in sleep screen then open, will get nothing from desktop duplication
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

}}