#pragma once

#include "image.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace vn { namespace renderer {

class Pipeline
{
public:
  Pipeline()                           = default;
  ~Pipeline()                          = default;
  Pipeline(Pipeline const&)            = delete;
  Pipeline(Pipeline&&)                 = delete;
  Pipeline& operator=(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline&&)      = delete;
  
  void init_graphics(
    std::string shader,
    std::string vs,
    std::string ps,
    std::string include,
    ImageFormat rtv_format,
    bool        use_blend      = false,
    bool        use_depth_test = false
  ) noexcept;

  void init_compute(std::string shader, std::string cs, std::string include = {}) noexcept;

  void bind(ID3D12GraphicsCommandList1* cmd) const noexcept;

  void set_descriptors(ID3D12GraphicsCommandList1* cmd, std::vector<std::pair<std::string_view, D3D12_GPU_DESCRIPTOR_HANDLE>> const& handles) const noexcept;

  template <typename ConstantsType>
  void set_descriptors(ID3D12GraphicsCommandList1* cmd, std::string_view constants_name, ConstantsType const& constants, std::vector<std::pair<std::string_view, D3D12_GPU_DESCRIPTOR_HANDLE>> const& handles) const noexcept
  {
    if (_resource_indexs.contains(constants_name.data()))
    {
      if (_is_graphics_pipeline)
        cmd->SetGraphicsRoot32BitConstants(_resource_indexs.at(constants_name.data()), sizeof(constants) / 4, &constants, 0);
      else
        cmd->SetComputeRoot32BitConstants(_resource_indexs.at(constants_name.data()), sizeof(constants) / 4, &constants, 0);
    }
    set_descriptors(cmd, handles);
  }

private:
  Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline_state;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> _root_signature;
  std::unordered_map<std::string, uint32_t>   _resource_indexs;
  bool                                        _is_graphics_pipeline{};
};

}}
