#include <Windows.h>
#include <iostream>
#include <thread>

#include "console/console.hpp"

#include "hooks/hooks.hpp"
#include "utils/utils.hpp"

#include "dependencies/minhook/MinHook.h"

DWORD WINAPI OnProcessAttach(LPVOID lpParam);
DWORD WINAPI OnProcessDetach(LPVOID lpParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        U::SetRenderingBackend(DIRECTX12);

        HANDLE hHandle = CreateThread(NULL, 0, OnProcessAttach, hinstDLL, 0, NULL);
        if (hHandle != NULL) {
            CloseHandle(hHandle);
        }
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        OnProcessDetach(NULL);
    }

    return TRUE;
}

DWORD WINAPI OnProcessAttach(LPVOID lpParam) {
    MH_Initialize( );

    Console::Alloc( );
    LOG("[+] Rendering backend: %s\n", U::RenderingBackendToStr( ));
    if (U::GetRenderingBackend( ) == NONE) {
        LOG("[!] Looks like you forgot to set a backend. Will unload after pressing enter...");
        std::cin.get( );

        FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(lpParam), 0);
        return 0;
    }

    H::Init( );

    return 0;
}

DWORD WINAPI OnProcessDetach(LPVOID lpParam) {
    // If the process quits leave memory management to the OS.
    // H::bShuttingDown == true means we pressed end an must free them by ourself.
    if (H::bShuttingDown) {
        H::Free( );
        MH_Uninitialize( );
    }

    Console::Free( );

    return 0;
}
