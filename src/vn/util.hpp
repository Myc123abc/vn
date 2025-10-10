#pragma once

#include <windows.h>

#include <print>
#include <string_view>

namespace vn {

#define ConsoleColor_Red(x)    "\033[31m"       x "\033[0m"
#define ConsoleColor_Green(x)  "\033[32m"       x "\033[0m"
#define ConsoleColor_Yellow(x) "\033[33m"       x "\033[0m"
#define ConsoleColor_Orange(x) "\033[38;5;208m" x "\033[0m"
#define ConsoleColor_Blue(x)   "\033[34m"       x "\033[0m"

inline void error(std::string_view msg) noexcept
{
  std::println(stderr, ConsoleColor_Red("[error] {}"), msg);
}

template <typename... T>
inline void error(std::format_string<T...> const fmt, T&&... args) noexcept
{
  std::println(stderr, ConsoleColor_Red("[error] {}"), std::format(fmt, std::forward<T>(args)...));
}

inline void info(std::string_view msg) noexcept
{
  std::println(stderr, ConsoleColor_Green("[info]  {}"), msg);
}

template <typename... T>
inline void info(std::format_string<T...> const fmt, T&&... args) noexcept
{
  std::println(stderr, ConsoleColor_Green("[info]  {}"), std::format(fmt, std::forward<T>(args)...));
}

inline void warn(std::string_view msg) noexcept
{
  std::println(stderr, ConsoleColor_Orange("[warn]  {}"), msg);
}

template <typename... T>
inline void warn(std::format_string<T...> const fmt, T&&... args) noexcept
{
  std::println(stderr, ConsoleColor_Orange("[warn]  {}"), std::format(fmt, std::forward<T>(args)...));
}

inline void debug(std::string_view msg) noexcept
{
#ifndef NDEBUG
  std::println(stderr, ConsoleColor_Blue("[debug] {}"), msg);
#endif
}

template <typename... T>
inline void debug(std::format_string<T...> const fmt, T&&... args) noexcept
{
#ifndef NDEBUG
  std::println(stderr, ConsoleColor_Blue("[debug] {}"), std::format(fmt, std::forward<T>(args)...));
#endif
}

inline void err_if(bool b, std::string_view msg) noexcept
{
  if (b)
  {
    error(msg);
    exit(EXIT_FAILURE);
  }
}

template <typename... T>
inline void err_if(bool b, std::format_string<T...> const fmt, T&&... args) noexcept
{
  if (b)
  {
    error(fmt, std::forward<T>(args)...);
    exit(EXIT_FAILURE);
  }
}

inline void err_if(HRESULT hr, std::string_view msg) noexcept
{
  err_if(FAILED(hr), msg);
}

template <typename... T>
inline void err_if(HRESULT hr, std::format_string<T...> const fmt, T&&... args) noexcept
{
  err_if(FAILED(hr), fmt, std::forward<T>(args)...);
}

auto read_file(std::string_view path) noexcept -> std::string;

}