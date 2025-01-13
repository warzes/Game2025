#include "stdafx.h"
#include "GPUMarker.h"
//=============================================================================
ScopedMarker::ScopedMarker(const char* pLabel, unsigned PIXColor)
{
	PIXBeginEvent(PIXColor, pLabel);
}
//=============================================================================
ScopedMarker::~ScopedMarker()
{
	PIXEndEvent();
}
//=============================================================================
// https://devblogs.microsoft.com/pix/winpixeventruntime/#:~:text=An%20%E2%80%9Cevent%E2%80%9D%20represents%20a%20region,a%20single%20point%20in%20time.
// https://devblogs.microsoft.com/pix/pix-2008-26-new-capture-layer/
ScopedGPUMarker::ScopedGPUMarker(ID3D12GraphicsCommandList* pCmdList, const char* pLabel, unsigned PIXColor)
	: mpCmdList(pCmdList)
{
	PIXBeginEvent(mpCmdList, (unsigned long long)PIXColor, pLabel);
}
//=============================================================================
ScopedGPUMarker::ScopedGPUMarker(ID3D12CommandQueue* pCmdQueue, const char* pLabel, unsigned PIXColor)
	:mpCmdQueue(pCmdQueue)
{
	PIXBeginEvent(mpCmdQueue, PIXColor, pLabel);
}
//=============================================================================
ScopedGPUMarker::~ScopedGPUMarker()
{
	PIXEndEvent(mpCmdList);
}
//=============================================================================