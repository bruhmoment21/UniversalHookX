# UniversalHookX ![C++](https://img.shields.io/badge/language-C%2B%2B-%23f34b7d.svg) ![Windows](https://img.shields.io/badge/platform-Windows-0078d7.svg)
Universal graphical hook for Windows apps that will display and ImGui Demo Window as an example.

## Usage
Call `Utils::SetRenderingBackground(eRenderingBackground);` in DllMain as shown [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/dllmain.cpp#L32).
You must do this or the DLL won't know what to hook or how to hook. [What is eRenderingBackend?](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/utils/utils.hpp#L3-L10).

## Purpose
The purpose of this library is to show how to hook different backends to display ImGui, the code should be easy to understand as I tried to make everything seem almost the same, see [backends folder](https://github.com/bruhmoment21/UniversalHookX/tree/main/UniversalHookX/src/hooks/backend).

## Supported Backends
- [x] DirectX9
- [x] DirectX10
- [x] DirectX11
- [x] DirectX12
- [ ] OpenGL
- [ ] Vulkan

# How it works
## DirectX
We create a 'dummy device' DirectX with a handle to the [console window](https://docs.microsoft.com/en-us/windows/console/getconsolewindow). See the `CreateDeviceD3DX` function in every DirectX backend. [See DX12 example 'CreateDeviceD3D12'](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L32-L69). The point is to get a pointer to the vTable to get the required functions addresses named respectively. We release it right after getting the pointers because we won't use our 'dummy device' for drawing. [Code used in DX12 backend hook](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L223-L239).

# Other
Feel free to open an issue if something isn't working, personally I couldn't get it to work on 'Far Cry 6'. **Resizing** works because we have [ResizeBuffers](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers) hooked. Input handling is up to you to decide how to make/use it. The WndProc hook is [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/hooks.cpp#L16).

## Dependencies
[MinHook](https://github.com/TsudaKageyu/minhook) - TsudaKageyu - Used for hooking using (trampoline method).  
[ImGui](https://github.com/ocornut/imgui) - ocornut - self-explanatory.
