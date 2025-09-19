#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <glm/glm.hpp>

#include <ranges>

namespace vn { namespace renderer {

struct Vertex
{
  glm::vec2 pos;
  glm::vec2 uv;
	glm::vec4 color;
};

class FrameBuffer
{
public:
  FrameBuffer()                              = default;
  ~FrameBuffer()                             = default;
  FrameBuffer(FrameBuffer const&)            = delete;
  FrameBuffer(FrameBuffer&&)                 = delete;
  FrameBuffer& operator=(FrameBuffer const&) = delete;
  FrameBuffer& operator=(FrameBuffer&&)      = delete;

  void init(uint32_t per_frame_capacity) noexcept;

  void clear() noexcept
  {
    _size          = 0;
    _window_offset = 0;
  }

  void upload(ID3D12GraphicsCommandList* command_list, std::span<Vertex> vertices, std::span<uint16_t> indices) noexcept;

private:
  auto append(void const* data, uint32_t size) noexcept -> FrameBuffer&;

  template <std::ranges::range T>
  requires std::ranges::sized_range<T> && std::ranges::contiguous_range<T>
  auto append_range(T&& values) noexcept -> FrameBuffer&
  {
    return append(std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<T>));
  }

  inline auto get_current_frame_buffer_pointer() const noexcept -> std::byte*;
  inline auto get_current_frame_buffer_address() const noexcept -> D3D12_GPU_VIRTUAL_ADDRESS;

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> _buffer;
  std::byte*                             _pointer{};
  uint32_t                               _per_frame_capacity{};
  uint32_t                               _size{};
  uint32_t                               _window_offset{};
};

}}