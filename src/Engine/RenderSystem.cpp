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
	if (!gRHI.CreateAPI(wndData, createInfo)) return false;

	return true;
}
//=============================================================================
void RenderSystem::Destroy()
{
	WaitForIdle();
	gRHI.DestroyAPI();
}
//=============================================================================
void RenderSystem::Resize(uint32_t width, uint32_t height)
{
	gRHI.ResizeFrameBuffer(width, height);
}
//=============================================================================
void RenderSystem::BeginFrame()
{
	gRHI.BeginFrame();
}
//=============================================================================
void RenderSystem::EndFrame()
{
	gRHI.EndFrame();
}
//=============================================================================
void RenderSystem::Present()
{
	gRHI.Present();
}
//=============================================================================
glm::ivec2 RenderSystem::GetFrameBufferSize() const
{
	return { gRHI.frameBufferWidth, gRHI.frameBufferHeight }; // TODO: temp
}
//=============================================================================