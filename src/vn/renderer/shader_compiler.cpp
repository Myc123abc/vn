#include "shader_compiler.hpp"
#include "../util.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

#include <array>

using namespace vn;

namespace
{

auto create_shader(EShLanguage stage, std::string_view shader_path) noexcept
{
  auto shader_content = read_file(shader_path);
  auto strings        = std::array<char const*, 1>{ shader_content.data() };

  auto shader = glslang::TShader{ stage };
  shader.setStrings(strings.data(), strings.size());
  shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 460);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
  auto res = shader.parse(GetDefaultResources(), 460, true, EShMsgDefault);
  err_if(!res, shader.getInfoLog());
  
  return shader;
}

auto compile_shader(glslang::TProgram const& program, EShLanguage stage) noexcept
{
  auto intermediate = program.getIntermediate(stage);
  err_if(!intermediate, "failed to get intermediate in glslang");
  
  auto spirv   = std::vector<uint32_t>{};
  auto options = glslang::SpvOptions{};
  options.validate                         = true;
#ifndef NDEBUG
  options.generateDebugInfo                = true;
  options.emitNonSemanticShaderDebugInfo   = false; // these will use VK_KHR_shader_relaxed_extended_instruction
  options.emitNonSemanticShaderDebugSource = false; // these will use VK_KHR_shader_relaxed_extended_instruction
  options.disableOptimizer                 = true;
#else
  options.stripDebugInfo   = true;
  options.optimizeSize     = true;
#endif
  glslang::GlslangToSpv(*intermediate, spirv, nullptr, &options);

  return spirv;
}

}

namespace vn { namespace renderer {

void ShaderCompiler::init() const noexcept
{
  err_if(!glslang::InitializeProcess(), "failed to initialize glslang process");
}

void ShaderCompiler::destroy() const noexcept
{
  glslang::FinalizeProcess();
}

auto ShaderCompiler::compile_graphics_shaders(std::string_view vertex, std::string_view fragment) const noexcept -> std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
{
  auto vertex_shader   = create_shader(EShLangVertex,   vertex);
  auto fragment_shader = create_shader(EShLangFragment, fragment);

  auto program = glslang::TProgram{};
  program.addShader(&vertex_shader);
  program.addShader(&fragment_shader);
  auto res = program.link(EShMsgDefault);
  err_if(!res, program.getInfoLog());

  // reflection
#if 0
  err_if(!program.buildReflection(), "failed to build reflection in glslang");
  auto num_uniform_variables = program.getNumUniformVariables();
  auto num_uniform_blocks    = program.getNumUniformBlocks();

  for (auto i = 0; i < num_uniform_blocks; ++i)
  {
    auto reflection = program.getUniformBlock(i);
  }
#endif

  return { compile_shader(program, EShLangVertex), compile_shader(program, EShLangFragment) };
}

}}