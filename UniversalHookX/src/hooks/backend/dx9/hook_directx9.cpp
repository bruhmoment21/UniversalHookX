#include "../../../backend.hpp"
#include "../../../console/console.hpp"

#ifdef ENABLE_BACKEND_DX9
#include <Windows.h>

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include <memory>

#include "hook_directx9.hpp"

#include "../../../dependencies/imgui/imgui_impl_dx9.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../hooks.hpp"

#include "../../../menu/menu.hpp"

static LPDIRECT3D9 g_pD3D = NULL;
static LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;

static void CleanupDeviceD3D9( );
static void RenderImGui_DX9(IDirect3DDevice9* pDevice);

static bool CreateDeviceD3D9(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_pD3D == NULL) {
        LOG("[!] Direct3DCreate9() is failed.\n");
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = { };
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice);
    if (hr != D3D_OK) {
        LOG("[!] CreateDevice() failed. [rv: %lu]\n", hr);
        return false;
    }

    return true;
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)> oReset;
static HRESULT WINAPI hkReset(IDirect3DDevice9* pDevice,
                              D3DPRESENT_PARAMETERS* pPresentationParameters) {
    ImGui_ImplDX9_InvalidateDeviceObjects( );

    return oReset(pDevice, pPresentationParameters);
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*)> oResetEx;
static HRESULT WINAPI hkResetEx(IDirect3DDevice9* pDevice,
                                D3DPRESENT_PARAMETERS* pPresentationParameters,
                                D3DDISPLAYMODEEX* pFullscreenDisplayMode) {
    ImGui_ImplDX9_InvalidateDeviceObjects( );

    return oResetEx(pDevice, pPresentationParameters, pFullscreenDisplayMode);
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*)> oPresent;
static HRESULT WINAPI hkPresent(IDirect3DDevice9* pDevice,
                                const RECT* pSourceRect,
                                const RECT* pDestRect,
                                HWND hDestWindowOverride,
                                const RGNDATA* pDirtyRegion) {
    RenderImGui_DX9(pDevice);

    return oPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD)> oPresentEx;
static HRESULT WINAPI hkPresentEx(IDirect3DDevice9* pDevice,
                                  const RECT* pSourceRect,
                                  const RECT* pDestRect,
                                  HWND hDestWindowOverride,
                                  const RGNDATA* pDirtyRegion,
                                  DWORD dwFlags) {
    RenderImGui_DX9(pDevice);

    return oPresentEx(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

namespace DX9 {
    void Hook(HWND hwnd) {
        if (!CreateDeviceD3D9(GetConsoleWindow( ))) {
            LOG("[!] CreateDeviceD3D9() failed.\n");
            return;
        }

        LOG("[+] DirectX9: g_pD3D: 0x%p\n", g_pD3D);
        LOG("[+] DirectX9: g_pd3dDevice: 0x%p\n", g_pd3dDevice);

        if (g_pd3dDevice) {
            Menu::InitializeContext(hwnd);

            // Hook
            void** pVTable = *reinterpret_cast<void***>(g_pd3dDevice);

            void* fnReset = pVTable[16];
            void* fnResetEx = pVTable[132];

            void* fnPresent = pVTable[17];
            void* fnPresentEx = pVTable[121];

            CleanupDeviceD3D9( );

            static MH_STATUS resetStatus = MH_CreateHook(reinterpret_cast<void**>(fnReset), &hkReset, reinterpret_cast<void**>(&oReset));
            static MH_STATUS resetExStatus = MH_CreateHook(reinterpret_cast<void**>(fnResetEx), &hkResetEx, reinterpret_cast<void**>(&oResetEx));

            static MH_STATUS presentStatus = MH_CreateHook(reinterpret_cast<void**>(fnPresent), &hkPresent, reinterpret_cast<void**>(&oPresent));
            static MH_STATUS presentExStatus = MH_CreateHook(reinterpret_cast<void**>(fnPresentEx), &hkPresentEx, reinterpret_cast<void**>(&oPresentEx));

            MH_EnableHook(fnReset);
            MH_EnableHook(fnResetEx);

            MH_EnableHook(fnPresent);
            MH_EnableHook(fnPresentEx);
        }
    }

    void Unhook( ) {
        if (ImGui::GetCurrentContext( )) {
            if (ImGui::GetIO( ).BackendRendererUserData)
                ImGui_ImplDX9_Shutdown( );

            if (ImGui::GetIO( ).BackendPlatformUserData)
                ImGui_ImplWin32_Shutdown( );

            ImGui::DestroyContext( );
        }
    }
} // namespace DX9

static void CleanupDeviceD3D9( ) {
    if (g_pD3D) {
        g_pD3D->Release( );
        g_pD3D = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release( );
        g_pd3dDevice = NULL;
    }
}

static void RenderImGui_DX9(IDirect3DDevice9* pDevice) {
    if (!ImGui::GetIO( ).BackendRendererUserData) {
        ImGui_ImplDX9_Init(pDevice);
    }

    if (!H::bShuttingDown && ImGui::GetCurrentContext( )) {
        // Fixes menu being too 'white' on games likes 'CS:S'.
        DWORD SRGBWriteEnable;
        pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &SRGBWriteEnable);
        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

        ImGui_ImplDX9_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        Menu::Render( );

        ImGui::EndFrame( );
        if (pDevice->BeginScene( ) == D3D_OK) {
            ImGui::Render( );
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData( ));
            pDevice->EndScene( );
        }

        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, SRGBWriteEnable);
    }
}
#else
#include <Windows.h>
namespace DX9 {
    void Hook(HWND hwnd) { LOG("[!] DirectX9 backend is not enabled!\n"); }
    void Unhook( ) { }
} // namespace DX9
#endif
