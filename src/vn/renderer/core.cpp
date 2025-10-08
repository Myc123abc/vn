#include "core.hpp"
#include "../util.hpp"

#include <vulkan/vulkan_win32.h>

#include <algorithm>
#include <vector>
#include <ranges>

using namespace vn;
using namespace Microsoft::WRL;

namespace
{

////////////////////////////////////////////////////////////////////////////////
///                          Debug Utils Messenger
////////////////////////////////////////////////////////////////////////////////

VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
  VkDebugUtilsMessageTypeFlagsEXT             message_type,
  VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
  void*                                       user_data) noexcept
{
  if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    warn(callback_data->pMessage);
  else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    error(callback_data->pMessage);
  return false;
}

auto get_debug_utils_messenger_create_info() noexcept
{
  auto create_info = VkDebugUtilsMessengerCreateInfoEXT{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = debug_utils_messenger_callback;
  return create_info;
}

}

namespace vn { namespace renderer {

////////////////////////////////////////////////////////////////////////////////
///                          Extention Functions
////////////////////////////////////////////////////////////////////////////////

PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT{};
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT{};

template <typename T>
void load_instance_ext_func(VkInstance instance, T& func, std::string_view func_name)
{
  func = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, func_name.data()));
  err_if(!func, "failed to load {}", func_name);
}

template <typename T>
inline void load_device_ext_func(VkDevice device, T& func, std::string_view func_name)
{
  func = reinterpret_cast<T>(vkGetDeviceProcAddr(device, func_name.data()));
  err_if(!func, "failed to load {}", func_name);
}

#define VAR_NAME(x) #x
#define load_instance_ext_func(instance, func) load_instance_ext_func(instance, func, VAR_NAME(func))
#define load_device_ext_func(device, func)     load_device_ext_func(device, func, VAR_NAME(func))

void load_instance_extension_funcs(VkInstance instance)
{
#ifndef NDEBUG
  load_instance_ext_func(instance, vkCreateDebugUtilsMessengerEXT);
  load_instance_ext_func(instance, vkDestroyDebugUtilsMessengerEXT);
#endif
}

void load_device_extension_funcs(VkDevice device)
{

}

}}

namespace vn { namespace renderer {

////////////////////////////////////////////////////////////////////////////////
///                                Core
////////////////////////////////////////////////////////////////////////////////

void Core::init() noexcept
{
  //
  // create instance, load instance extensions, create debug utils messenger
  //

  auto instance_create_info = VkInstanceCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

  // debug support
#ifndef NDEBUG
  // debug utils messenger
  auto debug_utils_messenger_create_info = get_debug_utils_messenger_create_info();
  instance_create_info.pNext = &debug_utils_messenger_create_info;

  // validation layer
  auto layer = "VK_LAYER_KHRONOS_validation";
  instance_create_info.enabledLayerCount   = 1;
  instance_create_info.ppEnabledLayerNames = &layer;
#endif

  // instance extentions
  auto extentions = std::vector<char const*>
  {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
  };
  instance_create_info.enabledExtensionCount   = extentions.size();
  instance_create_info.ppEnabledExtensionNames = extentions.data();
  
  // application information
  auto app_info = VkApplicationInfo{};
  app_info.pApplicationName = "vn";
  app_info.apiVersion       = VK_API_VERSION_1_4;
  instance_create_info.pApplicationInfo = &app_info;

  // create instance
  err_if(vkCreateInstance(&instance_create_info, nullptr, &_instance), "failed to create vulkan instance");

  // load instance extensions
  load_instance_extension_funcs(_instance);

  // create debug utils messenger
#ifndef NDEBUG
  err_if(vkCreateDebugUtilsMessengerEXT(_instance, &debug_utils_messenger_create_info, nullptr, &_debug_utils_messenger) != VK_SUCCESS,
          "failed to create debug utils messenger extension");
#endif

  // 
  // get physical device
  //

  auto count = uint32_t{};
  err_if(vkEnumeratePhysicalDevices(_instance, &count, nullptr), "failed to enumerate physical device");
  auto physical_devices = std::vector<VkPhysicalDevice>{ count };
  err_if(vkEnumeratePhysicalDevices(_instance, &count, physical_devices.data()), "failed to enumerate physical device");

  for (auto const& pd : physical_devices)
  {
    auto properties = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(pd, &properties);
    if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      _physical_device = pd;
  }
  if (!_physical_device)
    _physical_device = physical_devices[0];

  //
  // create device, load device extentions, get queue
  //

  // enumerate queue family properties
  vkGetPhysicalDeviceQueueFamilyProperties2(_physical_device, &count, nullptr);
  auto queue_family_properties = std::vector<VkQueueFamilyProperties2>{ count, { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 } };
  vkGetPhysicalDeviceQueueFamilyProperties2(_physical_device, &count, queue_family_properties.data());
  
  // get graphics queue family index
  auto graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
  for (auto const& [idx, properties] : std::views::enumerate(queue_family_properties))
    if (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      graphics_queue_family_index = idx;
  err_if(graphics_queue_family_index == std::numeric_limits<uint32_t>::max(), "failed to find graphics queue");
  
  // set queue create info
  auto queue_priority    = 1.f;
  auto queue_create_info = VkDeviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
  queue_create_info.queueCount       = 1;
  queue_create_info.queueFamilyIndex = graphics_queue_family_index;
  queue_create_info.pQueuePriorities = &queue_priority;

  // set device extensions
  extentions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME };

  // create device
  auto device_create_info = VkDeviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  device_create_info.queueCreateInfoCount    = 1;
  device_create_info.pQueueCreateInfos       = &queue_create_info;
  device_create_info.enabledExtensionCount   = extentions.size();
  device_create_info.ppEnabledExtensionNames = extentions.data();

  auto features12 = VkPhysicalDeviceVulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
  auto features13 = VkPhysicalDeviceVulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
  features12.pNext = &features13;
  features12.bufferDeviceAddress = true;
  features13.dynamicRendering    = true;
  features13.synchronization2    = true;
  device_create_info.pNext = &features12;
  
  err_if(vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device) != VK_SUCCESS,
         "failed to create device");

  // load device extentions
  load_device_extension_funcs(_device);

  // get queue
  auto queue_info = VkDeviceQueueInfo2{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
  queue_info.queueFamilyIndex = graphics_queue_family_index;
  queue_info.queueIndex       = 0;
  vkGetDeviceQueue2(_device, &queue_info, &_queue);

  //
  // create command pool
  //

  auto command_pool_create_info = VkCommandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  command_pool_create_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_create_info.queueFamilyIndex = graphics_queue_family_index;
  err_if(vkCreateCommandPool(_device, &command_pool_create_info, nullptr, &_command_pool) != VK_SUCCESS,
          "failed to create command pool");

  // create queue fences
  auto fence_create_info = VkFenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
  std::ranges::for_each(_queue_fences, [&](auto& fence)
  { 
    err_if(vkCreateFence(_device, &fence_create_info, nullptr, &fence),
            "failed to create semaphore");
  });

  //
  // create vma allocator
  //

  auto vma_allocator_create_info = VmaAllocatorCreateInfo{};
  vma_allocator_create_info.flags            = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT   |
                                               VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT     |
                                               VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT;
  vma_allocator_create_info.physicalDevice   = _physical_device;
  vma_allocator_create_info.device           = _device;
  vma_allocator_create_info.instance         = _instance;
  vma_allocator_create_info.vulkanApiVersion = app_info.apiVersion;
  err_if(vmaCreateAllocator(&vma_allocator_create_info, &_vma_allocator) != VK_SUCCESS,
          "failed to create Vulkan Memory Allocator");

  create_dx12_resource();
}

void Core::create_dx12_resource() noexcept
{
  // init debug controller
#ifndef NDEBUG
  auto debug_controller = ComPtr<ID3D12Debug>{};
  err_if(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)),
          "failed to create d3d12 debug controller");
  debug_controller->EnableDebugLayer();
#endif

  // create factory
#ifndef NDEBUG
  err_if(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#else
  err_if(CreateDXGIFactory2(0, IID_PPV_ARGS(&_factory)),
          "failed to create dxgi factory");
#endif

  // create device
  auto adapter = ComPtr<IDXGIAdapter4>{};
  err_if(_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)),
          "failed to enum dxgi adapter");
  err_if(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_dxgi_device)),
          "failed to create d3d12 device");

  // create command queue
  auto queue_desc = D3D12_COMMAND_QUEUE_DESC{};
  err_if(_dxgi_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&_command_queue)),
          "failed to create command queue");

  // create fence resources
  err_if(_dxgi_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)),
          "failed to create fence");
  _fence_event = CreateEvent(nullptr, false, false, nullptr);
  err_if(!_fence_event, "failed to create fence event");
  for (auto& fence_value : _fence_values)
    fence_value = 1;
}

void Core::destroy() const noexcept
{
  CloseHandle(_fence_event);
  std::ranges::for_each(_queue_fences, [&](auto fence) { vkDestroyFence(_device, fence, nullptr); });
  vmaDestroyAllocator(_vma_allocator);
  vkDestroyCommandPool(_device, _command_pool, nullptr);
  vkDestroyDevice(_device, nullptr);
#ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT(_instance, _debug_utils_messenger, nullptr);
#endif
  vkDestroyInstance(_instance, nullptr);
}

void Core::wait_gpu_complete() noexcept
{
  vkDeviceWaitIdle(_device);
  
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value), "failed to signal fence");

  if (_fence->GetCompletedValue() < fence_value)
  {
    // wait until frame is finished
    err_if(_fence->SetEventOnCompletion(fence_value, _fence_event), "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // advance frame
  _frame_index = ++_frame_index % Frame_Count;
  // advance fence
  _fence_values[_frame_index] = fence_value + 1;
}

void Core::move_to_next_frame() noexcept
{
  // get current fence value
  auto const fence_value = _fence_values[_frame_index];

  // signal fence
  err_if(_command_queue->Signal(_fence.Get(), fence_value),
          "failed to signal fence");
      
  // move to next frame
  _frame_index = ++_frame_index % Frame_Count;

  // wait if next fence not ready
  if (_fence->GetCompletedValue() < _fence_values[_frame_index])
  {
    err_if(_fence->SetEventOnCompletion(_fence_values[_frame_index], _fence_event),
            "failed to set event on completion");
    WaitForSingleObjectEx(_fence_event, INFINITE, false);
  }

  // update the next frame fence value
  _fence_values[_frame_index] = fence_value + 1;
}

auto Core::current_frame_available() const noexcept -> bool
{
  // wait current frame commands on the queue is complete
  if (vkWaitForFences(_device, 1, &_queue_fences[_frame_index], false, 0)) return false;
  err_if(vkResetFences(_device, 1, &_queue_fences[_frame_index]), "failed to reset fence");
  return true;
}

}}