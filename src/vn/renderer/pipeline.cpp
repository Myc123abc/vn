#include "pipeline.hpp"
#include "error_handling.hpp"
#include "core.hpp"

#include <dxcapi.h>
#include <directx/d3dx12.h>

#include <utf8.h>

#include <span>

using namespace Microsoft::WRL;
using namespace vn;
using namespace vn::renderer;

namespace
{

#ifdef _WIN32
auto to_wstring(std::string_view str) noexcept -> std::wstring
{
  auto u16str = utf8::utf8to16(str);
  return { reinterpret_cast<wchar_t*>(u16str.data()), u16str.size() };
}
#endif

auto compile_shader(IDxcCompiler3* compiler, IDxcUtils* utils, DxcBuffer& buffer, std::string_view main, std::string_view profile, std::span<LPCWSTR> args = {}) noexcept
{
  auto dxc_args = ComPtr<IDxcCompilerArgs>{};
  err_if(utils->BuildArguments(nullptr, to_wstring(main).data(), to_wstring(profile).data(), args.data(), args.size(), nullptr, 0, dxc_args.GetAddressOf()),
              "failed to create dxc args");

  auto include = ComPtr<IDxcIncludeHandler>{};
  err_if(utils->CreateDefaultIncludeHandler(&include), "failed to create default include handler in dxc");

  auto result = ComPtr<IDxcResult>{};
  err_if(compiler->Compile(&buffer, dxc_args->GetArguments(), dxc_args->GetCount(), include.Get(), IID_PPV_ARGS(&result)),
              "failed to compile {}", main);

  auto hr = HRESULT{};
  result->GetStatus(&hr);
  if (FAILED(hr))
  {
    auto msg = ComPtr<IDxcBlobUtf8>{};
    err_if(result->GetOutput(DXC_OUT_ERRORS,  IID_PPV_ARGS(&msg), nullptr), "failed to get error ouput of dxc");
    err_if(true, "failed to compile {}\n{}", main, msg->GetStringPointer());
  }

  auto cso = ComPtr<IDxcBlob>{};
  err_if(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&cso), nullptr), "failed to get compile result");

  return cso;
}

auto read_file(std::string_view path) noexcept -> std::string
{
  FILE* file{};
  fopen_s(&file, path.data(), "rb");
  err_if(!file, "failed to open file {}", path);

  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  rewind(file);

  auto data = std::string{};
  data.resize(size);
  fread(data.data(), 1, size, file);
  fclose(file);

  return data;
}

auto compile_vert_pixel(std::string_view path, std::string_view vert_main, std::string_view pixel_main, std::span<LPCWSTR> args) noexcept -> std::pair<ComPtr<IDxcBlob>, ComPtr<IDxcBlob>>
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");

  return { compile_shader(compiler.Get(), utils.Get(), buffer, vert_main, "vs_6_0", args), compile_shader(compiler.Get(), utils.Get(), buffer, pixel_main, "ps_6_0", args) };
}

auto compile_comp(std::string_view path, std::string_view comp_main, std::span<LPCWSTR> args) noexcept
{
  auto compiler = ComPtr<IDxcCompiler3>{};
  err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
              "failed to create dxc compiler");

  DxcBuffer buffer{};
  auto data       = read_file(path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto utils = ComPtr<IDxcUtils>{};
  err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
              "failed to create dxc utils");
  return compile_shader(compiler.Get(), utils.Get(), buffer, comp_main, "cs_6_0", args);
}

auto get_shader_visibility(ShaderType type) noexcept
{
  using enum ShaderType;
  switch (type)
  {
  case all:   return D3D12_SHADER_VISIBILITY_ALL;
  case pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
  }
}

auto get_descriptor_range_type(DescriptorType type) noexcept
{
  using enum DescriptorType;
  switch (type)
  {
  case constants:
    err_if(true, "constants should not have descriptor range!");
    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

  case srv:       return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  case uav:       return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  }
}

auto get_descriptor_range_flag(DescriptorFlag flag) noexcept
{
  if (flag == DescriptorFlag::static_data)
    return D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
  return D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
}

auto get_dxgi_format(DXGIFormat format) noexcept
{
  using enum DXGIFormat;
  switch (format)
  {
  case rgb32_float: return DXGI_FORMAT_R32G32B32_FLOAT;
  case rg32_float:  return DXGI_FORMAT_R32G32_FLOAT;
  case r32_uint:    return DXGI_FORMAT_R32_UINT;
  case bgra8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
  }
}

auto get_dxgi_format_offset(DXGIFormat format) noexcept
{
  using enum DXGIFormat;
  switch (format)
  {
  case rgb32_float: return 12;
  case rg32_float:  return 8;
  case r32_uint:    return 4;
  case bgra8_unorm: return 4;
  }
}

}

namespace vn { namespace renderer {

void Pipeline::init(PipelineConfig const& cfg) noexcept
{
  auto core = Core::instance();

  // create root signature
  auto root_params = std::vector<CD3DX12_ROOT_PARAMETER1>(cfg.descriptor_infos.size());
  auto ranges      = std::vector<CD3DX12_DESCRIPTOR_RANGE1>(root_params.size());
  for (auto i = 0; i < cfg.descriptor_infos.size(); ++i)
  {
    auto const& info = cfg.descriptor_infos[i];
      
    using enum DescriptorType;
    switch (info.type)
    {
    case constants:
      root_params[i].InitAsConstants(info.num, info.shader_register, info.shader_register_space, get_shader_visibility(info.shader_type));
      break;
      
    case srv:
    case uav:
      ranges[i].Init(get_descriptor_range_type(info.type), info.num, info.shader_register, info.shader_register_space, get_descriptor_range_flag(info.flags));
      root_params[i].InitAsDescriptorTable(1, &ranges[i], get_shader_visibility(info.shader_type));
      break;
    }
  }

  auto sampler_desc = D3D12_STATIC_SAMPLER_DESC{};
  sampler_desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MaxLOD           = D3D12_FLOAT32_MAX;
  sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 

  auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
  if (cfg.sampler)
    signature_desc.Init_1_1(root_params.size(), root_params.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  else
    signature_desc.Init_1_1(root_params.size(), root_params.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  auto signature = ComPtr<ID3DBlob>{};
  auto error     = ComPtr<ID3DBlob>{};
  err_if(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(core->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
          "failed to create root signature");

  // create shaders
  auto include = std::wstring{ L"-I" };
  if (!cfg.include.empty())
    include += to_wstring(cfg.include);

  auto compiler_args = std::vector<LPCWSTR>
  {
#ifndef NDEBUG
    L"-Zi",
    L"-Qembed_debug",
    L"-Od",
#endif
    include.c_str()
  };
  auto [vertex_shader, pixel_shader] = compile_vert_pixel(cfg.shader, cfg.vs, cfg.ps, compiler_args);
  
  // create pipeline state
  auto layouts = std::vector<D3D12_INPUT_ELEMENT_DESC>{};
  layouts.reserve(cfg.vertex_params.size());
  auto offset = uint32_t{};
  for (auto const& info : cfg.vertex_params)
  {
    layouts.emplace_back(info.name.c_str(), 0, get_dxgi_format(info.format), 0, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0);
    offset += get_dxgi_format_offset(info.format);
  }
  
  // use pipeline state stream
  struct PipelienStateStream
  {
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        root_signature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          input_layout;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    primitive_topology;
    CD3DX12_PIPELINE_STATE_STREAM_VS                    vs;
    CD3DX12_PIPELINE_STATE_STREAM_PS                    ps;
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            blend_desc;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS render_target_formats;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1        depth_stencil;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  depth_stencil_format;
  } ps_stream;
  
  auto render_target_formats = D3D12_RT_FORMAT_ARRAY{};
  render_target_formats.NumRenderTargets = 1;
  render_target_formats.RTFormats[0]     = get_dxgi_format(cfg.rtv_format);
  
  ps_stream.root_signature        = _root_signature.Get();
  ps_stream.input_layout          = { layouts.data(), static_cast<uint32_t>(layouts.size()) };
  ps_stream.primitive_topology    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  ps_stream.vs                    = CD3DX12_SHADER_BYTECODE{ vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
  ps_stream.ps                    = CD3DX12_SHADER_BYTECODE{ pixel_shader->GetBufferPointer(),  pixel_shader->GetBufferSize()  };
  if (cfg.use_depth_test)
    ps_stream.depth_stencil_format = DXGI_FORMAT_D32_FLOAT;
  ps_stream.render_target_formats = render_target_formats;
    
  auto size = sizeof(ps_stream);
  if (!cfg.use_depth_test)
    size -= sizeof(CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1) + sizeof(CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT);
  auto pipeline_state_stream_desc = D3D12_PIPELINE_STATE_STREAM_DESC{ size, &ps_stream };
  
  // check feature support
  auto options = D3D12_FEATURE_DATA_D3D12_OPTIONS2{};
  err_if(core->device()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options, sizeof(options)),
          "failed to get feature options");
  
  if (cfg.use_depth_test)
  {
    auto depth_stencil_desc = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
    depth_stencil_desc.DepthEnable = false;
    err_if(!options.DepthBoundsTestSupported, "unsupport depth bounds test");
    depth_stencil_desc.DepthBoundsTestEnable = true;
    ps_stream.depth_stencil = depth_stencil_desc;
  }
  
  auto  blend_state = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  auto& rt          = blend_state.RenderTarget[0];
  rt.BlendEnable           = cfg.use_blend;
  rt.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
  rt.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
  rt.BlendOp               = D3D12_BLEND_OP_ADD;
  rt.SrcBlendAlpha         = D3D12_BLEND_ONE;
  rt.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
  rt.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
  rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  ps_stream.blend_desc = blend_state;
  
  err_if(core->device()->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&_pipeline_state)),
          "failed to create pipeline state");
}

void Pipeline::bind(ID3D12GraphicsCommandList1* cmd) const noexcept
{
  cmd->SetPipelineState(_pipeline_state.Get());
  cmd->SetGraphicsRootSignature(_root_signature.Get());
}

}}
