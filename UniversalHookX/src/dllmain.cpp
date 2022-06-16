#include <Windows.h>
#include <cstdio>
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
		
		U::SetRenderingBackground(DIRECTX12);

		HANDLE hHandle = CreateThread(NULL, 0, OnProcessAttach, hinstDLL, 0, NULL);
		if (hHandle != NULL) CloseHandle(hHandle);
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
		LOG("[!] Will unload in 2 seconds...\nMake sure U::SetRenderingBackground( ) is called.\n");
		std::this_thread::sleep_for(std::chrono::seconds(2));
		FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(lpParam), 0);
		return 0;
	}

	H::Init( );

    return 0;
}

DWORD WINAPI OnProcessDetach(LPVOID lpParam) {
	H::Free( );
	Console::Free( );

	MH_Uninitialize( );

	return 0;
}
