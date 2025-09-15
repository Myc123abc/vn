cmake_minimum_required(VERSION 3.30)
project(vn)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################################################################################
###                                Dependencies
################################################################################

add_subdirectory(vendor/DirectX-Headers)

################################################################################
###                                 test exe
################################################################################

file(GLOB_RECURSE SRC src/*.cpp)

add_executable(test ${SRC})
target_compile_definitions(test PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_link_libraries(test PRIVATE
  d3d12 dxgi d3dcompiler dcomp
  DirectX-Headers
)

################################################################################
###                                 Shaders
################################################################################

set(SHADER     ${CMAKE_SOURCE_DIR}/assets/shader.hlsl)
set(VERTEX_CSO ${CMAKE_BINARY_DIR}/assets/vertex.cso)
set(PIXEL_CSO  ${CMAKE_BINARY_DIR}/assets/pixel.cso)

add_custom_command(
  OUTPUT ${VERTEX_CSO}
  COMMAND dxc -T vs_6_0 -E VSMain -Fo ${VERTEX_CSO} ${SHADER}
  DEPENDS ${SHADER}
)
add_custom_command(
  OUTPUT ${PIXEL_CSO}
  COMMAND dxc -T ps_6_0 -E PSMain -Fo ${PIXEL_CSO} ${SHADER}
  DEPENDS ${SHADER}
)
add_custom_target(Shaders ALL
  DEPENDS
    ${VERTEX_CSO}
    ${PIXEL_CSO}
)

# TODO: use for copy other assets like fonts in the future
# file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})