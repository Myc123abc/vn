#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <string>
#include <vector>

namespace vn { namespace renderer {

enum class DescriptorType
{
  constants,
  srv,
  uav,
};

enum class ShaderType
{
  all,
  pixel,
};

enum class DescriptorFlag
{
  none,
  static_data,
};

struct DescriptorInfo
{
  uint32_t       shader_register{};
  uint32_t       shader_register_space{};
  DescriptorType type{};
  uint32_t       num{};
  ShaderType     shader_type{};
  DescriptorFlag flags{};
};

enum class DXGIFormat
{
  rgb32_float,
  rg32_float,
  r32_uint,
  bgra8_unorm,
  rgba8_unorm,
};

struct VertexInputParameter
{
  std::string name;
  DXGIFormat  format;
};

struct PipelineConfig
{
  std::string                       shader;
  std::string                       vs;
  std::string                       ps;
  std::string                       include;
  std::vector<DescriptorInfo>       descriptor_infos;
  bool                              sampler{};
  std::vector<VertexInputParameter> vertex_params;
  DXGIFormat                        rtv_format{};
  bool                              use_blend{};
  bool                              use_depth_test{};
};

class Pipeline
{
public:
  Pipeline()                           = default;
  ~Pipeline()                          = default;
  Pipeline(Pipeline const&)            = delete;
  Pipeline(Pipeline&&)                 = delete;
  Pipeline& operator=(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline&&)      = delete;
  
  void init(PipelineConfig const& cfg) noexcept;

  void bind(ID3D12GraphicsCommandList1* cmd) const noexcept;
  
  template <typename ConstantsType>
  void set_descriptors(ID3D12GraphicsCommandList1* cmd, ConstantsType const& constants, std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> const& handles) const noexcept
  {
    cmd->SetGraphicsRoot32BitConstants(0, sizeof(constants), &constants, 0);
    for (auto i = 0; i < handles.size(); ++i)
      if (handles[i].ptr) cmd->SetGraphicsRootDescriptorTable(i + 1, handles[i]);
  }

private:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;
};

}}
