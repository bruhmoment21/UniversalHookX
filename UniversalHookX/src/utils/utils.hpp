#pragma once

enum RenderingBackend_t {
	NONE = 0,

	DIRECTX9,
	DIRECTX10,
	DIRECTX11,
	DIRECTX12,

	OPENGL,
};

namespace Utils {
	void SetRenderingBackground(RenderingBackend_t eRenderingBackground);
	RenderingBackend_t GetRenderingBackend( );
	const char* RenderingBackendToStr( );

	HWND GetCurrentProcessHWND( );
	void UnloadDLL( );
	
	HMODULE GetCurrentImageBase( );
}

namespace U = Utils;
