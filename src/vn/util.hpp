#pragma once

#include <print>

#include <windows.h>

namespace vn {

template <typename... T>
inline void exit_if(bool b, std::format_string<T...> const fmt, T&&... args)
{
  if (b)
  {
    std::println(stderr, fmt, std::forward<T>(args)...);
    exit(EXIT_FAILURE);
  }
}

template <typename... T>
inline void exit_if(HRESULT hr, std::format_string<T...> const fmt, T&&... args)
{
  exit_if(FAILED(hr), fmt, std::forward<T>(args)...);
}

}