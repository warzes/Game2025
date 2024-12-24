#include "stdafx.h"
#if EXAMPLE
#	include "000_Render_Triangle.h"
#endif
//=============================================================================
#if defined(_MSC_VER)
#	pragma comment( lib, "Engine.lib" )
#	pragma comment( lib, "3rdparty.lib" )
#endif
//=============================================================================
int main(
	[[maybe_unused]] int   argc,
	[[maybe_unused]] char* argv[])
{
#if EXAMPLE_RUN
	ExampleRender000();
#else
	extern void GameApp();
	GameApp();
#endif
}
//=============================================================================