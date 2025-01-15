#include "stdafx.h"
#include "RenderSystem.h"
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
glm::ivec2 RenderSystem::GetFrameBufferSize() const
{
	return { gRHI.GetFrameBufferWidth(), gRHI.GetFrameBufferHeight()};
}
//=============================================================================