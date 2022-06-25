#include "../../../backend.hpp"
#include "../../../console/console.hpp"

#ifdef BACKEND_ENABLE_OPENGL
#include <Windows.h>

#include <memory>

#include "hook_opengl.hpp"

#include "../../../dependencies/imgui/imgui_impl_opengl3.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../hooks.hpp"

static std::add_pointer_t<BOOL WINAPI(HDC)> oWglSwapBuffers;
static BOOL WINAPI hkWglSwapBuffers(HDC Hdc) {
	if (!H::bShuttingDown && ImGui::GetCurrentContext( )) {
		if (!ImGui::GetIO( ).BackendRendererUserData)
			ImGui_ImplOpenGL3_Init( );
		
		ImGui_ImplOpenGL3_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		if (H::bShowDemoWindow) {
			ImGui::ShowDemoWindow( );
		}

		ImGui::Render( );
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData( ));
	}

	return oWglSwapBuffers(Hdc);
}

namespace GL {
	void Hook(HWND hwnd) {
		HMODULE openGL32 = GetModuleHandleA("opengl32.dll");
		if (openGL32) {
			LOG("[+] OpenGL32: ImageBase: 0x%p\n", openGL32);

			void* fnWglSwapBuffers = reinterpret_cast<void*>(GetProcAddress(openGL32, "wglSwapBuffers"));
			if (fnWglSwapBuffers) {
				// Init ImGui
				ImGui::CreateContext( );
				ImGui_ImplWin32_Init(hwnd);

				ImGuiIO& io = ImGui::GetIO( );

				io.IniFilename = nullptr;
				io.LogFilename = nullptr;

				// Hook
				LOG("[+] OpenGL32: fnWglSwapBuffers: 0x%p\n", fnWglSwapBuffers);

				static MH_STATUS wsbStatus = MH_CreateHook(reinterpret_cast<void**>(fnWglSwapBuffers), &hkWglSwapBuffers, reinterpret_cast<void**>(&oWglSwapBuffers));
				
				MH_EnableHook(fnWglSwapBuffers);
			}
		}
	}

	void Unhook( ) {
		if (ImGui::GetCurrentContext( )) {
			if (ImGui::GetIO( ).BackendRendererUserData)
				ImGui_ImplOpenGL3_Shutdown( );

			ImGui_ImplWin32_Shutdown( );
			ImGui::DestroyContext( );
		}
	}
}
#else
#include <Windows.h>
namespace GL {
	void Hook(HWND hwnd) { LOG("[!] OpenGL backend is not enabled!\n"); }
	void Unhook( ) { }
}
#endif
