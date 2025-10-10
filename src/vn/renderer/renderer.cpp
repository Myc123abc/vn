#include "renderer.hpp"
#include "core.hpp"
#include "../util.hpp"
#include "message_queue.hpp"

#include <dxcapi.h>

#include <utf8.h>

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
  Core::instance()->init();
  create_pipeline_resource();
  run();
}

void Renderer::destroy() noexcept
{
  Core::instance()->wait_gpu_complete();
  Core::instance()->destroy();
}

void Renderer::create_pipeline_resource() noexcept
{
  auto core = Core::instance();

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
    {  "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    {  "COLOR",    0, DXGI_FORMAT_R32_UINT,     0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
}

void Renderer::add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept
{
  _current_frame_render_finish_procs.emplace_back([func, last_fence_value = Core::instance()->get_last_fence_value()]()
  {
    auto fence_value = Core::instance()->fence()->GetCompletedValue();
    err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    auto render_complete = fence_value >= last_fence_value;
    if (render_complete) func();
    return render_complete;
  });
}

void Renderer::run() noexcept
{
  // process last render finish processes
  for (auto it = _current_frame_render_finish_procs.begin(); it != _current_frame_render_finish_procs.end();)
    (*it)() ? it = _current_frame_render_finish_procs.erase(it) : ++it;

  MessageQueue::instance()->process_messages();

  update();
  render();
}

void Renderer::update() noexcept
{
  float offsets[] = { 0, 1.f/2, 1};
  float offsets2[] = { 1, 1.f/2, 0};
  auto i = 0;
  for (auto& [k, v] : _window_resources)
  {
    auto& window         = v.window;
    auto& frame_resource = v.frame_resource;
    
    frame_resource.vertices.append_range(std::vector<Vertex>
    {
      { { window.width * offsets[i], 0             }, {}, 0x00ff00ff },
      { { window.width * offsets2[i],     window.height }, {}, 0x0000ffff },
      { { 0,                window.height * offsets2[i] }, {}, 0x00ffffff },
    });
    frame_resource.indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(frame_resource.idx_beg + 0),
      static_cast<uint16_t>(frame_resource.idx_beg + 1),
      static_cast<uint16_t>(frame_resource.idx_beg + 2),
    });
    frame_resource.idx_beg += 3;

    ++i;
  }
}

void Renderer::render() noexcept
{
  for (auto& [k, wr] : _window_resources) wr.render();
  Core::instance()->move_to_next_frame();
}

}}