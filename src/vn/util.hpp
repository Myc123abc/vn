#pragma once

#include <stdint.h>

namespace vn {

inline auto align(uint32_t value, uint32_t alignment) noexcept
{
  return (value + alignment - 1) / alignment * alignment;
}

}
