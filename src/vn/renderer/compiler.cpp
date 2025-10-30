#include "compiler.hpp"
#include "error_handling.hpp"
#include "core.hpp"

#include <directx/d3dx12.h>

#include <utf8.h>

#include <vector>
#include <ranges>

using namespace vn;
using namespace Microsoft::WRL;

namespace
{

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

#ifdef _WIN32
auto to_wstring(std::string_view str) noexcept -> std::wstring
{
  auto u16str = utf8::utf8to16(str);
  return { reinterpret_cast<wchar_t*>(u16str.data()), u16str.size() };
}
#endif

// Map a reflected vertex input parameter description to an appropriate DXGI_FORMAT.
// Only 16-bit and 32-bit scalar component types are mappable for input layouts.
// 64-bit component types (double / 64-bit ints) are not supported by the Input Assembler; we return DXGI_FORMAT_UNKNOWN.
auto to_dxgi_format(D3D12_SIGNATURE_PARAMETER_DESC const& d) noexcept -> DXGI_FORMAT
{
  // Consider only the lowest four bits (x,y,z,w occupancy)
  BYTE mask = d.Mask & 0xF;
  if (mask == 0) return DXGI_FORMAT_UNKNOWN;

  // Count contiguous components (reflection should give 1,3,7,15 pattern)
  int count = 0;
  BYTE m = mask;
  while (m) { ++count; m >>= 1; }

  // Helper lambdas for mapping
  auto map32 = [count](DXGI_FORMAT r1, DXGI_FORMAT r2, DXGI_FORMAT r3, DXGI_FORMAT r4) -> DXGI_FORMAT
  {
    switch (count)
    {
    case 1: return r1;
    case 2: return r2;
    case 3: return r3;
    case 4: return r4;
    default: return DXGI_FORMAT_UNKNOWN;
    }
  };
  auto map16 = [count](DXGI_FORMAT r1, DXGI_FORMAT r2, DXGI_FORMAT /*r3*/, DXGI_FORMAT r4) -> DXGI_FORMAT
  {
    // No 3-component 16-bit formats; fall back to 4 only if explicitly 4, else UNKNOWN for 3.
    switch (count)
    {
    case 1: return r1;
    case 2: return r2;
    case 3: return DXGI_FORMAT_UNKNOWN;
    case 4: return r4;
    default: return DXGI_FORMAT_UNKNOWN;
    }
  };

  switch (d.ComponentType)
  {
  case D3D_REGISTER_COMPONENT_FLOAT32:
    return map32(DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT);
  case D3D_REGISTER_COMPONENT_UINT32:
    return map32(DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32A32_UINT);
  case D3D_REGISTER_COMPONENT_SINT32:
    return map32(DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32A32_SINT);
  case D3D_REGISTER_COMPONENT_FLOAT16:
    return map16(DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_FLOAT);
  case D3D_REGISTER_COMPONENT_UINT16:
    return map16(DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_UINT);
  case D3D_REGISTER_COMPONENT_SINT16:
    return map16(DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R16G16_SINT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_SINT);
  // 64-bit & others unsupported for vertex input layout
  case D3D_REGISTER_COMPONENT_FLOAT64:
  case D3D_REGISTER_COMPONENT_UINT64:
  case D3D_REGISTER_COMPONENT_SINT64:
  case D3D_REGISTER_COMPONENT_UNKNOWN:
  default:
    return DXGI_FORMAT_UNKNOWN;
  }
}

}

namespace vn { namespace renderer {

void Compiler::init() noexcept
{
  err_if(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_compiler)),
          "failed to create dxc compiler");
  err_if(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_utils)),
          "failed to create dxc utils");
  err_if(_utils->CreateDefaultIncludeHandler(&_include_handler),
          "failed to create default include handler in dxc");
}

auto Compiler::compile(std::string_view shader_path, std::string_view include, ShaderType type, std::string_view entry_point) noexcept -> std::pair<Microsoft::WRL::ComPtr<IDxcResult>, Microsoft::WRL::ComPtr<IDxcBlob>>
{
  auto buffer = DxcBuffer{};
  auto data   = read_file(shader_path);
  buffer.Ptr      = data.data();
  buffer.Size     = data.size();
  buffer.Encoding = DXC_CP_UTF8;

  auto profile = std::wstring{};
  if (type == ShaderType::vs) profile = L"vs_6_0";
  else if (type == ShaderType::ps) profile = L"ps_6_0";
  else if (type == ShaderType::cs) profile = L"cs_6_0";

  auto include_str = std::wstring{};
  if (!include.empty())
    include_str = std::wstring(L"-I") + to_wstring(include);
  auto args = std::vector<LPCWSTR>
  {
#ifndef NDEBUG
    L"-Zi",
    L"-Qembed_debug",
    L"-Od",
#endif
    include_str.c_str()
  };

  auto dxc_args = ComPtr<IDxcCompilerArgs>{};
  err_if(_utils->BuildArguments(nullptr, to_wstring(entry_point).data(), profile.data(), args.data(), args.size(), nullptr, 0, dxc_args.GetAddressOf()),
          "failed to create dxc args");

  auto result = ComPtr<IDxcResult>{};
  err_if(_compiler->Compile(&buffer, dxc_args->GetArguments(), dxc_args->GetCount(), _include_handler.Get(), IID_PPV_ARGS(&result)),
          "failed to compile {} of {}", entry_point, shader_path);

  auto hr = HRESULT{};
  result->GetStatus(&hr);
  if (FAILED(hr))
  {
    auto msg = ComPtr<IDxcBlobUtf8>{};
    err_if(result->GetOutput(DXC_OUT_ERRORS,  IID_PPV_ARGS(&msg), nullptr), "failed to get error ouput of dxc");
    err_if(true, "failed to compile {} of {}\n{}", entry_point, shader_path, msg->GetStringPointer());
  }

  auto cso = ComPtr<IDxcBlob>{};
  err_if(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&cso), nullptr), "failed to get compile result");

  return { result, cso };
}

auto Compiler::compile(std::string_view shader, std::string_view vertex_shader_entry_point, std::string_view pixel_shader_entry_point, std::string_view include) noexcept -> CompileResult
{
  // compile shaders
  auto [vs_res, vs_cso] = compile(shader, include, ShaderType::vs, vertex_shader_entry_point);
  auto [ps_res, ps_cso] = compile(shader, include, ShaderType::ps, pixel_shader_entry_point);

  // get reflection
  auto compile_result = CompileResult{};
  compile_result._vs_cso = vs_cso;
  compile_result._ps_cso = ps_cso;
  compile_result.vs      = { vs_cso->GetBufferPointer(), vs_cso->GetBufferSize() };
  compile_result.ps      = { ps_cso->GetBufferPointer(), ps_cso->GetBufferSize() };
  auto vs_reflection = compile_result.get_shader_reflection(vs_res.Get(), ShaderType::vs);
  auto ps_reflection = compile_result.get_shader_reflection(ps_res.Get(), ShaderType::ps);

  // get vertex input layout
  compile_result.get_vertex_input_layout(vs_reflection.Get());

  // create root signature
  compile_result.get_root_parameters(vs_reflection.Get());
  compile_result.get_root_parameters(ps_reflection.Get());

  auto sampler_desc = D3D12_STATIC_SAMPLER_DESC{};
  sampler_desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  sampler_desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MaxLOD           = D3D12_FLOAT32_MAX;
  sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; 

  auto signature_desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
  if (compile_result._has_sampler)
    signature_desc.Init_1_1(compile_result._root_params.size(), compile_result._root_params.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  else
    signature_desc.Init_1_1(compile_result._root_params.size(), compile_result._root_params.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  auto signature = ComPtr<ID3DBlob>{};
  auto error     = ComPtr<ID3DBlob>{};
  if (FAILED(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
  {
    auto msg = std::string{};
    msg.resize(error->GetBufferSize());
    memcpy(msg.data(), error->GetBufferPointer(), msg.size());
    err_if(true, "failed to serialize root signature.\n{}", msg);
  }
  //err_if(Core::instance()->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&compile_result.root_signature)),
  //        "failed to create root signature");

  return compile_result;
}

auto Compiler::CompileResult::get_shader_reflection(IDxcResult* result, ShaderType type) noexcept -> Microsoft::WRL::ComPtr<ID3D12ShaderReflection>
{
  // get reflection interface
  auto reflection = ComPtr<IDxcBlob>{};
  err_if(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection), nullptr), "failed to get reflection");

  auto buffer = DxcBuffer{};
  buffer.Ptr  = reflection->GetBufferPointer();
  buffer.Size = reflection->GetBufferSize();

  // get shader reflection information
  auto shader_reflection = ComPtr<ID3D12ShaderReflection>{};
  err_if(Compiler::instance()->_utils->CreateReflection(&buffer, IID_PPV_ARGS(&shader_reflection)), "failed to create shader reflection");

  return shader_reflection;
}

void Compiler::CompileResult::get_vertex_input_layout(ID3D12ShaderReflection* shader_reflection) noexcept
{
  auto desc = D3D12_SHADER_DESC{};
  shader_reflection->GetDesc(&desc);

  _input_element_descs.reserve(desc.InputParameters);
  _input_param_names.reserve(desc.InputParameters);
  for (auto i : std::views::iota(0u, desc.InputParameters))
  {
    auto param_desc = D3D12_SIGNATURE_PARAMETER_DESC{};
    shader_reflection->GetInputParameterDesc(i, &param_desc);
    _input_param_names.emplace_back(param_desc.SemanticName);
    _input_element_descs.emplace_back(D3D12_INPUT_ELEMENT_DESC
    {
      .SemanticName         = _input_param_names.back().c_str(),
      .SemanticIndex        = param_desc.SemanticIndex,
      .Format               = to_dxgi_format(param_desc),
      .InputSlot            = 0u,
      .AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0u,
    });
  }

  input_layout_desc = D3D12_INPUT_LAYOUT_DESC{ _input_element_descs.data(), static_cast<uint32_t>(_input_element_descs.size()) };
}

void Compiler::CompileResult::get_root_parameters(ID3D12ShaderReflection* shader_reflection) noexcept
{
  auto desc = D3D12_SHADER_DESC{};
  shader_reflection->GetDesc(&desc);

  // get bound resources
  _root_params.reserve(desc.BoundResources);
  for (auto i : std::views::iota(0u, desc.BoundResources))
  {
    auto resource_desc = D3D12_SHADER_INPUT_BIND_DESC{};
    err_if(shader_reflection->GetResourceBindingDesc(i, &resource_desc), "failed to get bound resource");

    if (!_resource_keys.emplace(resource_desc.Type, resource_desc.BindPoint, resource_desc.Space).second)
      continue;

    switch (resource_desc.Type)
    {
    case D3D_SIT_CBUFFER:
    {
      resource_indexs[resource_desc.Name] = _root_params.size();

      auto constant_buffer = shader_reflection->GetConstantBufferByIndex(i);
      auto buffer_desc = D3D12_SHADER_BUFFER_DESC{};
      err_if(constant_buffer->GetDesc(&buffer_desc), "faild to get constant buffer description");

      _root_params.emplace_back(D3D12_ROOT_PARAMETER1
      {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
        .Descriptor
        {
          .ShaderRegister = resource_desc.BindPoint,
          .RegisterSpace  = resource_desc.Space,
          .Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        },
      });
      break;
    }
    
    case D3D_SIT_SAMPLER:
    {
      err_if(_has_sampler, "TODO: duplication sampler, currently, assume I only use single sampler");
      _has_sampler = true;
      break;
    }

    case D3D_SIT_TEXTURE:
    {
      resource_indexs[resource_desc.Name] = _root_params.size();

      _ranges.emplace(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, resource_desc.BindPoint, resource_desc.Space, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

      _root_params.emplace_back(D3D12_ROOT_PARAMETER1
      {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable =
        {
          .NumDescriptorRanges = 1u,
          .pDescriptorRanges   = &_ranges.back(),
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      });
      break;
    }

    case D3D_SIT_BYTEADDRESS:
    {
      resource_indexs[resource_desc.Name] = _root_params.size();

      _ranges.emplace(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1u, resource_desc.BindPoint, resource_desc.Space);

      _root_params.emplace_back(D3D12_ROOT_PARAMETER1
      {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable =
        {
          .NumDescriptorRanges = 1u,
          .pDescriptorRanges   = &_ranges.back(),
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      });
      break;
    }

    default:
      err_if(true, "TODO: expand more parameter type of shader resources");
    }
  }
}

}}
