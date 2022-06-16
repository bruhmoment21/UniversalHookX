# UniversalHookX ![C++](https://img.shields.io/badge/language-C%2B%2B-%23f34b7d.svg) ![Windows](https://img.shields.io/badge/platform-Windows-0078d7.svg)
Universal graphical hook for Windows apps that will display an [ImGui Demo Window](https://github.com/bruhmoment21/UniversalHookX/blob/8bb97657c53a802d7db20feec65cd43ed8bfe0c8/UniversalHookX/src/dependencies/imgui/imgui_demo.cpp#L266) as an example.

## Usage
Call `Utils::SetRenderingBackground(eRenderingBackground)` in DllMain as shown [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/dllmain.cpp#L19).
You must do this or the DLL won't know what to hook or how to hook. [What is eRenderingBackend?](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/utils/utils.hpp#L3).

## Purpose
The purpose of this library is to show how to hook different backends to display ImGui, the code should be easy to understand as I tried to make everything seem almost the same, see [backends folder](https://github.com/bruhmoment21/UniversalHookX/tree/main/UniversalHookX/src/hooks/backend).

## Supported Backends
- [x] DirectX9
- [x] DirectX10
- [x] DirectX11
- [x] DirectX12
- [x] OpenGL
- [ ] Vulkan

# How it works
## DirectX
We create a 'dummy device' and a 'dummy swapchain' (for DirectX10 and higher) with a handle to the [console window](https://docs.microsoft.com/en-us/windows/console/getconsolewindow). See the `CreateDeviceD3DX` function in every DirectX backend. [See DX12 example 'CreateDeviceD3D12'](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L32-L69). The point is to get a pointer to the vTable to get the required functions addresses. We release it right after getting the pointers because we won't use our 'dummy device' and 'dummy swapchain' for drawing. [Code used in DX12 backend hook](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L218-L234).
## OpenGL
We hook wglSwapBuffers which is an exported function in opengl32.dll. [See code used in OpenGL backend](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/opengl/hook_opengl.cpp#L37)
# Media
## DirectX9 32bit
![image](https://user-images.githubusercontent.com/53657322/173915161-0c683d0f-7a50-4272-ad4d-3b4e1aaa7939.png)
![image](https://user-images.githubusercontent.com/53657322/173915463-4d19c09c-ab47-443c-9efa-2af49decd3aa.png)
## DirectX10 32bit
![image](https://user-images.githubusercontent.com/53657322/173996412-d842d04c-6ed9-4cd8-87b3-e83fca8dfabe.png)
## DirectX11 64bit
![image](https://user-images.githubusercontent.com/53657322/173915680-598f9a9c-9f63-457d-a9dd-ee5b04da1f31.png)
## DirectX12 64bit
![image](https://user-images.githubusercontent.com/53657322/173922887-f56629da-58bd-4ce6-b818-211c74cad6ab.png)
## OpenGL 32bit + 64bit
![image](https://user-images.githubusercontent.com/53657322/174028360-a59d71e8-de1a-4e79-8df4-8dd40b10775e.png)
![image](https://user-images.githubusercontent.com/53657322/174029463-a7e24813-850b-4261-86b7-4c26fb18a34b.png)

# Other
Feel free to open an issue if something isn't working, personally I couldn't get it to work on 'Far Cry 6'. **Resizing** works because [ResizeBuffers](https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers) is hooked. Input handling is up to you to decide how to make/use it. The WndProc hook is [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/hooks.cpp#L36). It should support both 64bit and 32bit architectures.

## Known issues
Try pressing [HOME](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/hooks.cpp#L41-L44) to rehook and see if things got better.  
[Minecraft (tested on 1.19) - menu textures glitched](https://user-images.githubusercontent.com/53657322/174030423-aa92e780-057e-451d-9d60-ddd20f668d03.png)

## Dependencies
[MinHook](https://github.com/TsudaKageyu/minhook) - TsudaKageyu - Used for hooking (trampoline method).  
[ImGui](https://github.com/ocornut/imgui) - ocornut - self-explanatory.
