#include "renderer.hpp"
#include "core.hpp"
#include "../util.hpp"
#include "message_queue.hpp"
#include "shader_compiler.hpp"

#include <glm/glm.hpp>

#include <span>
#include <chrono>

using namespace vn;
using namespace vn::renderer;
using namespace Microsoft::WRL;

namespace
{

auto create_shader_module(std::span<uint32_t> shader) noexcept
{
  auto core = Core::instance();

  auto shader_module             = VkShaderModule{};
  auto shader_module_create_info = VkShaderModuleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  shader_module_create_info.codeSize = shader.size_bytes();
  shader_module_create_info.pCode    = shader.data();
  err_if(vkCreateShaderModule(core->device(), &shader_module_create_info, nullptr, &shader_module) != VK_SUCCESS,
          "failed to create shader module");

  return shader_module;
}

}

namespace vn { namespace renderer {

void Renderer::init() noexcept
{
  _thread = std::thread{[this]
  {
    Core::instance()->init();
    ShaderCompiler::instance()->init();
    create_pipeline_resource();
    run();
  }};
}

void Renderer::destroy() noexcept
{
  // exit render thread
  _exit.store(true, std::memory_order_release);
  _render_acquire.release();
  _thread.join();

  // wait gpu render complete
  Core::instance()->wait_gpu_complete();

  // destroy resources
  process_frame_render_finish_procs(true);
  destroy_pipeline_resource();
  for (auto& [_, v] : _window_resources) v.destroy(); // TODO: use ranges destroy?
  _fullscreen_swapchain_resource.destroy();
  ShaderCompiler::instance()->destroy();
  Core::instance()->destroy();
}

void Renderer::create_pipeline_resource() noexcept
{
  auto core = Core::instance();

  // create descriptor set layout
  auto descriptor_set_layout_create_info = VkDescriptorSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  err_if(vkCreateDescriptorSetLayout(core->device(), &descriptor_set_layout_create_info, nullptr, &_descriptor_set_layout),
          "failed to create descriptor set layout");

  //
  // create pipeline layout
  //

  auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  auto descriptor_set_layouts      = { _descriptor_set_layout };
  auto push_constant               = VkPushConstantRange{};

  // fill push constant
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  push_constant.size       = sizeof(PushConstant);

  // make push constants
  auto push_constants = { push_constant };
  
  // fill pipeline layout create info
  pipeline_layout_create_info.setLayoutCount         = static_cast<uint32_t>(descriptor_set_layouts.size());
  pipeline_layout_create_info.pSetLayouts            = descriptor_set_layouts.begin();
  pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constants.size());
  pipeline_layout_create_info.pPushConstantRanges    = push_constants.begin();
  
  // create pipeline layout
  err_if(vkCreatePipelineLayout(core->device(), &pipeline_layout_create_info, nullptr, &_pipeline_layout) != VK_SUCCESS,
           "failed to create pipeline layout");

  //
  // create pipeline
  //

  auto rendering_info = VkPipelineRenderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
  rendering_info.colorAttachmentCount    = 1;
  rendering_info.pColorAttachmentFormats = &Swapchain_Image_Format_Vulkan;

  auto shaders       = ShaderCompiler::instance()->compile_graphics_shaders("assets/shader.vert", "assets/shader.frag");
  auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>{ 2, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
  shader_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].module = create_shader_module(shaders.first);
  shader_stages[0].pName  = "main";
  shader_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].module = create_shader_module(shaders.second);
  shader_stages[1].pName  = "main";

  auto vertex_input_state = VkPipelineVertexInputStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  auto input_assembly_state = VkPipelineInputAssemblyStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  auto viewport_state = VkPipelineViewportStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount  = 1;

  auto rasterization_state = VkPipelineRasterizationStateCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state.cullMode    = VK_CULL_MODE_BACK_BIT;
  rasterization_state.frontFace   = VK_FRONT_FACE_CLOCKWISE;
  rasterization_state.lineWidth   = 1.f;

  auto multisample_state = VkPipelineMultisampleStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample_state.minSampleShading     = 1.f;

  auto depth_stencil_state = VkPipelineDepthStencilStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
  
  auto color_blend_attachment = VkPipelineColorBlendAttachmentState{};
  color_blend_attachment.blendEnable         = true,
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD,
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD,
  color_blend_attachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT;
  auto color_blend_state = VkPipelineColorBlendStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  color_blend_state.attachmentCount = 1;
  color_blend_state.pAttachments    = &color_blend_attachment;

  auto dynamics = std::vector<VkDynamicState>
  {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };
  auto dynamic_state = VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dynamic_state.dynamicStateCount = (uint32_t)dynamics.size();
  dynamic_state.pDynamicStates    = dynamics.data();

  auto graphics_pipeline_create_info = VkGraphicsPipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  graphics_pipeline_create_info.pNext               = &rendering_info;
  graphics_pipeline_create_info.stageCount          = static_cast<uint32_t>(shader_stages.size());
  graphics_pipeline_create_info.pStages             = shader_stages.data();
  graphics_pipeline_create_info.pVertexInputState   = &vertex_input_state;
  graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state;
  graphics_pipeline_create_info.pTessellationState  = nullptr;
  graphics_pipeline_create_info.pViewportState      = &viewport_state;
  graphics_pipeline_create_info.pRasterizationState = &rasterization_state;
  graphics_pipeline_create_info.pMultisampleState   = &multisample_state;
  graphics_pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
  graphics_pipeline_create_info.pColorBlendState    = &color_blend_state;
  graphics_pipeline_create_info.pDynamicState       = &dynamic_state;
  graphics_pipeline_create_info.layout              = _pipeline_layout;

  err_if(vkCreateGraphicsPipelines(core->device(), nullptr, 1, &graphics_pipeline_create_info, nullptr, &_pipeline) != VK_SUCCESS,
          "failed to create pipeline");

  // destroy shader modules
  vkDestroyShaderModule(core->device(), shader_stages[0].module, nullptr);
  vkDestroyShaderModule(core->device(), shader_stages[1].module, nullptr);
}

void Renderer::destroy_pipeline_resource() const noexcept
{
  auto core = Core::instance();

  vkDestroyPipeline(core->device(), _pipeline, nullptr);
  vkDestroyPipelineLayout(core->device(), _pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(core->device(), _descriptor_set_layout, nullptr);
}

void Renderer::add_current_frame_render_finish_proc(std::function<void()>&& func) noexcept
{
  auto core = Core::instance();
  _current_frame_render_finish_procs.emplace_back(
  [func, last_fence_value = core->get_last_fence_value(), frame_index = core->frame_index()]
  (bool directly_destroy)
  {
    auto fence_value = Core::instance()->fence()->GetCompletedValue();
    err_if(fence_value == UINT64_MAX, "failed to get fence value because device is removed");
    auto render_complete = fence_value >= last_fence_value && Core::instance()->frame_index() == frame_index || directly_destroy;
    if (render_complete) func();
    return render_complete;
  });
}

void Renderer::run() noexcept
{
  auto beg = std::chrono::steady_clock ::now();
  uint32_t count{};
  while (true)
  {
    // wait render acquire
    _render_acquire.acquire();
    if (_exit.load(std::memory_order_acquire)) [[unlikely]]
      return;

    // process last render finish processes
    process_frame_render_finish_procs(false);

    MessageQueue::instance()->process_messages();

    update();
    render();

    ++count;
    auto now = std::chrono::steady_clock ::now();
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
  for (auto& [k, v] : _window_resources)
  {
    auto& window         = v.window;
    auto& frame_resource = v.frame_resource;

    // clear vertices and indices
    frame_resource.vertices.clear();
    frame_resource.indices.clear();
    frame_resource.idx_beg = {};
    
    frame_resource.vertices.append_range(std::vector<Vertex>
    {
      { { window.width / 2, 0             }, {}, 0x00ff00ff },
      { { window.width,     window.height }, {}, 0x0000ffff },
      { { 0,                window.height }, {}, 0x00ffffff },
    });
    frame_resource.indices.append_range(std::vector<uint16_t>
    {
      static_cast<uint16_t>(frame_resource.idx_beg + 0),
      static_cast<uint16_t>(frame_resource.idx_beg + 1),
      static_cast<uint16_t>(frame_resource.idx_beg + 2),
    });
    frame_resource.idx_beg += 3;
  }
}

void Renderer::render() noexcept
{
  auto core = Core::instance();

  if (!core->current_frame_available()) return;

  auto cmds = std::vector<VkCommandBuffer>{};
  cmds.reserve(_window_resources.size());
  for (auto& [_, wr] : _window_resources)
  {
    wr.render();
    cmds.emplace_back(wr.frame_resource.cmds[core->frame_index()]);
  }

  // submit queue
  auto submit_info = VkSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submit_info.commandBufferCount = cmds.size();
  submit_info.pCommandBuffers    = cmds.data();
  err_if(vkQueueSubmit(core->queue(), 1, &submit_info, core->queue_fence()), "failed to submit commands");

  // TODO: why initialize so slow...
  // TODO: move and resize has problem
  // TODO: the transparent of triangle problem also exist, use vulkan is not resolve... maybe is window style problem?
  //       in vk-transparent-window, it's not have the nvidia tag in start program...
  // TODO: check destroy old resources have any problem?

  // present swapchain
  for (auto const& [k, wr] : _window_resources)
  {
    // TODO: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
    //       use present1, and fullscreen optionmal also have
    //       and sometimes block, to read Multithreading Considerations. in above link
    //       see variable frame rate in above link
    if (wr.window.moving || wr.window.resizing)
      err_if(_fullscreen_swapchain_resource.swapchain->Present(0, 0), "failed to present swapchain");
    else
      err_if(wr.swapchain_resource.swapchain->Present(0, 0), "failed to present swapchain");
  }
  
  core->move_to_next_frame();
}

}}