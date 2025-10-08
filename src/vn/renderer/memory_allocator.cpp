#include "memory_allocator.hpp"
#include "core.hpp"
#include "../util.hpp"
#include "config.hpp"

#include <vulkan/vulkan_win32.h>

using namespace vn;
using namespace vn::renderer;

namespace {

auto align(uint32_t value, uint32_t alignment) noexcept
{
  return (value + alignment - 1) / alignment * alignment;
}

auto calculate_capacity(uint32_t old_capacity, uint32_t need_capacity)
{
  auto factor = (old_capacity < 256 * 1024)      ? 2.0 :
                (old_capacity < 8 * 1024 * 1024) ? 1.5 : 1.25;

  auto capacity = static_cast<uint32_t>(old_capacity * factor);
  if (old_capacity < need_capacity) capacity = need_capacity;

  // Round up to 256 bytes
  capacity = align(capacity, 256);

  // Clamp to max budget (optional)
  constexpr size_t Max = 128ull * 1024 * 1024;
  if (capacity > Max) capacity = align(need_capacity, 256);

  return capacity;
}

auto create_image_view(VkImage image, VkFormat format) noexcept
{
  auto core = Core::instance();
  auto view = VkImageView{};
  auto image_view_create_info = VkImageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  image_view_create_info.image                       = image;
  image_view_create_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format                      = format;
  image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_create_info.subresourceRange.layerCount = 1;
  image_view_create_info.subresourceRange.levelCount = 1;
  err_if(vkCreateImageView(core->device(), &image_view_create_info, nullptr, &view), "faild to create image view");
  return view;
}

}

namespace vn { namespace renderer {

////////////////////////////////////////////////////////////////////////////////
///                             Frame Buffer
////////////////////////////////////////////////////////////////////////////////

void FrameBuffer::init(uint32_t per_frame_capacity) noexcept
{
  auto core = Core::instance();

  _size               = {};
  _per_frame_capacity = align(per_frame_capacity, 8);

  // create buffer
  auto buffer_create_info = VkBufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  buffer_create_info.size  = _per_frame_capacity * Frame_Count;
  buffer_create_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  auto allocation_create_info = VmaAllocationCreateInfo{};
  allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
  auto allocation_info = VmaAllocationInfo{};
  err_if(vmaCreateBuffer(core->vma_allocator(), &buffer_create_info, &allocation_create_info, &_buffer, &_allocation, &allocation_info) != VK_SUCCESS,
           "failed to create buffer");
  
  // get mapped pointer
  _data = allocation_info.pMappedData;

  // get device address
  auto device_address_info = VkBufferDeviceAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
  device_address_info.buffer = _buffer;
  _address = vkGetBufferDeviceAddress(core->device(), &device_address_info);
}

void FrameBuffer::destroy() const noexcept
{
  auto core = Core::instance();

  vmaDestroyBuffer(core->vma_allocator(), _buffer, _allocation);
}

auto FrameBuffer::get_buffer_pointer(uint32_t frame_index) const noexcept -> std::byte*
{
  return reinterpret_cast<std::byte*>(_data) + frame_index * _per_frame_capacity;
}

auto FrameBuffer::append(void const* data, uint32_t size) noexcept -> uint32_t
{
  size = align(size, 4);
  
  auto total_size = _size + size;
  if (total_size <= _per_frame_capacity)
  {
    memcpy(get_buffer_pointer(Core::instance()->frame_index()) + _size, data, size);
    _size = total_size;
  }
  else
  {
    err_if(true, "TODO: dynamic alloc buffer");
  }
  return size;
}

#if 0
auto FrameBuffer::append(void const* data, uint32_t size) noexcept -> uint32_t
{
  // promise aligment
  size = align(size, 4);
  auto total_size = _size + size;
  if (total_size <= _per_frame_capacity)
  {
    memcpy(get_current_frame_buffer_pointer() + _size, data, size);
    _size = total_size;
  }
  else
  {
    // add old buffer for destroy
    Renderer::instance()->add_current_frame_render_finish_proc([buffer = _buffer] {});

    // temporary copy old data
    std::vector<std::byte> old_data(_size);
    memcpy(old_data.data(), get_current_frame_buffer_pointer(), _size);

    // create new bigger one
    init(calculate_capacity(_per_frame_capacity, total_size));

    // copy old data to new buffer
    append(old_data.data(), old_data.size());
    // now copy current data again
    append(data, size);
  }
  return size;
}
#endif

void FrameBuffer::upload(VkCommandBuffer cmd, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept
{
  auto vertices_offset = append_range(vertices);
  auto indices_offset  = append_range(indices);

  // bind index buffer
  vkCmdBindIndexBuffer(cmd, _buffer, Core::instance()->frame_index() * _per_frame_capacity + vertices_offset, VK_INDEX_TYPE_UINT16);

  // TODO: add indices offset for other data like shape properties
}

////////////////////////////////////////////////////////////////////////////////
///                               Image
////////////////////////////////////////////////////////////////////////////////

void Image::init(VkImage image, uint32_t width, uint32_t height, VkFormat format) noexcept
{
  _image  = image;
  _extent = { width, height };
  _format = format;
  _view   = create_image_view(image, format);
}

void Image::init(ID3D12Resource* resource, uint32_t width, uint32_t height, VkFormat format) noexcept
{
  auto core = Core::instance();

  _extent = { width, height };
  _format = format;

  // create shared handle
  auto handle = HANDLE{};
  err_if(core->dxgi_device()->CreateSharedHandle(resource, nullptr, GENERIC_ALL, nullptr, &handle),
          "failed to share dxgi swapchain image");  

  // create imported image
  auto external_memory_image_create_info = VkExternalMemoryImageCreateInfo{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
  external_memory_image_create_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

  auto image_create_info = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  image_create_info.pNext       = &external_memory_image_create_info;
  image_create_info.imageType   = VK_IMAGE_TYPE_2D;
  image_create_info.format      = format;
  image_create_info.extent      = { width, height, 1 };
  image_create_info.mipLevels   = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples     = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling      = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  auto vma_allocation_create_info = VmaAllocationCreateInfo{};
  vma_allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  vma_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  auto import_memory_win32_handle_info = VkImportMemoryWin32HandleInfoKHR{ VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
  import_memory_win32_handle_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  import_memory_win32_handle_info.handle     = handle;

  err_if(vmaCreateDedicatedImage(core->vma_allocator(), &image_create_info, &vma_allocation_create_info, &import_memory_win32_handle_info, &_image, &_allocation, nullptr),
          "failed to create dedicated image");

  CloseHandle(handle);

  _view = create_image_view(_image, format);
}

void Image::destroy() noexcept
{
  auto core = Core::instance();

  vkDestroyImageView(core->device(), _view, nullptr);
  if (_image && _allocation)
    vmaDestroyImage(core->vma_allocator(), _image, _allocation);

  _image      = {};
  _allocation = {};
  _view       = {};
  _extent     = {};
  _format     = {};
  _layout     = {};
}

void Image::set_layout(VkCommandBuffer cmd, VkImageLayout layout) noexcept
{
  if (_layout == layout) return;
  auto barrier = VkImageMemoryBarrier2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
  // TODO: use all commands bit will stall the GPU pipeline a bit, is inefficient.
  // should make stageMask more accurate.
  // reference: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
  barrier.srcStageMask                = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask               = VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask                = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask               = VK_ACCESS_2_MEMORY_READ_BIT  |
                                        VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.oldLayout                   = _layout;
  barrier.newLayout                   = layout;
  barrier.image                       = _image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  auto dep_info = VkDependencyInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers    = &barrier;

  vkCmdPipelineBarrier2(cmd, &dep_info);

  _layout = layout;
}

}}