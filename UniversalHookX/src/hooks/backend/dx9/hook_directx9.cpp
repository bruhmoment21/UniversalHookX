#include <Windows.h>
#include <d3d9.h>

#include <memory>
#include <thread>

#include "hook_directx9.hpp"

#include "../../../dependencies/imgui/imgui_impl_dx9.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../../console/console.hpp"
#include "../../hooks.hpp"

static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;

static bool CreateDeviceD3D9(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_pD3D == NULL) {
        LOG("[!] g_pD3D is NULL\n");
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};

    // Create the D3DDevice
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    HRESULT result = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice);
    while (result < 0) {
        LOG("[!] CreateDevice() failed. [rv: %lu]\n", result);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        result = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice);
    }

    return true;
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)> oReset;
static HRESULT WINAPI hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    ImGui_ImplDX9_InvalidateDeviceObjects( );
    return oReset(pDevice, pPresentationParameters);
}

static std::add_pointer_t<HRESULT WINAPI(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*)> oPresent;
static HRESULT WINAPI hkPresent(IDirect3DDevice9* pDevice, 
                                const RECT* pSourceRect, 
                                const RECT* pDestRect, 
                                HWND hDestWindowOverride, 
                                const RGNDATA* pDirtyRegion) {    
    if (ImGui::GetCurrentContext( )) {
        static const bool dxInit = ImGui_ImplDX9_Init(pDevice);

        // Fixes menu being too 'white' on games likes 'CS:S'.
        DWORD SRGBWriteEnable;
        pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &SRGBWriteEnable);
        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

        ImGui_ImplDX9_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        if (H::bShowDemoWindow) {
            ImGui::ShowDemoWindow( );
        }

        ImGui::EndFrame( );
        if (pDevice->BeginScene( ) == D3D_OK) {
            ImGui::Render( );
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData( ));
            pDevice->EndScene( );
        }

        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, SRGBWriteEnable);
    }

    return oPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

namespace DX9 {
    void Hook(HWND hwnd) {
        if (!CreateDeviceD3D9(GetConsoleWindow( ))) {
            return;
        }

        LOG("[+] DirectX9: g_pD3D: 0x%p\n", g_pD3D);
        LOG("[+] DirectX9: g_pd3dDevice: 0x%p\n", g_pd3dDevice);

        if (g_pd3dDevice) {
            // Init ImGui
            ImGui::CreateContext( );
            ImGui_ImplWin32_Init(hwnd);

            ImGuiIO& io = ImGui::GetIO( );

            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            // Hook
            void** pVTable = *reinterpret_cast<void***>(g_pd3dDevice);

            void* fnReset = pVTable[16];
            void* fnPresent = pVTable[17];

            g_pd3dDevice->Release( );
            g_pD3D->Release( );

            static MH_STATUS resetStatus = MH_CreateHook(reinterpret_cast<void**>(fnReset), &hkReset, reinterpret_cast<void**>(&oReset));
            static MH_STATUS presentStatus = MH_CreateHook(reinterpret_cast<void**>(fnPresent), &hkPresent, reinterpret_cast<void**>(&oPresent));

            MH_EnableHook(fnReset);
            MH_EnableHook(fnPresent); 
        }
    }

    void Unhook( ) {
        if (ImGui::GetCurrentContext( )) {
            if (ImGui::GetIO( ).BackendRendererUserData)
                ImGui_ImplDX9_Shutdown( );

            ImGui_ImplWin32_Shutdown( );
            ImGui::DestroyContext( );
        }
    }
}
