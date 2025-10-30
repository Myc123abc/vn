#pragma once

#include <functional>

namespace vn
{

template <typename T>
inline void combine_hash(std::size_t& seed, const T& v) noexcept
{
  seed ^= std::hash<T>{}(v) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

template <typename... Args>
constexpr std::size_t generic_hash(const Args&... args) noexcept
{
  auto seed = std::size_t{};
  (combine_hash(seed, args), ...);
  return seed;
}

}
