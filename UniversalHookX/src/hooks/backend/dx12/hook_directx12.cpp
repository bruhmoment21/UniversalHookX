#include "../../../backend.hpp"
#include "../../../console/console.hpp"

#ifdef ENABLE_BACKEND_DX12
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include <memory>

#include "hook_directx12.hpp"

#include "../../../dependencies/imgui/imgui_impl_dx12.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../../utils/utils.hpp"
#include "../../hooks.hpp"

#include "../../../menu/menu.hpp"

// Data
static int const NUM_BACK_BUFFERS = 3;
static IDXGIFactory4* g_dxgiFactory = NULL;
static ID3D12Device* g_pd3dDevice = NULL;
static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = NULL;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
static ID3D12CommandQueue* g_pd3dCommandQueue = NULL;
static ID3D12GraphicsCommandList* g_pd3dCommandList = NULL;
static IDXGISwapChain3* g_pSwapChain = NULL;
static ID3D12CommandAllocator* g_commandAllocators[NUM_BACK_BUFFERS] = { };
static ID3D12Resource* g_mainRenderTargetResource[NUM_BACK_BUFFERS] = { };
static D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = { };

static void CleanupDeviceD3D12( );
static void CleanupRenderTarget( );
static void RenderImGui_DX12(IDXGISwapChain3* pSwapChain);

static bool CreateDeviceD3D12(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd = { };
    sd.BufferCount = NUM_BACK_BUFFERS;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    D3D12_COMMAND_QUEUE_DESC desc = { };
    if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
        return false;

    IDXGISwapChain1* swapChain1 = NULL;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&g_dxgiFactory)) != S_OK)
        return false;
    if (g_dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
        return false;
    if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
        return false;
    swapChain1->Release( );

    return true;
}

static void CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i) {
        ID3D12Resource* pBackBuffer = NULL;
        pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer) {
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);

            D3D12_RENDER_TARGET_VIEW_DESC desc = { };
            desc.Format = static_cast<DXGI_FORMAT>(Utils::GetCorrectDXGIFormat(sd.BufferDesc.Format));
            desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &desc, g_mainRenderTargetDescriptor[i]);
            g_mainRenderTargetResource[i] = pBackBuffer;
        }
    }
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain3*, UINT, UINT)> oPresent;
static HRESULT WINAPI hkPresent(IDXGISwapChain3* pSwapChain,
                                UINT SyncInterval,
                                UINT Flags) {
    RenderImGui_DX12(pSwapChain);

    return oPresent(pSwapChain, SyncInterval, Flags);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain3*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*)> oPresent1;
static HRESULT WINAPI hkPresent1(IDXGISwapChain3* pSwapChain,
                                 UINT SyncInterval,
                                 UINT PresentFlags,
                                 const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    RenderImGui_DX12(pSwapChain);

    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)> oResizeBuffers;
static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* pSwapChain,
                                      UINT BufferCount,
                                      UINT Width,
                                      UINT Height,
                                      DXGI_FORMAT NewFormat,
                                      UINT SwapChainFlags) {
    CleanupRenderTarget( );

    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*)> oResizeBuffers1;
static HRESULT WINAPI hkResizeBuffers1(IDXGISwapChain3* pSwapChain,
                                       UINT BufferCount,
                                       UINT Width,
                                       UINT Height,
                                       DXGI_FORMAT NewFormat,
                                       UINT SwapChainFlags,
                                       const UINT* pCreationNodeMask,
                                       IUnknown* const* ppPresentQueue) {
    CleanupRenderTarget( );

    return oResizeBuffers1(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
}

static std::add_pointer_t<void WINAPI(ID3D12CommandQueue*, UINT, ID3D12CommandList*)> oExecuteCommandLists;
static void WINAPI hkExecuteCommandLists(ID3D12CommandQueue* pCommandQueue,
                                         UINT NumCommandLists,
                                         ID3D12CommandList* ppCommandLists) {
    if (!g_pd3dCommandQueue) {
        g_pd3dCommandQueue = pCommandQueue;
    }

    return oExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**)> oCreateSwapChain;
static HRESULT WINAPI hkCreateSwapChain(IDXGIFactory* pFactory,
                                        IUnknown* pDevice,
                                        DXGI_SWAP_CHAIN_DESC* pDesc,
                                        IDXGISwapChain** ppSwapChain) {
    CleanupRenderTarget( );

    return oCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGIFactory*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**)> oCreateSwapChainForHwnd;
static HRESULT WINAPI hkCreateSwapChainForHwnd(IDXGIFactory* pFactory,
                                               IUnknown* pDevice,
                                               HWND hWnd,
                                               const DXGI_SWAP_CHAIN_DESC1* pDesc,
                                               const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                               IDXGIOutput* pRestrictToOutput,
                                               IDXGISwapChain1** ppSwapChain) {
    CleanupRenderTarget( );

    return oCreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGIFactory*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**)> oCreateSwapChainForCoreWindow;
static HRESULT WINAPI hkCreateSwapChainForCoreWindow(IDXGIFactory* pFactory,
                                                     IUnknown* pDevice,
                                                     IUnknown* pWindow,
                                                     const DXGI_SWAP_CHAIN_DESC1* pDesc,
                                                     IDXGIOutput* pRestrictToOutput,
                                                     IDXGISwapChain1** ppSwapChain) {
    CleanupRenderTarget( );

    return oCreateSwapChainForCoreWindow(pFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGIFactory*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**)> oCreateSwapChainForComposition;
static HRESULT WINAPI hkCreateSwapChainForComposition(IDXGIFactory* pFactory,
                                                      IUnknown* pDevice,
                                                      const DXGI_SWAP_CHAIN_DESC1* pDesc,
                                                      IDXGIOutput* pRestrictToOutput,
                                                      IDXGISwapChain1** ppSwapChain) {
    CleanupRenderTarget( );

    return oCreateSwapChainForComposition(pFactory, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
}

namespace DX12 {
    void Hook(HWND hwnd) {
        if (!CreateDeviceD3D12(GetConsoleWindow( ))) {
            LOG("[!] CreateDeviceD3D12() failed.\n");
            return;
        }

        LOG("[+] DirectX12: g_pd3dDevice: 0x%p\n", g_pd3dDevice);
        LOG("[+] DirectX12: g_dxgiFactory: 0x%p\n", g_dxgiFactory);
        LOG("[+] DirectX12: g_pd3dCommandQueue: 0x%p\n", g_pd3dCommandQueue);
        LOG("[+] DirectX12: g_pSwapChain: 0x%p\n", g_pSwapChain);

        if (g_pd3dDevice) {
            Menu::InitializeContext(hwnd);

            // Hook
            void** pVTable = *reinterpret_cast<void***>(g_pSwapChain);
            void** pCommandQueueVTable = *reinterpret_cast<void***>(g_pd3dCommandQueue);
            void** pFactoryVTable = *reinterpret_cast<void***>(g_dxgiFactory);

            void* fnCreateSwapChain = pFactoryVTable[10];
            void* fnCreateSwapChainForHwndChain = pFactoryVTable[15];
            void* fnCreateSwapChainForCWindowChain = pFactoryVTable[16];
            void* fnCreateSwapChainForCompChain = pFactoryVTable[24];

            void* fnPresent = pVTable[8];
            void* fnPresent1 = pVTable[22];

            void* fnResizeBuffers = pVTable[13];
            void* fnResizeBuffers1 = pVTable[39];

            void* fnExecuteCommandLists = pCommandQueueVTable[10];

            if (g_pd3dCommandQueue) {
                g_pd3dCommandQueue->Release( );
                g_pd3dCommandQueue = NULL;
            }
            CleanupDeviceD3D12( );

            static MH_STATUS cscStatus = MH_CreateHook(reinterpret_cast<void**>(fnCreateSwapChain), &hkCreateSwapChain, reinterpret_cast<void**>(&oCreateSwapChain));
            static MH_STATUS cschStatus = MH_CreateHook(reinterpret_cast<void**>(fnCreateSwapChainForHwndChain), &hkCreateSwapChainForHwnd, reinterpret_cast<void**>(&oCreateSwapChainForHwnd));
            static MH_STATUS csccwStatus = MH_CreateHook(reinterpret_cast<void**>(fnCreateSwapChainForCWindowChain), &hkCreateSwapChainForCoreWindow, reinterpret_cast<void**>(&oCreateSwapChainForCoreWindow));
            static MH_STATUS csccStatus = MH_CreateHook(reinterpret_cast<void**>(fnCreateSwapChainForCompChain), &hkCreateSwapChainForComposition, reinterpret_cast<void**>(&oCreateSwapChainForComposition));

            static MH_STATUS presentStatus = MH_CreateHook(reinterpret_cast<void**>(fnPresent), &hkPresent, reinterpret_cast<void**>(&oPresent));
            static MH_STATUS present1Status = MH_CreateHook(reinterpret_cast<void**>(fnPresent1), &hkPresent1, reinterpret_cast<void**>(&oPresent1));

            static MH_STATUS resizeStatus = MH_CreateHook(reinterpret_cast<void**>(fnResizeBuffers), &hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers));
            static MH_STATUS resize1Status = MH_CreateHook(reinterpret_cast<void**>(fnResizeBuffers1), &hkResizeBuffers1, reinterpret_cast<void**>(&oResizeBuffers1));

            static MH_STATUS eclStatus = MH_CreateHook(reinterpret_cast<void**>(fnExecuteCommandLists), &hkExecuteCommandLists, reinterpret_cast<void**>(&oExecuteCommandLists));

            MH_EnableHook(fnCreateSwapChain);
            MH_EnableHook(fnCreateSwapChainForHwndChain);
            MH_EnableHook(fnCreateSwapChainForCWindowChain);
            MH_EnableHook(fnCreateSwapChainForCompChain);

            MH_EnableHook(fnPresent);
            MH_EnableHook(fnPresent1);

            MH_EnableHook(fnResizeBuffers);
            MH_EnableHook(fnResizeBuffers1);

            MH_EnableHook(fnExecuteCommandLists);
        }
    }

    void Unhook( ) {
        if (ImGui::GetCurrentContext( )) {
            if (ImGui::GetIO( ).BackendRendererUserData)
                ImGui_ImplDX12_Shutdown( );

            if (ImGui::GetIO( ).BackendPlatformUserData)
                ImGui_ImplWin32_Shutdown( );

            ImGui::DestroyContext( );
        }

        CleanupDeviceD3D12( );
    }
} // namespace DX12

static void CleanupRenderTarget( ) {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i)
        if (g_mainRenderTargetResource[i]) {
            g_mainRenderTargetResource[i]->Release( );
            g_mainRenderTargetResource[i] = NULL;
        }
}

static void CleanupDeviceD3D12( ) {
    CleanupRenderTarget( );

    if (g_pSwapChain) {
        g_pSwapChain->Release( );
        g_pSwapChain = NULL;
    }
    for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i)
        if (g_commandAllocators[i]) {
            g_commandAllocators[i]->Release( );
            g_commandAllocators[i] = NULL;
        }
    if (g_pd3dCommandList) {
        g_pd3dCommandList->Release( );
        g_pd3dCommandList = NULL;
    }
    if (g_pd3dRtvDescHeap) {
        g_pd3dRtvDescHeap->Release( );
        g_pd3dRtvDescHeap = NULL;
    }
    if (g_pd3dSrvDescHeap) {
        g_pd3dSrvDescHeap->Release( );
        g_pd3dSrvDescHeap = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release( );
        g_pd3dDevice = NULL;
    }
    if (g_dxgiFactory) {
        g_dxgiFactory->Release( );
        g_dxgiFactory = NULL;
    }
}

static void RenderImGui_DX12(IDXGISwapChain3* pSwapChain) {
    if (!ImGui::GetIO( ).BackendRendererUserData) {
        if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_pd3dDevice)))) {
            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = { };
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                desc.NumDescriptors = NUM_BACK_BUFFERS;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                desc.NodeMask = 1;
                if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
                    return;

                SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart( );
                for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i) {
                    g_mainRenderTargetDescriptor[i] = rtvHandle;
                    rtvHandle.ptr += rtvDescriptorSize;
                }
            }

            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = { };
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                desc.NumDescriptors = 1;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
                    return;
            }

            for (UINT i = 0; i < NUM_BACK_BUFFERS; ++i)
                if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocators[i])) != S_OK)
                    return;

            if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocators[0], NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
                g_pd3dCommandList->Close( ) != S_OK)
                return;

            ImGui_ImplDX12_Init(g_pd3dDevice, NUM_BACK_BUFFERS,
                                DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
                                g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart( ),
                                g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart( ));
        }
    }

    if (!H::bShuttingDown) {
        if (!g_mainRenderTargetResource[0]) {
            CreateRenderTarget(pSwapChain);
        }

        if (ImGui::GetCurrentContext( ) && g_pd3dCommandQueue && g_mainRenderTargetResource[0]) {
            ImGui_ImplDX12_NewFrame( );
            ImGui_ImplWin32_NewFrame( );
            ImGui::NewFrame( );

            Menu::Render( );

            ImGui::Render( );

            UINT backBufferIdx = pSwapChain->GetCurrentBackBufferIndex( );
            ID3D12CommandAllocator* commandAllocator = g_commandAllocators[backBufferIdx];
            commandAllocator->Reset( );

            D3D12_RESOURCE_BARRIER barrier = { };
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_pd3dCommandList->Reset(commandAllocator, NULL);
            g_pd3dCommandList->ResourceBarrier(1, &barrier);

            g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
            g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData( ), g_pd3dCommandList);
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);
            g_pd3dCommandList->Close( );

            g_pd3dCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&g_pd3dCommandList));
        }
    }
}
#else
#include <Windows.h>
namespace DX12 {
    void Hook(HWND hwnd) { LOG("[!] DirectX12 backend is not enabled!\n"); }
    void Unhook( ) { }
} // namespace DX12
#endif
