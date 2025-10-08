#pragma once

#include <string_view>
#include <vector>

namespace vn { namespace renderer {

class ShaderCompiler
{
private:
  ShaderCompiler()                                 = default;
  ~ShaderCompiler()                                = default;
public:
  ShaderCompiler(ShaderCompiler const&)            = delete;
  ShaderCompiler(ShaderCompiler&&)                 = delete;
  ShaderCompiler& operator=(ShaderCompiler const&) = delete;
  ShaderCompiler& operator=(ShaderCompiler&&)      = delete;

  static auto const instance() noexcept
  {
    static ShaderCompiler instance;
    return &instance;
  }

  void init()    const noexcept;
  void destroy() const noexcept;

  auto compile_graphics_shaders(std::string_view vertex, std::string_view fragment) const noexcept -> std::pair<std::vector<uint32_t>, std::vector<uint32_t>>;
};

}}