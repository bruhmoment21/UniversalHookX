# UniversalHookX ![C++](https://img.shields.io/badge/language-C%2B%2B-%23f34b7d.svg) ![Windows](https://img.shields.io/badge/platform-Windows-0078d7.svg)
Universal graphical hook for Windows apps, although it should work on [Linux](https://user-images.githubusercontent.com/53657322/176435063-c1511ee4-462c-47f2-9f3a-1cc983c73310.png) too if you implement it correctly, that will display an [ImGui Demo Window](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/menu/menu.cpp#L24) as an example.

## Usage
Call `Utils::SetRenderingBackend(eRenderingBackend)` in DllMain as shown [here](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/dllmain.cpp#L19).
You must do this or the DLL won't know what to hook or how to hook. [What is eRenderingBackend?](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/utils/utils.hpp#L3). To enable or disable a specific backend comment backends in [backend.hpp](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/backend.hpp)

## Purpose
The purpose of this library is to show how to hook different backends to display ImGui, the code should be easy to understand as I tried to make everything seem almost the same, see [backends folder](https://github.com/bruhmoment21/UniversalHookX/tree/main/UniversalHookX/src/hooks/backend).

## Supported Backends
- [x] DirectX9 + DirectX9Ex
- [x] DirectX10
- [x] DirectX11
- [x] DirectX12
- [x] OpenGL
- [x] Vulkan

# How it works
## DirectX
We create a 'dummy device' and a 'dummy swapchain' (for DirectX10 and higher) with a handle to the [console window](https://docs.microsoft.com/en-us/windows/console/getconsolewindow). See the `CreateDeviceD3DX` function in every DirectX backend. [See DX12 example 'CreateDeviceD3D12'](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L43-L72). The point is to get a pointer to the vTable to get the required functions addresses. We release it right after getting the pointers because we won't use our 'dummy device' and 'dummy swapchain' for drawing. [See code used in DX12 backend hook.](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/dx12/hook_directx12.cpp#L195-L259)
## OpenGL
We hook wglSwapBuffers which is an exported function in opengl32.dll. [See code used in OpenGL backend hook.](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/opengl/hook_opengl.cpp#L39-L56)
## Vulkan
We create a 'dummy device' to get the required functions addresses. The point is to hook into [vkQueuePresentKHR](https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkQueuePresentKHR.html) where we will submit the queue with our data from our command pool and our command buffer. [See code used in Vulkan backend hook.](https://github.com/bruhmoment21/UniversalHookX/blob/main/UniversalHookX/src/hooks/backend/vulkan/hook_vulkan.cpp#L314-L349)

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
## Vulkan 32bit + 64bit
![image](https://user-images.githubusercontent.com/53657322/175804247-681dd228-5d18-462a-82e4-bd9eab90bdcb.png)
![image](https://user-images.githubusercontent.com/53657322/176169557-d278097a-2e1e-40a1-ac07-2d87865ab363.png)

# Other
Feel free to open an issue if something isn't working. Input handling is left for the user because there is no general solution.

## Known issues
Tip: Try pressing the "Home" key, that will rehook graphics, and see if anything changed.
- **[!] Conflicts with other overlays such as MSI Afterburner and will probably crash.**
- [Vulkan] Games or apps that present from a queue that doesn't support GRAPHICS_BIT may have issues such as artifacts and menu jittering. (Example: DOOM Eternal)
- ~~[Vulkan] Portal 2 will crash if injected while Valve intro is playing.~~
- [OpenGL] [Minecraft (tested on 1.19) - ui textures glitched sometimes.](https://user-images.githubusercontent.com/53657322/174030423-aa92e780-057e-451d-9d60-ddd20f668d03.png)

## Dependencies
[MinHook](https://github.com/TsudaKageyu/minhook) - TsudaKageyu - Used for hooking (trampoline method).  
[ImGui](https://github.com/ocornut/imgui) - ocornut - self-explanatory.  
[Vulkan SDK](https://vulkan.lunarg.com/) - for the Vulkan API.
