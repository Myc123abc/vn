#pragma once

#include <dxgi1_6.h>

#include <vulkan/vulkan.h>

namespace vn { namespace renderer {

constexpr auto Frame_Count                   = 2u;
constexpr auto Swapchain_Image_Format_Vulkan = VK_FORMAT_B8G8R8A8_UNORM;
constexpr auto Swapchain_Image_Format_DXGI   = DXGI_FORMAT_B8G8R8A8_UNORM;

}}