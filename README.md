# UniversalHookX ![C++](https://img.shields.io/badge/language-C%2B%2B-%23f34b7d.svg) ![Windows](https://img.shields.io/badge/platform-Windows-0078d7.svg)
Universal graphical hook for Windows apps that will display an [ImGui Demo Window](https://github.com/bruhmoment21/UniversalHookX/blob/8bb97657c53a802d7db20feec65cd43ed8bfe0c8/UniversalHookX/src/dependencies/imgui/imgui_demo.cpp#L266) as an example.

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

# Media
## DirectX9 32bit
![image](https://user-images.githubusercontent.com/53657322/173915161-0c683d0f-7a50-4272-ad4d-3b4e1aaa7939.png)
![image](https://user-images.githubusercontent.com/53657322/173915463-4d19c09c-ab47-443c-9efa-2af49decd3aa.png)
## DirectX11 64bit
![image](https://user-images.githubusercontent.com/53657322/173915680-598f9a9c-9f63-457d-a9dd-ee5b04da1f31.png)
## DirectX12 64bit
![image](https://user-images.githubusercontent.com/53657322/173922887-f56629da-58bd-4ce6-b818-211c74cad6ab.png)

# Other
Feel free to open an issue if something isn't working, personally I couldn't get it to work on 'Far Cry 6'. **Resizing** works because we have [ResizeBuffers](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers) hooked. Input handling is up to you to decide how to make/use it. The WndProc hook is [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/hooks.cpp#L16). It should support both 64bit and 32bit architectures.

## Dependencies
[MinHook](https://github.com/TsudaKageyu/minhook) - TsudaKageyu - Used for hooking (trampoline method).  
[ImGui](https://github.com/ocornut/imgui) - ocornut - self-explanatory.