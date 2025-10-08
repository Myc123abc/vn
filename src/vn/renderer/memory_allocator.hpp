#pragma once

#include <d3d12.h>

#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <ranges>

namespace vn { namespace renderer {

struct alignas(8) Vertex
{
  glm::vec2 pos{};
  glm::vec2 uv{};
  uint32_t  color{};
};

struct PushConstant
{
  VkDeviceAddress       vertices{};
  glm::vec<2, uint32_t> window_extent{};
  glm::vec<2, int>      window_pos{};
};

class FrameBuffer
{
public:
  FrameBuffer()                              = default;
  ~FrameBuffer()                             = default;
  FrameBuffer(FrameBuffer const&)            = default;
  FrameBuffer(FrameBuffer&&)                 = delete;
  FrameBuffer& operator=(FrameBuffer const&) = default;
  FrameBuffer& operator=(FrameBuffer&&)      = delete;

  void init(uint32_t per_frame_capacity) noexcept;
  void destroy() const noexcept;

  void clear() noexcept { _size = {}; }

  void upload(VkCommandBuffer cmd, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept;

  auto address(uint32_t frame_index) const noexcept { return _address + frame_index * _per_frame_capacity; }

private:
  auto append(void const* data, uint32_t size) noexcept -> uint32_t;

  template <std::ranges::range T>
  requires std::ranges::sized_range<T> && std::ranges::contiguous_range<T>
  auto append_range(T&& values) noexcept -> uint32_t
  {
    return append(std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<T>));
  }

  auto get_buffer_pointer(uint32_t frame_index) const noexcept -> std::byte*;

private:
  VkBuffer        _buffer{};
  VmaAllocation   _allocation{};
  VkDeviceAddress _address{};
  void*           _data{};
  uint32_t        _per_frame_capacity{};
  uint32_t        _size{};
};

class Image
{
public:
  Image()                        = default;
  ~Image()                       = default;
  Image(Image const&)            = default;
  Image(Image&&)                 = delete;
  Image& operator=(Image const&) = default;
  Image& operator=(Image&&)      = delete;

  void init(VkImage image, uint32_t width, uint32_t height, VkFormat format) noexcept;
  void init(ID3D12Resource* resource, uint32_t width, uint32_t height, VkFormat format) noexcept;
  void destroy() noexcept;

  void set_layout(VkCommandBuffer cmd, VkImageLayout layout) noexcept;

  auto view()   const noexcept { return _view;   }
  auto extent() const noexcept { return _extent; }

private:
  VkImage       _image{};
  VmaAllocation _allocation{};
  VkImageView   _view{};
  VkExtent2D    _extent{};
  VkFormat      _format{};
  VkImageLayout _layout{};
};

}}