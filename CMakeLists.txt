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
###                                 test exe
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB_RECURSE SRC src/*.cpp)

add_executable(test ${SRC})
target_compile_definitions(test PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UTF_CPP_CPLUSPLUS=202002L
)
target_link_libraries(test PRIVATE
  d3d12 dxgi dxcompiler dcomp
  DirectX-Headers
)
target_include_directories(test PRIVATE
  vendor/glm
  vendor/utfcpp/source
)

################################################################################
###                                 Assets
################################################################################

add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)