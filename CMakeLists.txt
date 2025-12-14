cmake_minimum_required(VERSION 3.30)
project(vn)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################################################################################
###                                Dependencies
################################################################################

add_subdirectory(vendor/DirectX-Headers)
add_subdirectory(vendor/glm)

################################################################################
###                                  Library
################################################################################

file(GLOB_RECURSE SRC src/*.cpp)

add_library(vn ${SRC})
target_compile_definitions(vn
PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UTF_CPP_CPLUSPLUS=202002L
)
target_link_libraries(vn
PRIVATE
  d3d12 dxgi dxcompiler dcomp dxguid dwmapi
  DirectX-Headers
)
target_include_directories(vn
PRIVATE
  vendor/utfcpp/source
  vendor/stb
  include/vn
PUBLIC
  vendor/glm
  include
)

################################################################################
###                                 Assets
################################################################################

add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          ${CMAKE_BINARY_DIR}/assets
)

################################################################################
###                                  Test
################################################################################

add_executable(test test/main.cpp)
target_link_libraries(test PRIVATE vn)
