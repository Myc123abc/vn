cmake_minimum_required(VERSION 3.30)
project(vn)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(vendor/DirectX-Headers)

file(GLOB_RECURSE SRC src/*.cpp)

add_executable(test ${SRC})
target_compile_definitions(test PRIVATE
  WIN32_LEAN_AND_MEAN
)
target_link_libraries(test PRIVATE
  d3d12 dxgi d3dcompiler
  DirectX-Headers
)