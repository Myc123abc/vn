#pragma once

#include "../object_pool.hpp"
#include "image.hpp"

namespace vn { namespace renderer {

using ImagePoolType = ObjectPool<Image, 32>;
using ImageHandle   = ImagePoolType::Handle;

class MemoryPool
{
private:
  MemoryPool()                             = default;
  ~MemoryPool()                            = default;
public:
  MemoryPool(MemoryPool const&)            = delete;
  MemoryPool(MemoryPool&&)                 = delete;
  MemoryPool& operator=(MemoryPool const&) = delete;
  MemoryPool& operator=(MemoryPool&&)      = delete;

  static auto const instance() noexcept
  {
    static MemoryPool instance;
    return &instance;
  }

  auto alloc_image()                noexcept { return _image_pool.create();    }
  auto get(ImageHandle handle)      noexcept { return _image_pool.get(handle); }
  void destroy(ImageHandle& handle) noexcept
  {
    get(handle)->destroy();
    _image_pool.destroy(handle);
  }

private:
  ObjectPool<Image, 32> _image_pool;
};

}}
