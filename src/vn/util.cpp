#include "util.hpp"

#include <stdio.h>

namespace vn {

auto read_file(std::string_view path) noexcept -> std::string
{
  FILE* file{};
  fopen_s(&file, path.data(), "rb");
  err_if(!file, "failed to open file {}", path);

  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  rewind(file);

  auto data = std::string{};
  data.resize(size);
  fread(data.data(), 1, size, file);
  fclose(file);

  return data;
}

}
