#pragma once
#include <Windows.h>

namespace Hooks {
	void Init( );
	void Free( );

	inline bool bShuttingDown;
}

namespace H = Hooks;
