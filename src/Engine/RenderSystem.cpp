#include "stdafx.h"
#include "RenderSystem.h"
#include "RHIBackend.h"
//=============================================================================
RenderSystem::~RenderSystem()
{
}
//=============================================================================
bool RenderSystem::Create(const WindowData& wndData, const RenderSystemCreateInfo& createInfo)
{
	if (!RHIBackend::CreateAPI(wndData, createInfo)) return false;

	return true;
}
//=============================================================================
void RenderSystem::Destroy()
{
	RHIBackend::DestroyAPI();
}
//=============================================================================
void RenderSystem::Resize(uint32_t width, uint32_t height)
{
	RHIBackend::ResizeFrameBuffer(width, height);
}
//=============================================================================
void RenderSystem::Present()
{
	RHIBackend::Present();
}
//=============================================================================