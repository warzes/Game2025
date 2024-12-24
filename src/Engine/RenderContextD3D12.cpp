#include "stdafx.h"
#if RENDER_D3D12
#include "RenderContextD3D12.h"
#include "RenderCore.h"
//=============================================================================
RenderContext::~RenderContext()
{
	assert(!device);
}
//=============================================================================
void RenderContext::Release()
{
	SafeRelease(DXGIFactory);
}
//=============================================================================
#endif // RENDER_D3D12