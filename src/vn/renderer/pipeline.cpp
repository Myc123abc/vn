#include "pipeline.hpp"
#include "error_handling.hpp"
#include "core.hpp"
#include "compiler.hpp"

#include <directx/d3dx12.h>

using namespace Microsoft::WRL;
using namespace vn;
using namespace vn::renderer;

namespace
{

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
  case rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
}

auto get_dxgi_format_offset(DXGIFormat format) noexcept
{
  using enum DXGIFormat;
  switch (format)
  {
  case rgb32_float: return 12;
  case rg32_float:  return 8;
  
  case r32_uint:
  case bgra8_unorm:
  case rgba8_unorm: return 4;
  }
}

}

namespace vn { namespace renderer {

void Pipeline::init(PipelineConfig const& cfg) noexcept
{
  auto core = Core::instance();

  auto compile_result = Compiler::instance()->compile(cfg.shader, cfg.vs, cfg.ps, cfg.include);
  _root_signature = compile_result.root_signature;
#if 1
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
  signature_desc.Init_1_1(root_params.size(), root_params.data(), 1, &sampler_desc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  auto signature = ComPtr<ID3DBlob>{};
  auto error     = ComPtr<ID3DBlob>{};
  err_if(D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
          "failed to serialize root signature");
  err_if(core->device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_root_signature)),
          "failed to create root signature");
#endif
  
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
  ps_stream.input_layout          = compile_result.input_layout_desc;
  ps_stream.primitive_topology    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  ps_stream.vs                    = compile_result.vs;
  ps_stream.ps                    = compile_result.ps;
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
