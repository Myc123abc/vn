#include "window_resource.hpp"
#include "renderer.hpp"
#include "core.hpp"
#include "../util.hpp"

#include <vulkan/vulkan_win32.h>

#include <algorithm>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void SwapchainResource::init(HWND handle, uint32_t width, uint32_t height, bool transparent) noexcept
{
  this->transparent = transparent;

  auto core = Core::instance();

  // create dxgi swapchain
  auto swapchain = ComPtr<IDXGISwapChain1>{};
  auto swapchain_desc = DXGI_SWAP_CHAIN_DESC1{};
  swapchain_desc.BufferCount      = Frame_Count;
  swapchain_desc.Width            = width;
  swapchain_desc.Height           = height;
  swapchain_desc.Format           = Swapchain_Image_Format_DXGI;
  swapchain_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapchain_desc.SampleDesc.Count = 1;
  if (transparent)
  {
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    err_if(core->factory()->CreateSwapChainForComposition(core->command_queue(), &swapchain_desc, nullptr, &swapchain),
            "failed to create swapchain for composition");

    // create composition
    err_if(DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&_comp_device)),
            "failed to create composition device");
    err_if(_comp_device->CreateTargetForHwnd(handle, true, &_comp_target),
            "failed to create composition target");
    err_if(_comp_device->CreateVisual(&_comp_visual),
            "failed to create composition visual");
    err_if(_comp_visual->SetContent(swapchain.Get()),
            "failed to bind swapchain to composition visual");
    err_if(_comp_target->SetRoot(_comp_visual.Get()),
            "failed to bind composition visual to target");
    err_if(_comp_device->Commit(),
            "failed to commit composition device");
  }  
  else
    err_if(core->factory()->CreateSwapChainForHwnd(core->command_queue(), handle, &swapchain_desc, nullptr, nullptr, &swapchain),
            "failed to create swapchain");
  err_if(swapchain.As(&this->swapchain), "failed to get swapchain");

  for (auto i = 0; i < Frame_Count; ++i)
  {
    auto resource = ComPtr<ID3D12Resource>{};
    err_if(swapchain->GetBuffer(i, IID_PPV_ARGS(&resource)), "failed to get dxgi swapchain image");
    images[i].init(resource.Get(), width, height, Swapchain_Image_Format_Vulkan);
  }
}

void SwapchainResource::destroy() noexcept
{
  std::ranges::for_each(images, [](auto& image) { image.destroy(); });
}

void SwapchainResource::resize(uint32_t width, uint32_t height) noexcept
{
  auto core = Core::instance();

  // wait gpu finish
  core->wait_gpu_complete();

  // reset swapchain relation resources
  std::ranges::for_each(images, [](auto& image) { image.destroy(); });
  if (transparent)
    _comp_visual->SetContent(nullptr);

  // resize swapchain
  err_if(swapchain->ResizeBuffers(Frame_Count, width, height, DXGI_FORMAT_UNKNOWN, 0),
          "failed to resize swapchain");

  if (transparent)
  {
    // rebind composition resources
    err_if(_comp_visual->SetContent(swapchain.Get()),
            "failed to bind swapchain to composition visual");
    err_if(_comp_device->Commit(),
            "failed to commit composition device");
  }

  // get swapchain images
  for (auto i = 0; i < Frame_Count; ++i)
  {
    auto resource = ComPtr<ID3D12Resource>{};
    err_if(swapchain->GetBuffer(i, IID_PPV_ARGS(&resource)), "failed to get dxgi swapchain image");
    images[i].init(resource.Get(), width, height, Swapchain_Image_Format_Vulkan);
  }
}

void FrameResource::init() noexcept
{
  auto core = Core::instance();

  // create command buffers
  auto command_buffer_allocate_info = VkCommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  command_buffer_allocate_info.commandPool        = core->command_pool();
  command_buffer_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info.commandBufferCount = cmds.size();
  err_if(vkAllocateCommandBuffers(core->device(), &command_buffer_allocate_info, cmds.data()) != VK_SUCCESS,
          "failed to create command buffers");

  // init frame buffer
  buffer.init(1024); // TODO: test dynamic raise up buffer
}

void FrameResource::destroy() const noexcept
{
  buffer.destroy();
}

void WindowResource::init(Window const& window) noexcept
{
  this->window = window;

  swapchain_resource.init(window.handle, window.width, window.height);
  frame_resource.init();

  // first frame rendered then display
  Renderer::instance()->add_current_frame_render_finish_proc([handle = window.handle]
  { 
    ShowWindow(handle, SW_SHOW);
    UpdateWindow(handle);
  });
}

void WindowResource::render() noexcept
{
  auto  core               = Core::instance();
  auto  renderer           = Renderer::instance();
  auto  cmd                = frame_resource.cmds[core->frame_index()];
  auto& swapchain_resource = (window.moving || window.resizing) ? renderer->_fullscreen_swapchain_resource : this->swapchain_resource;
  auto  swapchain_image    = swapchain_resource.current_image();

  // reset command buffer
  err_if(vkResetCommandBuffer(cmd, 0), "failed to reset command buffer");
  
  // start to record commands
  auto command_buffer_begin_info = VkCommandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  err_if(vkBeginCommandBuffer(cmd, &command_buffer_begin_info), "failed to begin record command");

  // render begin
  swapchain_image->set_layout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  auto color_attachment_info = VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
  color_attachment_info.imageView   = swapchain_image->view();
  color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment_info.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment_info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

  auto rendering_info = VkRenderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
  rendering_info.renderArea.extent    = swapchain_image->extent();
  rendering_info.layerCount           = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments    = &color_attachment_info;

  vkCmdBeginRendering(cmd, &rendering_info);

  //
  // render window
  //
  
  // set scissor and viewport
  auto scissor = VkRect2D{};
  scissor.extent = swapchain_image->extent();
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  auto viewport = VkViewport{};
  viewport.width    = static_cast<float>(scissor.extent.width);
  viewport.height   = static_cast<float>(scissor.extent.height);
  viewport.maxDepth = 1.f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // bind and set pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->_pipeline);
  //vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->_pipeline_layout, 0, 0, nullptr, 0, nullptr);
  auto pc = PushConstant{};
  pc.vertices         = frame_resource.buffer.address(core->frame_index());
  pc.window_extent    = { scissor.extent.width, scissor.extent.height };
  if (window.moving || window.resizing)
    pc.window_pos = window.pos();
  vkCmdPushConstants(cmd, renderer->_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

  // upload buffer
  frame_resource.buffer.clear();
  frame_resource.buffer.upload(cmd, frame_resource.vertices, frame_resource.indices);
  
  // draw
  vkCmdDrawIndexed(cmd, frame_resource.indices.size(), 1, 0, 0, 0);

  // render end
  vkCmdEndRendering(cmd);

  // end command buffer
  swapchain_image->set_layout(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  err_if(vkEndCommandBuffer(cmd), "failed to end command buffer");
}

}}