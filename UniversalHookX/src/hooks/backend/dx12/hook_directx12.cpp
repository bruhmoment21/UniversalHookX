#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <memory>
#include <mutex>

#include "hook_directx12.hpp"

#include "../../../dependencies/imgui/imgui_impl_dx12.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../../console/console.hpp"
#include "../../hooks.hpp"

// Data
static int const                    NUM_BACK_BUFFERS = 3;
static ID3D12Device*                g_pd3dDevice = NULL;
static ID3D12DescriptorHeap*        g_pd3dRtvDescHeap = NULL;
static ID3D12DescriptorHeap*        g_pd3dSrvDescHeap = NULL;
static ID3D12CommandQueue*          g_pd3dCommandQueue = NULL;
static ID3D12GraphicsCommandList*   g_pd3dCommandList = NULL;
static IDXGISwapChain3*             g_pSwapChain = NULL;
static ID3D12CommandAllocator*      g_commandAllocators[NUM_BACK_BUFFERS] = {};
static ID3D12Resource*              g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

static void CleanupDeviceD3D12( );
static void CleanupRenderTarget( );

static bool CreateDeviceD3D12(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = NUM_BACK_BUFFERS;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.Stereo = FALSE;

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
        return false;

    IDXGIFactory4* dxgiFactory = NULL;
    IDXGISwapChain1* swapChain1 = NULL;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
        return false;
    if (dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
        return false;
    if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
        return false;
    swapChain1->Release( );
    dxgiFactory->Release( );

    return true;
}

static void CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        ID3D12Resource* pBackBuffer = NULL;
        pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));

        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain3*, UINT, UINT)> oPresent;
static HRESULT WINAPI hkPresent(IDXGISwapChain3* pSwapChain,
                                UINT SyncInterval,
                                UINT Flags) {
    static std::once_flag once;
    std::call_once(once, [ pSwapChain ]( ) {
        if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_pd3dDevice)))) {
            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                desc.NumDescriptors = NUM_BACK_BUFFERS;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                desc.NodeMask = 1;
                if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
                    return;

                SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart( );
                for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
                    g_mainRenderTargetDescriptor[i] = rtvHandle;
                    rtvHandle.ptr += rtvDescriptorSize;
                }
            }

            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                desc.NumDescriptors = 1;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
                    return;
            }

            for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
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
    });

    if (!H::bShuttingDown) {
        if (!g_mainRenderTargetResource[0]) {
            CreateRenderTarget(pSwapChain);
        }

        if (ImGui::GetCurrentContext( ) && g_pd3dCommandQueue && g_mainRenderTargetResource[0]) {
            ImGui_ImplDX12_NewFrame( );
            ImGui_ImplWin32_NewFrame( );
            ImGui::NewFrame( );

            if (H::bShowDemoWindow) {
                ImGui::ShowDemoWindow( );
            }

            ImGui::Render( );

            UINT backBufferIdx = pSwapChain->GetCurrentBackBufferIndex( );
            ID3D12CommandAllocator* commandAllocator = g_commandAllocators[backBufferIdx];
            commandAllocator->Reset( );

            D3D12_RESOURCE_BARRIER barrier = {};
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

    return oPresent(pSwapChain, SyncInterval, Flags);
}

static std::add_pointer_t<HRESULT WINAPI(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT)> oResizeBuffers;
static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* pSwapChain,
                                      UINT        BufferCount,
                                      UINT        Width,
                                      UINT        Height,
                                      DXGI_FORMAT NewFormat,
                                      UINT        SwapChainFlags) {
    CleanupRenderTarget( );

    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
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

namespace DX12 {
    void Hook(HWND hwnd) {
        if (!CreateDeviceD3D12(GetConsoleWindow( ))) {
            return;
        }

        LOG("[+] DirectX12: g_pd3dDevice: 0x%p\n", g_pd3dDevice);
        LOG("[+] DirectX12: g_pd3dCommandQueue: 0x%p\n", g_pd3dCommandQueue);
        LOG("[+] DirectX12: g_pSwapChain: 0x%p\n", g_pSwapChain);

        if (g_pd3dDevice) {
            // Init ImGui
            ImGui::CreateContext( );
            ImGui_ImplWin32_Init(hwnd);

            ImGuiIO& io = ImGui::GetIO( );

            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            // Hook
            void** pVTable = *reinterpret_cast<void***>(g_pSwapChain);
            void** pCommandQueueVTable = *reinterpret_cast<void***>(g_pd3dCommandQueue);

            void* fnPresent = pVTable[8];
            void* fnResizeBuffer = pVTable[13];
            void* fnExecuteCommandLists = pCommandQueueVTable[10];

            if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release( ); g_pd3dCommandQueue = NULL; }
            CleanupDeviceD3D12( );

            static MH_STATUS presentStatus = MH_CreateHook(reinterpret_cast<void**>(fnPresent), &hkPresent, reinterpret_cast<void**>(&oPresent));
            static MH_STATUS resizeStatus = MH_CreateHook(reinterpret_cast<void**>(fnResizeBuffer), &hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers));
            static MH_STATUS eclStatus = MH_CreateHook(reinterpret_cast<void**>(fnExecuteCommandLists), &hkExecuteCommandLists, reinterpret_cast<void**>(&oExecuteCommandLists));

            MH_EnableHook(fnPresent);
            MH_EnableHook(fnResizeBuffer);
            MH_EnableHook(fnExecuteCommandLists);
        }
    }

    void Unhook( ) {
        if (ImGui::GetCurrentContext( )) {
            if (ImGui::GetIO( ).BackendRendererUserData)
                ImGui_ImplDX12_Shutdown( );

            ImGui_ImplWin32_Shutdown( );
            ImGui::DestroyContext( );
        }

        CleanupDeviceD3D12( );
    }
}

static void CleanupRenderTarget( ) {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release( ); g_mainRenderTargetResource[i] = NULL; }
}

static void CleanupDeviceD3D12( ) {
    CleanupRenderTarget( );

    if (g_pSwapChain) { g_pSwapChain->Release( ); g_pSwapChain = NULL; }
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_commandAllocators[i]) { g_commandAllocators[i]->Release(); g_commandAllocators[i] = NULL; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release( ); g_pd3dCommandList = NULL; }
    if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release( ); g_pd3dRtvDescHeap = NULL; }
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release( ); g_pd3dSrvDescHeap = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release( ); g_pd3dDevice = NULL; }
}
