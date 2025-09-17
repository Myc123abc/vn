#include "desktop_duplication.hpp"
#include "../util.hpp"
#include "renderer.hpp"

#include <d3d12.h>

using namespace Microsoft::WRL;

namespace vn { namespace renderer {

void DesktopDuplication::init() noexcept
{
  // init d3d11 device
  D3D_FEATURE_LEVEL feature_levels[]
  {
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_1 
  };
  D3D_FEATURE_LEVEL feature_level{};
  err_if(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &_device, &feature_level, &_device_context),
          "failed to create d3d11 device");
  
  // get dxgi device
  ComPtr<IDXGIDevice> dxgI_device;
  err_if(_device.As(&dxgI_device), "failed to get dxgi device");

  // get adapter
  ComPtr<IDXGIAdapter> adapter;
  err_if(dxgI_device->GetParent(IID_PPV_ARGS(&adapter)), "failed to get adapter from dxgi device");

  // get factory
  err_if(adapter->GetParent(IID_PPV_ARGS(&_factory)), "failed to get factory from dxgi adapter");

  // TODO: can get multiple monitors here
  // get dxgi ouput
  ComPtr<IDXGIOutput> output;
  err_if(adapter->EnumOutputs(0, &output), "failed to get dxgi output");

  // get dxgi output1
  ComPtr<IDXGIOutput1> output1;
  err_if(output.As(&output1), "failed to get dxgi output1");

  // create desktop duplicaiton
  err_if(output1->DuplicateOutput(_device.Get(), &_desk_dup), "failed to get desktop duplication");

  capture_backdrop();
}

auto DesktopDuplication::capture_backdrop() noexcept -> bool
{
  ComPtr<IDXGIResource>   desktop_resource;
  DXGI_OUTDUPL_FRAME_INFO frame_info{};

  // get new frame
  auto res = _desk_dup->AcquireNextFrame(500, &frame_info, &desktop_resource);
  if (res == DXGI_ERROR_WAIT_TIMEOUT)
    return false;
  err_if(res, "failed to capture backdrop");

  ComPtr<ID3D11Texture2D> texture;
  err_if(desktop_resource.As(&texture), "failed to get d3d11 texture");

  D3D11_TEXTURE2D_DESC texture_desc{};
  texture->GetDesc(&texture_desc);

  ComPtr<IDXGIResource1> texture_dxgi_resource;
  err_if(texture.As(&texture_dxgi_resource), "failed to conver to dxgi resource");

  HANDLE handle{};
  err_if(texture_dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle),
          "failed to create shared handle");

  ComPtr<ID3D12Resource> d3d12_resource;
  err_if(Renderer::instance()->_device->OpenSharedHandle(handle, IID_PPV_ARGS(&d3d12_resource)), "failed to share d3d11 texture");

  _desk_dup->ReleaseFrame();

  return true;
}

}}