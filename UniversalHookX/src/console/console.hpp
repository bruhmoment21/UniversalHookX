#pragma once
#include <cstdio>

//#define DISABLE_LOGGING_CONSOLE

#ifndef DISABLE_LOGGING_CONSOLE
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

namespace Console {
	void Alloc( );
	void Free( );
}
