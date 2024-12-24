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

	m_graphicsContext = CreateGraphicsContext();

	return true;
}
//=============================================================================
void RenderSystem::Destroy()
{
	WaitForIdle();

	m_graphicsContext.reset();
	RHIBackend::DestroyAPI();
}
//=============================================================================
void RenderSystem::Resize(uint32_t width, uint32_t height)
{
	RHIBackend::ResizeFrameBuffer(width, height);
}
//=============================================================================
void RenderSystem::BeginFrame()
{
	RHIBackend::BeginFrame();
}
//=============================================================================
void RenderSystem::EndFrame()
{
	RHIBackend::EndFrame();
}
//=============================================================================
void RenderSystem::Present()
{
	RHIBackend::Present();
}
//=============================================================================
glm::ivec2 RenderSystem::GetFrameBufferSize() const
{
	return { gRenderContext.frameBufferWidth, gRenderContext.frameBufferHeight }; // TODO: temp
}
//=============================================================================