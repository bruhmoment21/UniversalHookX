#include "hooks.hpp"

#include "backend/dx9/hook_directx9.hpp"
#include "backend/dx10/hook_directx10.hpp"
#include "backend/dx11/hook_directx11.hpp"
#include "backend/dx12/hook_directx12.hpp"

#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"
#include "../console/console.hpp"

static HWND g_hWindow = NULL;

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_KEYDOWN) {
		if (wParam == VK_INSERT) {
			H::bShowDemoWindow = !H::bShowDemoWindow;
		} else if (wParam == VK_END) {
			H::bShuttingDown = true;
			U::UnloadDLL( );
			return 0;
		}
	}

	LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (H::bShowDemoWindow) {
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

		// (Doesn't work for some games like 'Sid Meier's Civilization VI')
		// Window may not maximize from taskbar because 'H::bShowDemoWindow' is set to true by default. ('hooks.hpp')
		// 
		// return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) == 0;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

namespace Hooks {
	void Init( ) {
		g_hWindow = U::GetCurrentProcessHWND( );

#ifdef DISABLE_LOGGING_CONSOLE
		bool bNoConsole = GetConsoleWindow() == NULL;
		if (bNoConsole) {
			AllocConsole( );
		}
#endif

		RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
		switch (eRenderingBackend) {
			case DIRECTX9: DX9::Hook(g_hWindow); break;
			case DIRECTX10: DX10::Hook(g_hWindow); break;
			case DIRECTX11: DX11::Hook(g_hWindow); break;
			case DIRECTX12: DX12::Hook(g_hWindow); break;
		}

#ifdef DISABLE_LOGGING_CONSOLE
		if (bNoConsole) {
			FreeConsole( );
		}
#endif

		oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
	}

	void Free( ) {
		MH_DisableHook(MH_ALL_HOOKS);

		RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
		switch (eRenderingBackend) {
			case DIRECTX9: DX9::Unhook( ); break;
			case DIRECTX10: DX10::Unhook( ); break;
			case DIRECTX11: DX11::Unhook( ); break;
			case DIRECTX12: DX12::Unhook( ); break;
		}

		if (oWndProc) {
			SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
		}
	}
}
