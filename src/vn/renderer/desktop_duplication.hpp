#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi1_2.h>

namespace vn { namespace renderer {

class DesktopDuplication
{
private:
  DesktopDuplication()                                     = default;
  ~DesktopDuplication()                                    = default;
public:
  DesktopDuplication(DesktopDuplication const&)            = delete;
  DesktopDuplication(DesktopDuplication&&)                 = delete;
  DesktopDuplication& operator=(DesktopDuplication const&) = delete;
  DesktopDuplication& operator=(DesktopDuplication&&)      = delete;

  static auto const instance() noexcept
  {
    static DesktopDuplication instance;
    return &instance;
  }

  void init() noexcept;
  auto capture_backdrop() noexcept -> bool;

public:
  Microsoft::WRL::ComPtr<ID3D11Device>           _device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext>    _device_context;
  Microsoft::WRL::ComPtr<IDXGIFactory2>          _factory;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> _desk_dup;
};

}}