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
	void SetRenderingBackend(RenderingBackend_t eRenderingBackend);
	RenderingBackend_t GetRenderingBackend( );
	const char* RenderingBackendToStr( );

	HWND GetProcessWindow( );
	void UnloadDLL( );
	
	HMODULE GetCurrentImageBase( );
}

namespace U = Utils;
