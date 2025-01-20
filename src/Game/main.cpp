#include "stdafx.h"
#if EXAMPLE
#	include "000_Render_Clear.h"
#	include "001_Render_Triangle.h"
#	include "002_Render_TextureTriangle.h"
#	include "003_Render_TriangleBundles.h"
#	include "004_Render_TriangleCB.h"
#	include "005_Render_Cube.h"

#	include "xxx_Render_Test.h"
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
	//ExampleRender000();
	//ExampleRender001();
	//ExampleRender002();
	//ExampleRender003();
	//ExampleRender004();
	//ExampleRender005();

	ExampleRenderXXX();
#else
	extern void GameApp();
	GameApp();
#endif
}
//=============================================================================