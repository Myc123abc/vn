#pragma once

#include <functional>
#include <string_view>

namespace vn
{

template <typename T>
inline void combine_hash(size_t& seed, T const& v) noexcept
{
  seed ^= std::hash<T>{}(v) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

template <uint32_t N>
inline void combine_hash(size_t& seed, char const (&str)[N]) noexcept
{
  return combine_hash(seed, std::string_view{ str });
}

template <typename... T>
constexpr auto generic_hash(T const&... args) noexcept
{
  auto seed = size_t{};
  (combine_hash(seed, args), ...);
  return seed;
}

}
